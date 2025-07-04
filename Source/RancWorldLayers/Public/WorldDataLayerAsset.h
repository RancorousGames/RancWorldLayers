#pragma once

#include "WorldDataLayerAsset.generated.h"

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Editor & Debugging")
	bool bAllowPNG_IO;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Editor & Debugging")
	TArray<FString> ChannelSemantics;

	/*UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Editor & Debugging")
	FWorldDataLayerDebugVisualization DebugVisualization;*/
};