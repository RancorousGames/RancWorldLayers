#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UWorldDataLayer.h"
#include "UMyWorldDataSubsystem.generated.h"

UCLASS()
class RANCWORLDLAYERS_API UMyWorldDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Generic Query Methods - The Core API
	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	bool GetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, FLinearColor& OutValue) const;

	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	float GetFloatValueAtLocation(FName LayerName, const FVector2D& WorldLocation) const; // Convenience for single-channel float layers

	// Generic Data Modification (CPU)
	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	void SetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, const FLinearColor& NewValue);

private:
	UPROPERTY()
	TMap<FName, UWorldDataLayer*> WorldDataLayers;

	FIntPoint WorldLocationToPixel(const FVector2D& WorldLocation, const UWorldDataLayer* DataLayer) const;
};
