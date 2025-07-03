#include "UMyWorldDataSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UWorldDataLayerAsset.h"

void UMyWorldDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Discover all UWorldDataLayerAsset assets
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetData;
	AssetRegistryModule.Get().GetAssetsByClass(UWorldDataLayerAsset::StaticClass()->GetClassPathName(), AssetData);

	for (const FAssetData& Data : AssetData)
	{
		UWorldDataLayerAsset* LayerAsset = Cast<UWorldDataLayerAsset>(Data.GetAsset());
		if (LayerAsset)
		{
			UWorldDataLayer* NewDataLayer = NewObject<UWorldDataLayer>(this);
			NewDataLayer->Initialize(LayerAsset);
			WorldDataLayers.Add(LayerAsset->LayerName, NewDataLayer);
		}
	}
}

void UMyWorldDataSubsystem::Deinitialize()
{
	WorldDataLayers.Empty();
	Super::Deinitialize();
}

bool UMyWorldDataSubsystem::GetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, FLinearColor& OutValue) const
{
	const UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName);
	if (DataLayer)
	{
		FIntPoint PixelCoords = WorldLocationToPixel(WorldLocation, DataLayer);
		OutValue = DataLayer->GetValueAtPixel(PixelCoords);
		return true;
	}
	return false;
}

float UMyWorldDataSubsystem::GetFloatValueAtLocation(FName LayerName, const FVector2D& WorldLocation) const
{
	FLinearColor Value;
	if (GetValueAtLocation(LayerName, WorldLocation, Value))
	{
		return Value.R; // Assuming R channel holds the float value for single-channel layers
	}
	return 0.0f;
}

void UMyWorldDataSubsystem::SetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, const FLinearColor& NewValue)
{
	UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName);
	if (DataLayer)
	{
		FIntPoint PixelCoords = WorldLocationToPixel(WorldLocation, DataLayer);
		DataLayer->SetValueAtPixel(PixelCoords, NewValue);
	}
}

FIntPoint UMyWorldDataSubsystem::WorldLocationToPixel(const FVector2D& WorldLocation, const UWorldDataLayer* DataLayer) const
{
	// This is a placeholder implementation. Proper conversion will depend on world bounds and layer resolution.
	// For now, assuming a simple mapping where (0,0) world maps to (0,0) pixel.
	// And a world size of 1024x1024 units maps to the layer's resolution.

	FVector2D NormalizedLocation = WorldLocation / 1024.0f; // Normalize to 0-1 range based on an assumed world size

	int32 PixelX = FMath::FloorToInt(NormalizedLocation.X * DataLayer->Resolution.X);
	int32 PixelY = FMath::FloorToInt(NormalizedLocation.Y * DataLayer->Resolution.Y);

	return FIntPoint(PixelX, PixelY);
}
