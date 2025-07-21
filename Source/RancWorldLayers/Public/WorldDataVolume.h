#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "WorldDataVolume.generated.h"

class UWorldDataLayerAsset;

UENUM(BlueprintType)
enum class EOutOfBoundsBehavior : uint8
{
	/** Queries outside the volume will return the layer's default value. */
	ReturnDefaultValue,
	/** Queries outside the volume will be clamped to the nearest edge pixel. */
	ClampToEdge
};

/**
 * The central actor that defines the spatial bounds and active data layers for a level's World Data System.
 * There should be exactly one of these actors per persistent level.
 */
UCLASS(Blueprintable, hidecategories = (Collision, Brush, WorldPartition, Input, Cooking))
class RANCWORLDLAYERS_API AWorldDataVolume : public AVolume
{
	GENERATED_BODY()

public:
	AWorldDataVolume();

	/** The list of all World Data Layer Assets that should be loaded and managed for this level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Data")
	TArray<TSoftObjectPtr<UWorldDataLayerAsset>> LayerAssets;

	/** Defines how queries for locations outside the volume's bounds are handled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Data")
	EOutOfBoundsBehavior OutOfBoundsBehavior = EOutOfBoundsBehavior::ReturnDefaultValue;

protected:
	virtual bool ShouldCheckCollisionComponentForErrors() const override;
};