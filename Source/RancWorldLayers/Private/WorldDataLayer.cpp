#include "WorldDataLayer.h"
#include "Spatial/Quadtree.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"

void UWorldDataLayer::Initialize(UWorldDataLayerAsset* InConfig, const FVector2D& InWorldGridSize)
{
	Config = TObjectPtr<UWorldDataLayerAsset>(InConfig);

	if (Config->ResolutionMode == EResolutionMode::Absolute)
	{
		Resolution = Config->Resolution;
	} else // RelativeToWorld
	{
		Resolution.X = FMath::RoundToInt(InWorldGridSize.X / Config->CellSize.X);
		Resolution.Y = FMath::RoundToInt(InWorldGridSize.Y / Config->CellSize.Y);
	}

	int32 BytesPerPixel = GetBytesPerPixel();
	RawData.SetNumZeroed(Resolution.X * Resolution.Y * BytesPerPixel);

	// 1. Initialize with DefaultValue
	for (int32 Y = 0; Y < Resolution.Y; ++Y)
	{
		for (int32 X = 0; X < Resolution.X; ++X)
		{
			SetValueAtPixel(FIntPoint(X, Y), Config->DefaultValue);
		}
	}

	// 2. Override with InitialDataTexture if provided
	if (UTexture2D* Texture = Config->InitialDataTexture.LoadSynchronous())
	{
		const int32 TexWidth = Texture->GetSizeX();
		const int32 TexHeight = Texture->GetSizeY();
		
		UE_LOG(LogTemp, Log, TEXT("WorldDataLayer: Initializing '%s' from texture '%s' (%dx%d). Format: %d"), 
			*Config->LayerName.ToString(), *Texture->GetName(), TexWidth, TexHeight, (int32)Texture->GetPixelFormat());

		// Ensure we can read the data. 
		// Note: Requires texture to be Uncompressed (RGBA8) or similar.
		const FColor* FormattedData = reinterpret_cast<const FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.LockReadOnly());
		if (FormattedData)
		{
			for (int32 Y = 0; Y < Resolution.Y; ++Y)
			{
				for (int32 X = 0; X < Resolution.X; ++X)
				{
					int32 TexX = FMath::Clamp(FMath::RoundToInt((float)X / (float)Resolution.X * (float)TexWidth), 0, TexWidth - 1);
					int32 TexY = FMath::Clamp(FMath::RoundToInt((float)Y / (float)Resolution.Y * (float)TexHeight), 0, TexHeight - 1);
					
					FColor TexColor = FormattedData[TexY * TexWidth + TexX];
					SetValueAtPixel(FIntPoint(X, Y), FLinearColor::FromSRGBColor(TexColor));
				}
			}
			Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
			UE_LOG(LogTemp, Log, TEXT("WorldDataLayer: Successfully populated '%s' from texture."), *Config->LayerName.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("WorldDataLayer: Failed to lock texture data for '%s'. Ensure texture Compression is set to UserInterface2D or VectorDisplacementMap."), *Config->LayerName.ToString());
		}
	}

	bIsDirty = false;
	LastReadbackTime = 0.0f;

	// Initialize Spatial Index if configured
	if (Config->SpatialOptimization.bBuildAccelerationStructure)
	{
		SpatialIndices.Empty();
		for (const FLinearColor& ValueToTrack : Config->SpatialOptimization.ValuesToTrack)
		{
			SpatialIndices.Emplace(ValueToTrack, MakeShared<FQuadtree>(FBox2D(FVector2D(0, 0), FVector2D(Resolution.X, Resolution.Y))));
		}

		// Populate initial data
		for (int32 Y = 0; Y < Resolution.Y; ++Y)
		{
			for (int32 X = 0; X < Resolution.X; ++X)
			{
				FIntPoint PixelCoords(X, Y);
				FLinearColor PixelValue = GetValueAtPixel(PixelCoords);
				for (const auto& Elem : SpatialIndices)
				{
					if (PixelValue.Equals(Elem.Key, KINDA_SMALL_NUMBER))
					{
						Elem.Value->Insert(PixelCoords);
						break;
					}
				}
			}
		}
	}
}

FLinearColor UWorldDataLayer::GetValueAtPixel(const FIntPoint& PixelCoords) const
{
	if (!RawData.IsValidIndex(GetPixelIndex(PixelCoords)))
	{
		return FLinearColor::Black; // Or some other default/error value
	}

	int32 PixelIndex = GetPixelIndex(PixelCoords);
	int32 BytesPerPixel = GetBytesPerPixel();

	switch (Config->DataFormat)
	{
		case EDataFormat::R8:
			return FLinearColor(RawData[PixelIndex] / 255.0f, 0.0f, 0.0f, 0.0f);
		case EDataFormat::R16F:
			return FLinearColor(*((FFloat16*)&RawData[PixelIndex]), 0.0f, 0.0f, 0.0f);
		case EDataFormat::RGBA8:
			return FLinearColor(
				RawData[PixelIndex] / 255.0f,
				RawData[PixelIndex + 1] / 255.0f,
				RawData[PixelIndex + 2] / 255.0f,
				RawData[PixelIndex + 3] / 255.0f
			);
		case EDataFormat::RGBA16F:
			return FLinearColor(
				*((FFloat16*)&RawData[PixelIndex]),
				*((FFloat16*)&RawData[PixelIndex + 2]),
				*((FFloat16*)&RawData[PixelIndex + 4]),
				*((FFloat16*)&RawData[PixelIndex + 6])
			);
		default:
			return FLinearColor::Black;
	}
}

// Source/RancWorldLayers/Private/WorldDataLayer.cpp

// Source/RancWorldLayers/Private/WorldDataLayer.cpp

void UWorldDataLayer::SetValueAtPixel(const FIntPoint& PixelCoords, const FLinearColor& NewValue)
{
	if (!RawData.IsValidIndex(GetPixelIndex(PixelCoords)))
	{
		return; // Out of bounds
	}

	const bool bShouldUpdateIndex = Config->SpatialOptimization.bBuildAccelerationStructure && !SpatialIndices.IsEmpty();
	FLinearColor OldValue = FLinearColor::Black;

	if (bShouldUpdateIndex)
	{
		OldValue = GetValueAtPixel(PixelCoords);
		// A simple check is not sufficient due to data format quantization. We must proceed.
	}

	// --- Modify the Raw Data ---
	int32 PixelIndex = GetPixelIndex(PixelCoords);
	switch (Config->DataFormat)
	{
		case EDataFormat::R8:
			RawData[PixelIndex] = FMath::RoundToInt(NewValue.R * 255.0f);
			break;
		case EDataFormat::R16F:
			*((FFloat16*)&RawData[PixelIndex]) = FFloat16(NewValue.R);
			break;
		case EDataFormat::RGBA8:
			RawData[PixelIndex] = FMath::RoundToInt(NewValue.R * 255.0f);
			RawData[PixelIndex + 1] = FMath::RoundToInt(NewValue.G * 255.0f);
			RawData[PixelIndex + 2] = FMath::RoundToInt(NewValue.B * 255.0f);
			RawData[PixelIndex + 3] = FMath::RoundToInt(NewValue.A * 255.0f);
			break;
		case EDataFormat::RGBA16F:
			*((FFloat16*)&RawData[PixelIndex]) = FFloat16(NewValue.R);
			*((FFloat16*)&RawData[PixelIndex + 2]) = FFloat16(NewValue.G);
			*((FFloat16*)&RawData[PixelIndex + 4]) = FFloat16(NewValue.B);
			*((FFloat16*)&RawData[PixelIndex + 6]) = FFloat16(NewValue.A);
			break;
	}

	bIsDirty = true;

	// --- Update Spatial Indices with Format-Aware Comparison ---
	if (bShouldUpdateIndex)
	{
		// Remove the point from the quadtree for the OLD value.
		for (const auto& Elem : SpatialIndices)
		{
			bool bMatches = false;
			switch(Config->DataFormat)
			{
				case EDataFormat::R8:
				case EDataFormat::R16F:
					// For single-channel formats, compare only the R channel.
					bMatches = FMath::IsNearlyEqual(OldValue.R, Elem.Key.R);
					break;
				default:
					// For multi-channel formats, the read-back OldValue is canonical.
					bMatches = OldValue.Equals(Elem.Key, KINDA_SMALL_NUMBER);
					break;
			}
			if (bMatches)
			{
				Elem.Value->Remove(PixelCoords);
				break;
			}
		}

		// Add the point to the quadtree for the NEW value.
		for (const auto& Elem : SpatialIndices)
		{
			bool bMatches = false;
			switch(Config->DataFormat)
			{
				case EDataFormat::R8:
				case EDataFormat::R16F:
					// Compare the R channel of the incoming value with the tracked key's R channel.
					bMatches = FMath::IsNearlyEqual(NewValue.R, Elem.Key.R);
					break;
				default:
					bMatches = NewValue.Equals(Elem.Key, KINDA_SMALL_NUMBER);
					break;
			}
			if (bMatches)
			{
				Elem.Value->Insert(PixelCoords);
				break;
			}
		}
	}
}

int32 UWorldDataLayer::GetPixelIndex(const FIntPoint& PixelCoords) const
{
	return (PixelCoords.Y * Resolution.X + PixelCoords.X) * GetBytesPerPixel();
}

int32 UWorldDataLayer::GetBytesPerPixel() const
{
	switch (Config->DataFormat)
	{
		case EDataFormat::R8:
			return 1;
		case EDataFormat::R16F:
			return 2; // FFloat16 is 2 bytes
		case EDataFormat::RGBA8:
			return 4;
		case EDataFormat::RGBA16F:
			return 8; // 4 FFloat16s = 8 bytes
		default:
			return 0; // Should not happen
	}
}