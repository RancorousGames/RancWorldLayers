#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "WorldDataLayerAsset.h"
#include "WorldDataLayer.generated.h"

class FQuadtree;

UCLASS()
class RANCWORLDLAYERS_API UWorldDataLayer : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UWorldDataLayerAsset> Config;

	bool bIsInitializing = false;

	UPROPERTY()
	FIntPoint Resolution;

	TArray<uint8> RawData;

	bool bIsDirty;
	bool bHasBeenInitializedFromTexture = false;

	UPROPERTY()
	UTexture* GpuRepresentation;

	TMap<FLinearColor, TSharedPtr<FQuadtree>> SpatialIndices;

	float LastReadbackTime;

	void Initialize(UWorldDataLayerAsset* InConfig, const FVector2D& InWorldGridSize);
	void Reinitialize(const FVector2D& InWorldGridSize);
	FLinearColor GetValueAtPixel(const FIntPoint& PixelCoords) const;
	void SetValueAtPixel(const FIntPoint& PixelCoords, const FLinearColor& NewValue);
	
	int32 GetBytesPerPixel() const;

private:
	int32 GetPixelIndex(const FIntPoint& PixelCoords) const;
};