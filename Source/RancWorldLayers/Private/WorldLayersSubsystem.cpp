#include "WorldLayersSubsystem.h"

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

void UWorldLayersSubsystem::Deinitialize()
{
	WorldDataLayers.Empty();
	Super::Deinitialize();
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
	}
}

#include "WorldDataLayer.h"

UTexture2D* UWorldLayersSubsystem::GetDebugTextureForLayer(FName LayerName)
{
	UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName);
	if (!DataLayer)
	{
		return nullptr;
	}

	// Create a transient texture to visualize the data
	UTexture2D* DebugTexture = UTexture2D::CreateTransient(DataLayer->Resolution.X, DataLayer->Resolution.Y, PF_B8G8R8A8);
	if (!DebugTexture)
	{
		return nullptr;
	}

	// Lock the texture for writing
	uint8* MipData = static_cast<uint8*>(DebugTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

	// Copy data from our layer to the texture
	for (int32 y = 0; y < DataLayer->Resolution.Y; ++y)
	{
		for (int32 x = 0; x < DataLayer->Resolution.X; ++x)
		{
			FLinearColor PixelValue = DataLayer->GetValueAtPixel(FIntPoint(x, y));
			FColor MappedColor = PixelValue.ToFColor(true); // to sRGB
			int32 PixelIndex = (y * DataLayer->Resolution.X + x) * 4; // 4 bytes per pixel (BGRA)
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
	// Placeholder for world bounds. This should ideally come from a global game setting or world properties.
	const FVector2D WorldBounds = FVector2D(102400.0f, 102400.0f); // Assuming 1024m x 1024m world

	FVector2D NormalizedLocation = WorldLocation / WorldBounds;

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
