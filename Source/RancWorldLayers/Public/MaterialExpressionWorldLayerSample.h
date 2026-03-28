#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "MaterialExpressionWorldLayerSample.generated.h"

/**
 * A material expression that samples a specific World Data Layer at a world location.
 * Automatically handles UV mapping based on the provided or global volume bounds.
 */
UCLASS(collapsecategories, hidecategories = Object)
class RANCWORLDLAYERS_API UMaterialExpressionWorldLayerSample : public UMaterialExpressionTextureSampleParameter2D
{
	GENERATED_UCLASS_BODY()

	/** Optional: Pick the Layer Asset directly to resolve the layer name and initial texture more reliably. */
	UPROPERTY(EditAnywhere, Category = "World Layer")
	TObjectPtr<class UWorldDataLayerAsset> LayerAsset;

	/** The name of the layer to sample (if LayerAsset is not set). */
	UPROPERTY(EditAnywhere, Category = "World Layer", meta = (EditCondition = "LayerAsset == nullptr"))
	FName LayerName;

	/** Optional world position to sample at. Defaults to Absolute World Position (XY). */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to Absolute World Position if not specified"))
	FExpressionInput WorldPosition;

	/** Optional Origin of the volume. If not specified, looks for a parameter or MPC. */
	UPROPERTY(meta = (RequiredInput = "false"))
	FExpressionInput VolumeOrigin;

	/** Optional Size of the volume. If not specified, looks for a parameter or MPC. */
	UPROPERTY(meta = (RequiredInput = "false"))
	FExpressionInput VolumeSize;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};
