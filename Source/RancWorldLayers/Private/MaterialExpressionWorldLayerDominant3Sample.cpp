// Copyright Rancorous Games, 2026

#include "MaterialExpressionWorldLayerDominant3Sample.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionWorldLayerDominant3Sample"

UMaterialExpressionWorldLayerDominant3Sample::UMaterialExpressionWorldLayerDominant3Sample(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShowOutputNameOnPin = true;

#if WITH_EDITORONLY_DATA
	MenuCategories.Empty();
	MenuCategories.Add(FText::FromString(TEXT("RancWorldLayers")));
#endif
}

#if WITH_EDITOR

int32 UMaterialExpressionWorldLayerDominant3Sample::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// UV pass-through
	if (OutputIndex == 6)
	{
		return Super::Compile(Compiler, 6);
	}

	// 5 is the internal index we created in the parent class to return the full RGBA sample
	int32 SampleCode = Super::Compile(Compiler, 5); 
	if (SampleCode == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	// Constants
	int32 Mul255 = Compiler->Constant(255.0f);
	int32 Div16 = Compiler->Constant(16.0f);
	int32 Div15 = Compiler->Constant(15.0f);
	int32 One = Compiler->Constant(1.0f);

	if (OutputIndex == 0) // Biome 1 (R)
	{
		int32 R = Compiler->ComponentMask(SampleCode, true, false, false, false);
		return Compiler->Round(Compiler->Mul(R, Mul255));
	}
	else if (OutputIndex == 1) // Biome 2 (G)
	{
		int32 G = Compiler->ComponentMask(SampleCode, false, true, false, false);
		return Compiler->Round(Compiler->Mul(G, Mul255));
	}
	else if (OutputIndex == 2) // Biome 3 (B)
	{
		int32 B = Compiler->ComponentMask(SampleCode, false, false, true, false);
		return Compiler->Round(Compiler->Mul(B, Mul255));
	}
	else
	{
		// Weight calculation from Alpha
		int32 A = Compiler->ComponentMask(SampleCode, false, false, false, true);
		int32 AlphaInt = Compiler->Round(Compiler->Mul(A, Mul255));
		
		int32 P1 = Compiler->Floor(Compiler->Div(AlphaInt, Div16));
		int32 P2 = Compiler->Fmod(AlphaInt, Div16);

		int32 W1 = Compiler->Div(P1, Div15);
		int32 W2 = Compiler->Div(P2, Div15);

		if (OutputIndex == 3) // Weight 1
		{
			return W1;
		}
		else if (OutputIndex == 4) // Weight 2
		{
			return W2;
		}
		else if (OutputIndex == 5) // Weight 3
		{
			int32 W1PlusW2 = Compiler->Add(W1, W2);
			return Compiler->Sub(One, W1PlusW2);
		}
	}

	return INDEX_NONE;
}

void UMaterialExpressionWorldLayerDominant3Sample::GetCaption(TArray<FString>& OutCaptions) const
{
	FName TargetName = LayerAsset ? LayerAsset->LayerName : LayerName;
	OutCaptions.Add(FString::Printf(TEXT("Dom3: %s"), *TargetName.ToString()));
}

TArray<FExpressionOutput>& UMaterialExpressionWorldLayerDominant3Sample::GetOutputs()
{
	Outputs.Empty();
	Outputs.Add(FExpressionOutput(TEXT("Biome 1"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("Biome 2"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("Biome 3"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("Weight 1"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("Weight 2"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("Weight 3"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("UV"), 1, 1, 0, 0, 0));
	return Outputs;
}

#endif

#undef LOCTEXT_NAMESPACE
