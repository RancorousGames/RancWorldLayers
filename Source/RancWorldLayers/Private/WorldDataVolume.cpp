#include "WorldDataVolume.h"
#include "Components/BrushComponent.h"
#include "WorldLayersSubsystem.h"

AWorldDataVolume::AWorldDataVolume()
{
	// Make the volume visible in the editor but with no collision.
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	GetBrushComponent()->SetCanEverAffectNavigation(false);

#if WITH_EDITOR
	GetBrushComponent()->SetIsVisualizationComponent(true);
#endif

	bIsEditorOnlyActor = false;
	
	bColored = true;
	BrushColor.R = 25;
	BrushColor.G = 255;
	BrushColor.B = 25;
	BrushColor.A = 255;
}

void AWorldDataVolume::PostActorCreated()
{
	Super::PostActorCreated();
	UE_LOG(LogTemp, Log, TEXT("AWorldDataVolume: PostActorCreated. Registering with subsystem."));
	if (UWorldLayersSubsystem* WLS = UWorldLayersSubsystem::Get(this))
	{
		WLS->InitializeFromVolume(this);
	}
}

void AWorldDataVolume::PostLoad()
{
	Super::PostLoad();
	UE_LOG(LogTemp, Log, TEXT("AWorldDataVolume: PostLoad. Registering with subsystem."));
	if (UWorldLayersSubsystem* WLS = UWorldLayersSubsystem::Get(this))
	{
		WLS->InitializeFromVolume(this);
	}
}

bool AWorldDataVolume::ShouldCheckCollisionComponentForErrors() const
{
	return false;
}

void AWorldDataVolume::InitializeSubsystem()
{
	if (UWorldLayersSubsystem* Sub = UWorldLayersSubsystem::Get(this))
	{
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] WorldDataVolume: Explicit initialization requested. Clearing first."));
		Sub->ClearAllLayers();
		Sub->InitializeFromVolume(this);
	}
}

void AWorldDataVolume::PopulateLayers()
{
	UWorldLayersSubsystem* Sub = UWorldLayersSubsystem::Get(this);
	if (!Sub) return;

	// If no volume is registered, take ownership
	if (!Sub->GetWorldDataVolume())
	{
		InitializeSubsystem();
	}

	// Singleton Guard: If we aren't the lucky one, stop here.
	if (Sub->GetWorldDataVolume() != this)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[RancWorldLayers] Volume '%s' is inactive. Ignoring population request."), *GetName());
		return;
	}

	int32 AssetsSuccessfullyLoaded = 0;
	UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] WorldDataVolume: Populating %d layers from LayerAssets."), LayerAssets.Num());

	for (const TSoftObjectPtr<UWorldDataLayerAsset>& LayerAssetPtr : LayerAssets)
	{
		if (UWorldDataLayerAsset* LayerAsset = LayerAssetPtr.LoadSynchronous())
		{
			AssetsSuccessfullyLoaded++;
			if (UWorldDataLayer* ExistingLayer = const_cast<UWorldDataLayer*>(Sub->GetDataLayer(LayerAsset->LayerName)))
			{
				bool bShouldReinit = false;
				if (GIsEditor && LayerAsset->Mutability == EWorldDataLayerMutability::InitialOnly) bShouldReinit = true;
				else if (!ExistingLayer->bHasBeenInitializedFromTexture) bShouldReinit = true;

				if (bShouldReinit) ExistingLayer->Reinitialize(Sub->GetWorldGridSize());
				
				if (LayerAsset->Mutability == EWorldDataLayerMutability::Derivative)
				{
					Sub->UpdateDerivativeLayer(LayerAsset->LayerName);
				}
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Registering new layer from asset: %s"), *LayerAsset->LayerName.ToString());
				Sub->RegisterDataLayer(LayerAsset);
			}
		}
	}

	if (bAutoPopulateTestLayer && AssetsSuccessfullyLoaded == 0)
	{
		const FName LayerName = "__InternalTestLayer__";
		const UWorldDataLayer* DataLayer = Sub->GetDataLayer(LayerName);
		if (!DataLayer)
		{
			UWorldDataLayerAsset* PocAsset = NewObject<UWorldDataLayerAsset>(this);
			PocAsset->LayerName = LayerName;
			PocAsset->ResolutionMode = EResolutionMode::Absolute;
			PocAsset->Resolution = FIntPoint(1024, 1024);
			PocAsset->DataFormat = EDataFormat::RGBA8; 
			Sub->RegisterDataLayer(PocAsset);
			DataLayer = Sub->GetDataLayer(LayerName);
		}

		if (DataLayer && (bOverwriteTestLayer || DataLayer->Config->InitialDataTexture.IsNull()))
		{
			UWorldDataLayer* MutableLayer = const_cast<UWorldDataLayer*>(DataLayer);
			for (int32 Y = 0; Y < MutableLayer->Resolution.Y; ++Y)
			{
				for (int32 X = 0; X < MutableLayer->Resolution.X; ++X)
				{
					float Value = (float)X / (float)MutableLayer->Resolution.X;
					MutableLayer->SetValueAtPixel(FIntPoint(X, Y), FLinearColor(Value, Value, Value, 1.0f));
				}
			}
			MutableLayer->bHasBeenInitializedFromTexture = true;
		}
	}
}
