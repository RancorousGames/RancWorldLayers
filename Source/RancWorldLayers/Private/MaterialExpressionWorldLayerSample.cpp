#include "MaterialExpressionWorldLayerSample.h"
#include "MaterialCompiler.h"
#include "WorldLayersSubsystem.h"
#include "WorldDataLayerAsset.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/Texture2D.h"
#include "WorldDataVolume.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionWorldLayerSample"

UMaterialExpressionWorldLayerSample::UMaterialExpressionWorldLayerSample(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LayerName = TEXT("Heightmap");
	ParameterName = TEXT("WL_Texture_Default");

#if WITH_EDITORONLY_DATA
	bCollapsed = false;
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionWorldLayerSample::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	FName TargetName = LayerAsset ? LayerAsset->LayerName : LayerName;
	
	// Update the internal parameter name so it shows up correctly in instances
	ParameterName = FName(*FString::Printf(TEXT("WL_Texture_%s"), *TargetName.ToString()));

	// 1. Resolve UV coordinates: (WorldPos.xy - Origin.xy) / Size.xy
	int32 PosIndex = WorldPosition.GetTracedInput().Expression ? WorldPosition.Compile(Compiler) : Compiler->WorldPosition(WPT_Default);
	int32 PosXY = Compiler->ComponentMask(PosIndex, true, true, false, false);

	int32 OriginCode = INDEX_NONE;
	int32 SizeCode = INDEX_NONE;

	if (VolumeOrigin.GetTracedInput().Expression) OriginCode = VolumeOrigin.Compile(Compiler);
	if (VolumeSize.GetTracedInput().Expression) SizeCode = VolumeSize.Compile(Compiler);

	if (OriginCode == INDEX_NONE || SizeCode == INDEX_NONE)
	{
		UMaterialParameterCollection* MPC = Cast<UMaterialParameterCollection>(StaticLoadObject(UMaterialParameterCollection::StaticClass(), nullptr, TEXT("/Game/RancWorldLayers/MPC_WorldLayers")));
		if (!MPC) MPC = Cast<UMaterialParameterCollection>(StaticLoadObject(UMaterialParameterCollection::StaticClass(), nullptr, TEXT("/RancWorldLayers/MPC_WorldLayers")));

		if (MPC)
		{
			if (OriginCode == INDEX_NONE)
			{
				int32 OriginIdx, OriginComp;
				MPC->GetParameterIndex(MPC->GetParameterId(TEXT("WL_Origin")), OriginIdx, OriginComp);
				OriginCode = Compiler->AccessCollectionParameter(MPC, OriginIdx, OriginComp);
			}
			if (SizeCode == INDEX_NONE)
			{
				int32 SizeIdx, SizeComp;
				MPC->GetParameterIndex(MPC->GetParameterId(TEXT("WL_Size")), SizeIdx, SizeComp);
				SizeCode = Compiler->AccessCollectionParameter(MPC, SizeIdx, SizeComp);
			}
		}
		else
		{
			if (OriginCode == INDEX_NONE) OriginCode = Compiler->VectorParameter(TEXT("WL_Origin"), FLinearColor::Black);
			if (SizeCode == INDEX_NONE) SizeCode = Compiler->VectorParameter(TEXT("WL_Size"), FLinearColor(10000.0f, 10000.0f, 0.0f, 0.0f));
		}
	}

	int32 OriginXY = Compiler->ComponentMask(OriginCode, true, true, false, false);
	int32 SizeXY = Compiler->ComponentMask(SizeCode, true, true, false, false);
	int32 UV = Compiler->Div(Compiler->Sub(PosXY, OriginXY), SizeXY);

	// 2. Resolve Texture for the Parameter
	// We set the 'Texture' property of the base class so GetReferencedTextures() works automatically
	UTexture* ResolveTex = nullptr;
	UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this);
	if (Subsystem)
	{
		ResolveTex = Subsystem->GetLayerGpuTexture(TargetName);
	}
	if (!ResolveTex && LayerAsset)
	{
		ResolveTex = LayerAsset->InitialDataTexture.LoadSynchronous();
	}
	if (!ResolveTex)
	{
		ResolveTex = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
	}

	Texture = ResolveTex;

	// 3. Compile as a standard Texture Parameter using our calculated UVs
	// This uses the built-in logic which is much more stable
	int32 TextureReferenceIndex = INDEX_NONE;
	int32 TextureCode = Compiler->TextureParameter(ParameterName, Texture, TextureReferenceIndex, SamplerType);
	
	return Compiler->TextureSample(
		TextureCode, 
		UV, 
		SamplerType, 
		INDEX_NONE, INDEX_NONE, 
		TMVM_None, 
		SSM_FromTextureAsset, 
		TGM_None, 
		TextureReferenceIndex
	);
}

void UMaterialExpressionWorldLayerSample::GetCaption(TArray<FString>& OutCaptions) const
{
	FName TargetName = LayerAsset ? LayerAsset->LayerName : LayerName;
	OutCaptions.Add(FString::Printf(TEXT("World Layer: %s"), *TargetName.ToString()));
}
#endif

#undef LOCTEXT_NAMESPACE
