#pragma once

#include "NiagaraSystem.h"
#include "WorldDataLayerAsset.generated.h"

class UTexture2D;

UENUM()
enum class EResolutionMode : uint8
{
	Absolute,
	RelativeToWorld
};

UENUM()
enum class EDataFormat : uint8
{
	R8,
	R16F,
	RGBA8,
	RGBA16F
};

UENUM()
enum class EWorldDataLayerVisualizationMode : uint8
{
	Grayscale,
	ColorRamp
};

USTRUCT(BlueprintType)
struct FWorldDataLayerDebugVisualization
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Debug Visualization")
	EWorldDataLayerVisualizationMode VisualizationMode = EWorldDataLayerVisualizationMode::Grayscale;

	UPROPERTY(EditAnywhere, Category = "Debug Visualization")
	TSoftObjectPtr<UCurveLinearColor> ColorCurve;
};

UENUM()
enum class EWorldDataLayerStructureType : uint8
{
	Quadtree
};

USTRUCT(BlueprintType)
struct FWorldDataLayerSpatialOptimization
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Spatial Optimization")
	bool bBuildAccelerationStructure = false;

	UPROPERTY(EditAnywhere, Category = "Spatial Optimization", meta = (EditCondition = "bBuildAccelerationStructure"))
	EWorldDataLayerStructureType StructureType = EWorldDataLayerStructureType::Quadtree;

	UPROPERTY(EditAnywhere, Category = "Spatial Optimization", meta = (EditCondition = "bBuildAccelerationStructure"))
	TArray<FLinearColor> ValuesToTrack;
};

UENUM()
enum class EWorldDataLayerReadbackBehavior : uint8
{
	None,
	OnDemand,
	Periodic
};

USTRUCT(BlueprintType)
struct FWorldDataLayerGPUConfiguration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "GPU Configuration")
	bool bKeepUpdatedOnGPU = false;

	UPROPERTY(EditAnywhere, Category = "GPU Configuration")
	bool bIsGPUWritable = false;

	UPROPERTY(EditAnywhere, Category = "GPU Configuration", meta = (EditCondition = "bIsGPUWritable"))
	EWorldDataLayerReadbackBehavior ReadbackBehavior = EWorldDataLayerReadbackBehavior::None;

	UPROPERTY(EditAnywhere, Category = "GPU Configuration", meta = (EditCondition = "bIsGPUWritable"))
	float PeriodicReadbackSeconds = 0.0f;

	UPROPERTY(EditAnywhere, Category = "GPU Configuration")
	TSoftObjectPtr<class UNiagaraSystem> AssociatedNiagaraSystem;
};

UENUM()
enum class EWorldDataLayerMutability : uint8
{
	Immutable,
	InitialOnly,
	Continuous
};

UCLASS()
class RANCWORLDLAYERS_API UWorldDataLayerAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core Identity")
	FName LayerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core Identity")
	FGuid LayerID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data Representation")
	EResolutionMode ResolutionMode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data Representation", meta = (EditCondition = "ResolutionMode == EResolutionMode::Absolute", EditConditionHides))
	FIntPoint Resolution;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data Representation", meta = (EditCondition = "ResolutionMode == EResolutionMode::RelativeToWorld", EditConditionHides))
	FVector2D CellSize;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data Representation")
	EDataFormat DataFormat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data Representation")
	FLinearColor DefaultValue;

	/** Optional texture to populate the layer with initial data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data Representation")
	TSoftObjectPtr<UTexture2D> InitialDataTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Editor & Debugging")
	bool bAllowPNG_IO;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Editor & Debugging")
	TArray<FString> ChannelSemantics;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Editor & Debugging")
	FWorldDataLayerDebugVisualization DebugVisualization;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Editor & Debugging", meta = (EditCondition = "DebugVisualization.VisualizationMode == EWorldDataLayerVisualizationMode::ColorRamp", EditConditionHides))
	FVector2D DebugValueRange;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime Behavior & Optimization")
	FWorldDataLayerGPUConfiguration GPUConfiguration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime Behavior & Optimization")
	EWorldDataLayerMutability Mutability = EWorldDataLayerMutability::Continuous;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime Behavior & Optimization")
	FWorldDataLayerSpatialOptimization SpatialOptimization;
};