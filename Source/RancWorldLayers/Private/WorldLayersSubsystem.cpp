#include "WorldLayersSubsystem.h"
#include "Rendering/Texture2DResource.h"
#include "Engine/TextureRenderTarget2D.h"

#include "ImageUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "WorldDataLayerAsset.h"

void UWorldLayersSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Discover all UWorldDataLayerAsset assets
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetData;
	AssetRegistryModule.Get().GetAssetsByClass(UWorldDataLayerAsset::StaticClass()->GetClassPathName(), AssetData);

	for (const FAssetData& Data : AssetData)
	{
		UWorldDataLayerAsset* LayerAsset = Cast<UWorldDataLayerAsset>(Data.GetAsset());
		RegisterDataLayer(LayerAsset);
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UWorldLayersSubsystem::Tick));
}

void UWorldLayersSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	WorldDataLayers.Empty();
	Super::Deinitialize();
}

bool UWorldLayersSubsystem::Tick(float DeltaTime)
{
	for (auto& Elem : WorldDataLayers)
	{
		UWorldDataLayer* Layer = Elem.Value;
		if (!Layer) continue;

		// Handle CPU to GPU sync
		if (Layer->bIsDirty && Layer->GpuRepresentation)
		{
			SyncCPUToGPU(Layer);
			Layer->bIsDirty = false;
		}

		// Handle GPU to CPU readback
		if (Layer->Config->GPUConfiguration.bIsGPUWritable && Layer->GpuRepresentation)
		{
			const EWorldDataLayerReadbackBehavior ReadbackBehavior = Layer->Config->GPUConfiguration.ReadbackBehavior;
			const float PeriodicReadbackSeconds = Layer->Config->GPUConfiguration.PeriodicReadbackSeconds;

			if (ReadbackBehavior == EWorldDataLayerReadbackBehavior::Periodic)
			{
				if (GetWorld()->GetTimeSeconds() - Layer->LastReadbackTime >= PeriodicReadbackSeconds)
				{
					ReadbackTexture(Layer);
					Layer->LastReadbackTime = GetWorld()->GetTimeSeconds();
				}
			}
			else if (ReadbackBehavior == EWorldDataLayerReadbackBehavior::OnDemand)
			{
				// OnDemand readback would be triggered by a specific API call, not here.
				// For now, we can just ensure it doesn't continuously read back.
			}
		}
	}
	return true;
}

UWorldLayersSubsystem* UWorldLayersSubsystem::Get(UObject* WorldContext)
{
	if (!WorldContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("WorldContext is null in UMyWorldDataSubsystem::Get."));
		return nullptr;
	}

	auto* World = WorldContext->GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		if (!World->IsEditorWorld())
			UE_LOG(LogTemp, Warning, TEXT("GameInstance is null in UMyWorldDataSubsystem::Get."));
		return nullptr;
	}

	// Get the subsystem; if it doesn't exist, Unreal Engine will handle the creation automatically
	UWorldLayersSubsystem* Subsystem = GameInstance->GetSubsystem<UWorldLayersSubsystem>();

	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMyWorldDataSubsystem is not found, but it should have been automatically created."));
	}

	return Subsystem;
}



bool UWorldLayersSubsystem::GetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, FLinearColor& OutValue) const
{
	const UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName);
	if (DataLayer)
	{
		FIntPoint PixelCoords = WorldLocationToPixel(WorldLocation, DataLayer);
		OutValue = DataLayer->GetValueAtPixel(PixelCoords);
		return true;
	}
	OutValue = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f); // Initialize OutValue to (0,0,0,0) if layer not found
	return false;
}

float UWorldLayersSubsystem::GetFloatValueAtLocation(FName LayerName, const FVector2D& WorldLocation) const
{
	FLinearColor Value;
	if (GetValueAtLocation(LayerName, WorldLocation, Value))
	{
		return Value.R; // Assuming R channel holds the float value for single-channel layers
	}
	return 0.0f;
}

void UWorldLayersSubsystem::SetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, const FLinearColor& NewValue)
{
	UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName);
	if (DataLayer)
	{
		FIntPoint PixelCoords = WorldLocationToPixel(WorldLocation, DataLayer);
		DataLayer->SetValueAtPixel(PixelCoords, NewValue);
	}
}

void UWorldLayersSubsystem::RegisterDataLayer(UWorldDataLayerAsset* LayerAsset)
{
	if (LayerAsset)
	{
		UWorldDataLayer* NewDataLayer = NewObject<UWorldDataLayer>(this);
		NewDataLayer->Initialize(LayerAsset);
		WorldDataLayers.Add(LayerAsset->LayerName, NewDataLayer);

		if (LayerAsset->GPUConfiguration.bKeepUpdatedOnGPU)
		{
			EPixelFormat PixelFormat = PF_Unknown;
			switch (LayerAsset->DataFormat)
			{
				case EDataFormat::R8: PixelFormat = PF_G8; break;
				case EDataFormat::R16F: PixelFormat = PF_R16F; break;
				case EDataFormat::RGBA8: PixelFormat = PF_B8G8R8A8; break;
				case EDataFormat::RGBA16F: PixelFormat = PF_FloatRGBA; break;
			}

			if (PixelFormat != PF_Unknown)
			{
				if (LayerAsset->GPUConfiguration.bIsGPUWritable)
				{
					NewDataLayer->GpuRepresentation = NewObject<UTextureRenderTarget2D>(NewDataLayer);
					Cast<UTextureRenderTarget2D>(NewDataLayer->GpuRepresentation)->InitCustomFormat(NewDataLayer->Resolution.X, NewDataLayer->Resolution.Y, PixelFormat, false);
				}
				else
				{
					NewDataLayer->GpuRepresentation = UTexture2D::CreateTransient(NewDataLayer->Resolution.X, NewDataLayer->Resolution.Y, PixelFormat);
				}
				NewDataLayer->GpuRepresentation->UpdateResource();
			}
		}
	}
}

UTexture* UWorldLayersSubsystem::GetLayerGpuTexture(FName LayerName) const
{
	const UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName);
	if (DataLayer)
	{
		return DataLayer->GpuRepresentation;
	}
	return nullptr;
}

bool UWorldLayersSubsystem::FindNearestPointWithValue(FName LayerName, const FVector2D& SearchOrigin, float MaxSearchRadius, const FLinearColor& TargetValue, FVector2D& OutWorldLocation) const
{
	const UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName);
	if (!DataLayer || !DataLayer->Config->SpatialOptimization.bBuildAccelerationStructure || DataLayer->SpatialIndices.IsEmpty())
	{
		return false;
	}

	// Find the specific quadtree for the TargetValue.
	// We iterate and use Equals() to handle floating point inaccuracies, instead of a direct TMap::Find().
	TSharedPtr<FQuadtree> TargetQuadtree;
	for (const auto& Elem : DataLayer->SpatialIndices)
	{
		if (TargetValue.Equals(Elem.Key, KINDA_SMALL_NUMBER))
		{
			TargetQuadtree = Elem.Value;
			break;
		}
	}

	if (!TargetQuadtree)
	{
		// The requested value is not being tracked, so no quadtree exists for it.
		return false;
	}

	// Convert world search origin to pixel coordinates
	FIntPoint SearchPixel = WorldLocationToPixel(SearchOrigin, DataLayer);

	// Perform search in the correct, value-specific quadtree
	FIntPoint NearestPixel;
	if (TargetQuadtree->FindNearest(SearchPixel, MaxSearchRadius, NearestPixel))
	{
		OutWorldLocation = PixelToWorldLocation(NearestPixel, DataLayer);
		return true;
	}

	return false;
}

void UWorldLayersSubsystem::ReadbackTexture(UWorldDataLayer* DataLayer)
{
	if (!DataLayer || !DataLayer->GpuRepresentation)
	{
		return;
	}

	UTextureRenderTarget2D* RenderTarget = Cast<UTextureRenderTarget2D>(DataLayer->GpuRepresentation);
	if (!RenderTarget)
	{
		return;
	}

	FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (RenderTargetResource)
	{
		TArray<FColor> Pixels;
		// ReadPixels is a synchronous call that reads the GPU data back to the CPU.
		// It expects the pixel format to be FColor (BGRA8).
		RenderTargetResource->ReadPixels(Pixels);

		// Assuming RGBA8 for now, need to handle other formats based on DataLayer->Config->DataFormat
		// This part needs to be robust for different pixel formats.
		// For simplicity, we'll assume RGBA8 for now and copy directly.
		// A more complete solution would involve converting from FColor to the layer's native format.
		
		// Ensure RawData has enough space
		DataLayer->RawData.SetNum(Pixels.Num() * sizeof(FColor));
		FMemory::Memcpy(DataLayer->RawData.GetData(), Pixels.GetData(), Pixels.Num() * sizeof(FColor));

		DataLayer->bIsDirty = true;
	}
}

void UWorldLayersSubsystem::SyncCPUToGPU(UWorldDataLayer* DataLayer)
{
	if (!DataLayer || !DataLayer->GpuRepresentation)
	{
		return;
	}

	FTexture2DResource* TextureResource = (FTexture2DResource*)DataLayer->GpuRepresentation->GetResource();
	if (TextureResource)
	{
		ENQUEUE_RENDER_COMMAND(UpdateTextureRegionsData)(
			[TextureResource, RawData = DataLayer->RawData, Width = DataLayer->Resolution.X, Height = DataLayer->Resolution.Y](FRHICommandListImmediate& RHICmdList)
			{
				FUpdateTextureRegion2D UpdateRegion(0, 0, 0, 0, Width, Height);
				RHICmdList.UpdateTexture2D(TextureResource->GetTexture2DRHI(), 0, UpdateRegion, Width * (RawData.GetTypeSize() * 4), RawData.GetData());
			}
		);
	}
}

#include "Curves/CurveLinearColor.h"
#include "WorldDataLayer.h"

UTexture2D* UWorldLayersSubsystem::GetDebugTextureForLayer(FName LayerName, UTexture2D* InDebugTexture)
{
	UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName);
	if (!DataLayer)
	{
		return nullptr;
	}

	const int32 Width = DataLayer->Resolution.X;
	const int32 Height = DataLayer->Resolution.Y;

	// Create a new texture if one isn't provided or if the size is wrong
	UTexture2D* DebugTexture = InDebugTexture;
	if (!DebugTexture || DebugTexture->GetSizeX() != Width || DebugTexture->GetSizeY() != Height)
	{
		DebugTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		if (!DebugTexture)
		{
			return nullptr;
		}
	}

	// Lock the texture for writing
	uint8* MipData = static_cast<uint8*>(DebugTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

	UCurveLinearColor* ColorCurve = Cast<UCurveLinearColor>(DataLayer->Config->DebugVisualization.ColorCurve.LoadSynchronous());

	// Copy data from our layer to the texture
	for (int32 y = 0; y < Height; ++y)
	{
		for (int32 x = 0; x < Width; ++x)
		{
			FLinearColor PixelValue = DataLayer->GetValueAtPixel(FIntPoint(x, y));
			FColor MappedColor;

			if (DataLayer->Config->DebugVisualization.VisualizationMode == EWorldDataLayerVisualizationMode::ColorRamp && ColorCurve)
			{
				float NormalizedValue = (PixelValue.R - DataLayer->Config->DebugValueRange.X) / (DataLayer->Config->DebugValueRange.Y - DataLayer->Config->DebugValueRange.X);
				MappedColor = ColorCurve->GetLinearColorValue(FMath::Clamp(NormalizedValue, 0.f, 1.f)).ToFColor(true);
			}
			else // Grayscale
			{
				MappedColor = PixelValue.ToFColor(true); // to sRGB
			}

			int32 PixelIndex = (y * Width + x) * 4; // 4 bytes per pixel (BGRA)
			MipData[PixelIndex] = MappedColor.B;
			MipData[PixelIndex + 1] = MappedColor.G;
			MipData[PixelIndex + 2] = MappedColor.R;
			MipData[PixelIndex + 3] = MappedColor.A;
		}
	}

	// Unlock the texture
	DebugTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	DebugTexture->UpdateResource();

	return DebugTexture;
}

void UWorldLayersSubsystem::ExportLayerToPNG(UWorldDataLayerAsset* LayerAsset, const FString& FilePath)
{
	// It's good practice to check for a valid LayerAsset first
	if (!LayerAsset)
	{
		return;
	}
    
	UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerAsset->LayerName);
	if (!DataLayer)
	{
		// Log a warning if the layer isn't registered in the subsystem
		UE_LOG(LogTemp, Warning, TEXT("ExportLayerToPNG: Could not find registered data layer '%s'"), *LayerAsset->LayerName.ToString());
		return;
	}

	// 1. Create a TArray to hold the raw FColor pixel data.
	TArray<FColor> RawColorData;
	const int32 Width = DataLayer->Resolution.X;
	const int32 Height = DataLayer->Resolution.Y;
	RawColorData.SetNumUninitialized(Width * Height);

	// 2. Copy data from your custom layer format to the FColor TArray.
	for (int32 y = 0; y < Height; ++y)
	{
		for (int32 x = 0; x < Width; ++x)
		{
			const FLinearColor PixelValue = DataLayer->GetValueAtPixel(FIntPoint(x, y));
			RawColorData[y * Width + x] = PixelValue.ToFColor(true); // Convert to sRGB FColor
		}
	}

	// 3. Create an FImageView which is a lightweight "view" of your raw data.
	const FImageView ImageView(RawColorData.GetData(), Width, Height, ERawImageFormat::BGRA8);

	// 4. Use FImageUtils to compress the view into a PNG byte buffer.
	TArray64<uint8> CompressedData;
	if (FImageUtils::CompressImage(CompressedData, TEXT("png"), ImageView))
	{
		// 5. Use FFileHelper to save the byte buffer to disk.
		FFileHelper::SaveArrayToFile(CompressedData, *FilePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ExportLayerToPNG: Failed to compress image data for layer '%s'"), *LayerAsset->LayerName.ToString());
	}
}

void UWorldLayersSubsystem::ImportLayerFromPNG(UWorldDataLayerAsset* LayerAsset, const FString& FilePath)
{
	UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerAsset->LayerName);
	if (DataLayer)
	{
		UTexture2D* ImportedTexture = FImageUtils::ImportFileAsTexture2D(FilePath);
		if (ImportedTexture)
		{
			const FColor* MipData = static_cast<const FColor*>(ImportedTexture->GetPlatformData()->Mips[0].BulkData.LockReadOnly());

			for (int32 y = 0; y < ImportedTexture->GetSizeY(); ++y)
			{
				for (int32 x = 0; x < ImportedTexture->GetSizeX(); ++x)
				{
					FColor PixelColor = MipData[y * ImportedTexture->GetSizeX() + x];
					FLinearColor LinearColor = FLinearColor::FromSRGBColor(PixelColor);
					DataLayer->SetValueAtPixel(FIntPoint(x, y), LinearColor);
				}
			}

			ImportedTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
			LayerAsset->Modify();
		}
	}
}

FIntPoint UWorldLayersSubsystem::WorldLocationToPixel(const FVector2D& WorldLocation, const UWorldDataLayer* DataLayer) const
{
	const FVector2D WorldBounds = DataLayer->Config->WorldGridSize;

	FVector2D NormalizedLocation = (WorldLocation - DataLayer->Config->WorldGridOrigin) / WorldBounds;

	int32 PixelX;
	int32 PixelY;

	if (DataLayer->Config->ResolutionMode == EResolutionMode::Absolute)
	{
		PixelX = FMath::FloorToInt(NormalizedLocation.X * DataLayer->Resolution.X);
		PixelY = FMath::FloorToInt(NormalizedLocation.Y * DataLayer->Resolution.Y);
	}
	else // RelativeToWorld
	{
		PixelX = FMath::FloorToInt(WorldLocation.X / DataLayer->Config->CellSize.X);
		PixelY = FMath::FloorToInt(WorldLocation.Y / DataLayer->Config->CellSize.Y);
	}

	// Clamp pixel coordinates to be within the layer's resolution
	PixelX = FMath::Clamp(PixelX, 0, DataLayer->Resolution.X - 1);
	PixelY = FMath::Clamp(PixelY, 0, DataLayer->Resolution.Y - 1);

	return FIntPoint(PixelX, PixelY);
}

FVector2D UWorldLayersSubsystem::PixelToWorldLocation(const FIntPoint& PixelLocation, const UWorldDataLayer* DataLayer) const
{
	const FVector2D WorldBounds = DataLayer->Config->WorldGridSize;

	FVector2D WorldLocation;

	if (DataLayer->Config->ResolutionMode == EResolutionMode::Absolute)
	{
		WorldLocation.X = (static_cast<float>(PixelLocation.X) / DataLayer->Resolution.X) * WorldBounds.X + DataLayer->Config->WorldGridOrigin.X;
		WorldLocation.Y = (static_cast<float>(PixelLocation.Y) / DataLayer->Resolution.Y) * WorldBounds.Y + DataLayer->Config->WorldGridOrigin.Y;
	}
	else // RelativeToWorld
	{
		WorldLocation.X = PixelLocation.X * DataLayer->Config->CellSize.X;
		WorldLocation.Y = PixelLocation.Y * DataLayer->Config->CellSize.Y;
	}

	return WorldLocation;
}