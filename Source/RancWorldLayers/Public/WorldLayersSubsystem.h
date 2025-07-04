#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WorldDataLayer.h"
#include "WorldLayersSubsystem.generated.h"

class UWorldDataLayerAsset;

UCLASS()
class RANCWORLDLAYERS_API UWorldLayersSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	virtual void Deinitialize() override;

	static UWorldLayersSubsystem* Get(UObject* WorldContext);

	// Generic Query Methods - The Core API
	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	bool GetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, FLinearColor& OutValue) const;

	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	float GetFloatValueAtLocation(FName LayerName, const FVector2D& WorldLocation) const; // Convenience for single-channel float layers

	// Generic Data Modification (CPU)
	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	void SetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, const FLinearColor& NewValue);

	void RegisterDataLayer(UWorldDataLayerAsset* LayerAsset);

	// Editor Utility and Debugging
	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	UTexture2D* GetDebugTextureForLayer(FName LayerName);

	void ExportLayerToPNG(UWorldDataLayerAsset* LayerAsset, const FString& FilePath);
	void ImportLayerFromPNG(UWorldDataLayerAsset* LayerAsset, const FString& FilePath);

private:
	UPROPERTY()
	TMap<FName, UWorldDataLayer*> WorldDataLayers;

	FIntPoint WorldLocationToPixel(const FVector2D& WorldLocation, const UWorldDataLayer* DataLayer) const;
};
