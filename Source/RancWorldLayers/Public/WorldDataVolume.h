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

	virtual void PostActorCreated() override;
	virtual void PostLoad() override;

	/** The list of all World Data Layer Assets that should be loaded and managed for this level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Data")
	TArray<TSoftObjectPtr<UWorldDataLayerAsset>> LayerAssets;

	/** Defines how queries for locations outside the volume's bounds are handled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Data")
	EOutOfBoundsBehavior OutOfBoundsBehavior = EOutOfBoundsBehavior::ReturnDefaultValue;

	/** Forces the WorldLayersSubsystem to sync with this volume and populate/initialize layers. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "World Data")
	void InitializeSubsystem();

	/** Populates all registered layers with their default data (useful for Editor setup). */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "World Data")
	void PopulateLayers();

	/** If true, the 'PopulateLayers' function will also create and populate a 'BiomeTest' layer with a gradient. Useful for standalone plugin testing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Data|Test")
	bool bAutoPopulateTestLayer = true;

	/** If true, the test gradient will overwrite existing data in the 'BiomeTest' layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Data|Test")
	bool bOverwriteTestLayer = false;

	/** The relative height at which the small debug plane is spawned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Data|Debug")
	float SmallPlaneSpawnHeight = 1000.0f;

protected:
	virtual bool ShouldCheckCollisionComponentForErrors() const override;
};