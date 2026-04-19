// Copyright Rancorous Games, 2026

#pragma once

#include "CoreMinimal.h"
#include "MaterialExpressionWorldLayerSample.h"
#include "MaterialExpressionWorldLayerDominant3Sample.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class RANCWORLDLAYERS_API UMaterialExpressionWorldLayerDominant3Sample : public UMaterialExpressionWorldLayerSample
{
	GENERATED_BODY()

public:
	UMaterialExpressionWorldLayerDominant3Sample(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual TArray<FExpressionOutput>& GetOutputs() override;
#endif
};
