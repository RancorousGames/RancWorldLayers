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
	SamplerType = SAMPLERTYPE_LinearColor;

#if WITH_EDITORONLY_DATA
	bCollapsed = false;
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionWorldLayerSample::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	FName TargetName = LayerAsset ? LayerAsset->LayerName : LayerName;
	ParameterName = FName(*FString::Printf(TEXT("WL_Texture_%s"), *TargetName.ToString()));

	int32 PosIndex = WorldPosition.GetTracedInput().Expression ? WorldPosition.Compile(Compiler) : Compiler->WorldPosition(WPT_Default);
	int32 PosXY = Compiler->ComponentMask(PosIndex, true, true, false, false);

	int32 OriginCode = INDEX_NONE;
	int32 SizeCode = INDEX_NONE;

	FString OriginSource = TEXT("Pin");
	FString SizeSource = TEXT("Pin");

	// PIN PRIORITY: Check Pins first.
	if (VolumeOrigin.GetTracedInput().Expression) OriginCode = VolumeOrigin.Compile(Compiler);
	if (VolumeSize.GetTracedInput().Expression) SizeCode = VolumeSize.Compile(Compiler);

	// MPC FALLBACK: If pins are empty, use MPC
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
				
				bool bFound = false;
				FLinearColor DefOrigin = MPC->GetVectorParameterDefaultValue(TEXT("WL_Origin"), bFound);
				OriginSource = FString::Printf(TEXT("MPC (%s)"), bFound ? *DefOrigin.ToString() : TEXT("NotFound"));
			}
			if (SizeCode == INDEX_NONE)
			{
				int32 SizeIdx, SizeComp;
				MPC->GetParameterIndex(MPC->GetParameterId(TEXT("WL_Size")), SizeIdx, SizeComp);
				SizeCode = Compiler->AccessCollectionParameter(MPC, SizeIdx, SizeComp);
				
				bool bFound = false;
				FLinearColor DefSize = MPC->GetVectorParameterDefaultValue(TEXT("WL_Size"), bFound);
				SizeSource = FString::Printf(TEXT("MPC (%s)"), bFound ? *DefSize.ToString() : TEXT("NotFound"));
			}
		}
		else
		{
			if (OriginCode == INDEX_NONE) { OriginCode = Compiler->VectorParameter(TEXT("WL_Origin"), FLinearColor::Black); OriginSource = TEXT("Local Param"); }
			if (SizeCode == INDEX_NONE) { SizeCode = Compiler->VectorParameter(TEXT("WL_Size"), FLinearColor(10000.0f, 10000.0f, 0.0f, 0.0f)); SizeSource = TEXT("Local Param"); }
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Node Compile: Layer='%s', OriginSource='%s', SizeSource='%s'"), 
		*TargetName.ToString(), *OriginSource, *SizeSource);

	int32 OriginXY = Compiler->ComponentMask(OriginCode, true, true, false, false);
	int32 SizeXY = Compiler->ComponentMask(SizeCode, true, true, false, false);
	int32 UV = Compiler->Div(Compiler->Sub(PosXY, OriginXY), SizeXY);

	UTexture* ResolveTex = nullptr;
	UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this);
	if (Subsystem) ResolveTex = Subsystem->GetLayerGpuTexture(TargetName);
	if (!ResolveTex && LayerAsset) ResolveTex = LayerAsset->InitialDataTexture.LoadSynchronous();
	if (!ResolveTex) ResolveTex = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));

	Texture = ResolveTex;

	int32 TextureReferenceIndex = INDEX_NONE;
	int32 TextureCode = Compiler->TextureParameter(ParameterName, Texture, TextureReferenceIndex, SamplerType);
	
	// If the specific node (like Dom3) wants just the UVs or just the Raw Sample, it will call this.
	// We handle the actual return based on OutputIndex.
	// 0-4 are standard TextureSample outputs (RGB, R, G, B, A)
	// 5 is the internal "Full RGBA" for child nodes
	// 6 is the calculated UV
	if (OutputIndex == 6) return UV;

	return Compiler->TextureSample(TextureCode, UV, SamplerType, INDEX_NONE, INDEX_NONE, TMVM_None, SSM_FromTextureAsset, TGM_None, TextureReferenceIndex);
}

void UMaterialExpressionWorldLayerSample::GetCaption(TArray<FString>& OutCaptions) const
{
	FName TargetName = LayerAsset ? LayerAsset->LayerName : LayerName;
	OutCaptions.Add(FString::Printf(TEXT("World Layer: %s"), *TargetName.ToString()));
}

#define IF_INPUT_RETURN(Item) if(!InputIndex) return &Item; --InputIndex
FExpressionInput* UMaterialExpressionWorldLayerSample::GetInput(int32 InputIndex)
{
	IF_INPUT_RETURN(WorldPosition);
	IF_INPUT_RETURN(VolumeOrigin);
	IF_INPUT_RETURN(VolumeSize);
	return Super::GetInput(InputIndex);
}
#undef IF_INPUT_RETURN

#define IF_INPUT_RETURN(Name) if(!InputIndex) return Name; --InputIndex
FName UMaterialExpressionWorldLayerSample::GetInputName(int32 InputIndex) const
{
	IF_INPUT_RETURN(TEXT("WorldPosition"));
	IF_INPUT_RETURN(TEXT("VolumeOrigin"));
	IF_INPUT_RETURN(TEXT("VolumeSize"));
	return Super::GetInputName(InputIndex);
}
#undef IF_INPUT_RETURN

TArrayView<FExpressionInput*> UMaterialExpressionWorldLayerSample::GetInputsView()
{
	CachedInputs.Empty();
	int32 Index = 0;
	while (FExpressionInput* Input = GetInput(Index++))
	{
		CachedInputs.Add(Input);
	}
	return CachedInputs;
}

#define IF_INPUT_RETURN_TYPE(Type) if(!InputIndex) return Type; --InputIndex
EMaterialValueType UMaterialExpressionWorldLayerSample::GetInputValueType(int32 InputIndex)
{
	IF_INPUT_RETURN_TYPE(MCT_Float3); // WorldPosition
	IF_INPUT_RETURN_TYPE(MCT_Float3); // VolumeOrigin
	IF_INPUT_RETURN_TYPE(MCT_Float3); // VolumeSize
	return Super::GetInputValueType(InputIndex);
}
#undef IF_INPUT_RETURN_TYPE

#endif

#undef LOCTEXT_NAMESPACE
