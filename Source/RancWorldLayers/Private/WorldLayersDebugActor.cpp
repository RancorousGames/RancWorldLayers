
#include "WorldLayersDebugActor.h"
#include "WorldLayersDebugWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "WorldLayersSubsystem.h"
#include "WorldDataVolume.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/DecalComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"

#include "Framework/Application/IInputProcessor.h"

#if WITH_EDITOR
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Widgets/SWeakWidget.h"
#endif

AWorldLayersDebugActor::AWorldLayersDebugActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bIsEditorOnlyActor = false;

	FMemory::Memzero(bLastNumPadKeysDown, sizeof(bLastNumPadKeysDown));

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	DebugMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugMesh"));
	DebugMesh->SetupAttachment(RootComponent);
	DebugMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	DebugMesh->SetCastShadow(false);
	DebugMesh->SetVisibility(false);

	DebugDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("DebugDecal"));
	DebugDecal->SetupAttachment(RootComponent);
	DebugDecal->SetVisibility(false);
	DebugDecal->DecalSize = FVector(1000.0f, 100.0f, 100.0f); // Default size
	DebugDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f)); // Point down

	// Try to find a default plane mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane"));
	if (PlaneMesh.Succeeded())
	{
		DebugMesh->SetStaticMesh(PlaneMesh.Object);
	}
}

void AWorldLayersDebugActor::PostActorCreated()
{
	Super::PostActorCreated();
	
	// In Editor, we want to immediately create the widget and position
	if (GIsEditor && !GetWorld()->IsGameWorld())
	{
		InitializeActor();
	}
}

void AWorldLayersDebugActor::Destroyed()
{
#if WITH_EDITOR
	if (GIsEditor && CachedSlateWidget.IsValid())
	{
		if (BoundViewport.IsValid())
		{
			BoundViewport.Pin()->RemoveOverlayWidget(CachedSlateWidget.ToSharedRef());
		}
		CachedSlateWidget.Reset();
		BoundViewport.Reset();
	}
#endif

	if (DebugWidgetInstance)
	{
		DebugWidgetInstance->ClearFlags(RF_Standalone);
		DebugWidgetInstance = nullptr;
	}

	Super::Destroyed();
}

void AWorldLayersDebugActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeActor();
}

void AWorldLayersDebugActor::InitializeActor()
{
	CreateDebugWidget();
	PositionActor();
	RefreshLayerNames();

	// Load the custom debug render target
	DebugRenderTargetInstance = Cast<UTextureRenderTarget2D>(StaticLoadObject(UTextureRenderTarget2D::StaticClass(), nullptr, *DebugRenderTargetPath));
	if (DebugRenderTargetInstance)
	{
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Successfully loaded custom debug Render Target: %s"), *DebugRenderTargetPath);
	}

	// Load the custom debug materials
	UMaterialInterface* DebugMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *DebugMaterialPath));
	UMaterialInterface* DebugDecalMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *DebugDecalMaterialPath));

	if (DebugMat)
	{
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Successfully loaded custom debug material: %s"), *DebugMaterialPath);
		
		if (DebugMesh)
		{
			DebugMesh->SetMaterial(0, DebugMat);
			PlaneMID = DebugMesh->CreateDynamicMaterialInstance(0, DebugMat);
		}
	}

	if (DebugDecalMat)
	{
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Successfully loaded custom debug decal material: %s"), *DebugDecalMaterialPath);
		if (DebugDecal)
		{
			DebugDecal->SetDecalMaterial(DebugDecalMat);
			DecalMID = DebugDecal->CreateDynamicMaterialInstance();
		}
	}
	else if (DebugMat)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RancWorldLayers] Decal material not found at %s, falling back to %s for decal (may not render)."), *DebugDecalMaterialPath, *DebugMaterialPath);
		if (DebugDecal)
		{
			DebugDecal->SetDecalMaterial(DebugMat);
			DecalMID = DebugDecal->CreateDynamicMaterialInstance();
		}
	}

	if (!DebugMat && !DebugDecalMat)
	{
		UE_LOG(LogTemp, Error, TEXT("[RancWorldLayers] FAILED to load custom debug materials. 3D visualization may be incorrect."));
	}

	Set3DMode(Current3DMode);
}

void AWorldLayersDebugActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if WITH_EDITOR
	if (GIsEditor && CachedSlateWidget.IsValid())
	{
		if (BoundViewport.IsValid())
		{
			BoundViewport.Pin()->RemoveOverlayWidget(CachedSlateWidget.ToSharedRef());
		}
		CachedSlateWidget.Reset();
		BoundViewport.Reset();
	}
#endif

	if (DebugWidgetInstance)
	{
		DebugWidgetInstance->ClearFlags(RF_Standalone);
		DebugWidgetInstance = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AWorldLayersDebugActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	HandleDebugInput();
	Update3DVisualization();
}

void AWorldLayersDebugActor::HandleDebugInput()
{
	UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this);
	if (!Subsystem) return;

	auto IsKeyDown = [&](const FKey& Key) { return Subsystem->IsKeyDown(Key); };

	const bool bCtrlDown = IsKeyDown(EKeys::LeftControl) || IsKeyDown(EKeys::RightControl);
	if (!bCtrlDown) return;

	// Consolidated Cycle (NumPad 0)
	bool bNumPad0Down = IsKeyDown(EKeys::NumPadZero);
	if (bNumPad0Down && !bLastNumPad0Down)
	{
		// Unified Cycle Logic
		// States: 
		// 0: All Hidden
		// 1: 3D Small Plane
		// 2: 3D World Plane
		// 3: 3D Decal
		// 4: UI MiniMap

		CombinedMode = (CombinedMode + 1) % 5;

		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Debug Mode Cycled [Actor:%p]: NewMode=%d"), this, CombinedMode);

		OnDebugModeChanged(CombinedMode);

		// Notify anyone else interested
		Subsystem->OnRequestUpdate.Broadcast();

		// Autoritative Plugin Fix: Tell the Volume to ensure its layers are ready
		int32 VolumeCount = 0;
		for (TActorIterator<AWorldDataVolume> It(GetWorld()); It; ++It)
		{
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Triggering PopulateLayers on Volume: %s"), *It->GetName());
			It->PopulateLayers();
			VolumeCount++;
		}
		if (VolumeCount == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[RancWorldLayers] No AWorldDataVolume found to populate layers for."));
		}

		switch (CombinedMode)
		{
		case 0: // All Hidden
			if (DebugWidgetInstance) DebugWidgetInstance->SetDebugMode(EWorldLayersDebugMode::Hidden);
			Set3DMode(EWorldLayers3DMode::None);
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Now showing: None"));
			break;
		case 1: // 3D Small Plane
			if (DebugWidgetInstance) DebugWidgetInstance->SetDebugMode(EWorldLayersDebugMode::Hidden);
			Set3DMode(EWorldLayers3DMode::SmallPlane);
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Now showing: 3D Small Plane"));
			break;
		case 2: // 3D World sized Plane
			if (DebugWidgetInstance) DebugWidgetInstance->SetDebugMode(EWorldLayersDebugMode::Hidden);
			Set3DMode(EWorldLayers3DMode::WorldPlane);
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Now showing: World sized Plane"));
			break;
		case 3: // 3D Decal
			if (DebugWidgetInstance) DebugWidgetInstance->SetDebugMode(EWorldLayersDebugMode::Hidden);
			Set3DMode(EWorldLayers3DMode::Decal);
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Now showing: 3D Decal"));
			break;
		case 4: // UI MiniMap
			if (DebugWidgetInstance) DebugWidgetInstance->SetDebugMode(EWorldLayersDebugMode::MiniMap);
			Set3DMode(EWorldLayers3DMode::None);
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Now showing: UI MiniMap"));
			break;
		}
	}
	bLastNumPad0Down = bNumPad0Down;

	// SELECT LAYER (NumPad 1-9)
	FKey NumKeys[] = { 
		EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, 
		EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, 
		EKeys::NumPadEight, EKeys::NumPadNine 
	};

	bool bAnyDebugKeyPressed = (bNumPad0Down && !bLastNumPad0Down);
	int32 NewSelectedLayer = -1;

	for (int32 i = 1; i <= 9; ++i)
	{
		bool bKeyDown = IsKeyDown(NumKeys[i]);
		if (bKeyDown && !bLastNumPadKeysDown[i])
		{
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] HandleDebugInput [Actor:%p]: NumPad %d Detected. Selecting Layer %d."), this, i, i-1);
			NewSelectedLayer = i - 1;
			bAnyDebugKeyPressed = true;
		}
		bLastNumPadKeysDown[i] = bKeyDown;
	}

	// FULL AUTO-REFRESH LOGIC
	if (bAnyDebugKeyPressed && Subsystem)
	{
		// 1. Update selection if needed
		if (NewSelectedLayer != -1)
		{
			SelectedLayerIndex = NewSelectedLayer;
			if (DebugWidgetInstance) DebugWidgetInstance->SetSelectedLayer(SelectedLayerIndex);
		}

		// 2. Force Subsystem & Volume Refresh
		for (TActorIterator<AWorldDataVolume> It(GetWorld()); It; ++It)
		{
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Auto-Refreshing Volume: %s"), *It->GetName());
			It->PopulateLayers(); // PopulateLayers now handles the ownership/reset check internally
		}

		// 3. Update the UI names (critical since PopulateLayers might have changed the layer list)
		UpdateDebugWidget();

		// 4. Notify anyone else interested
		Subsystem->OnRequestUpdate.Broadcast();
	}
}

void AWorldLayersDebugActor::CreateDebugWidget()
{
	if (DebugWidgetClass && !DebugWidgetInstance)
	{
		UWorld* World = GetWorld();
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Creating Debug Widget. World: %s"), World ? *World->GetName() : TEXT("NULL"));
		
		DebugWidgetInstance = CreateWidget<UWorldLayersDebugWidget>(World, DebugWidgetClass);
		if (DebugWidgetInstance)
		{
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Debug Widget Created Successfully. Adding to Viewport/Overlay."));
			if (World->IsGameWorld())
			{
				DebugWidgetInstance->AddToViewport();
			}
#if WITH_EDITOR
			else if (GIsEditor)
			{
				FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
				TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveLevelViewport();
				if (ActiveLevelViewport.IsValid())
				{
					TSharedRef<SWidget> SlateWidget = DebugWidgetInstance->TakeWidget();
					CachedSlateWidget = SlateWidget;
					BoundViewport = ActiveLevelViewport;

					UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Adding Widget to Editor Viewport Overlay."));
					ActiveLevelViewport->AddOverlayWidget(SlateWidget);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[RancWorldLayers] Could not find an active Level Editor Viewport to add the debug widget."));
				}
			}
#endif
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[RancWorldLayers] Failed to create Debug Widget instance."));
		}
	}
}

void AWorldLayersDebugActor::PositionActor()
{
	UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this);
	if (!Subsystem) return;

	AWorldDataVolume* Volume = nullptr;
	for (TActorIterator<AWorldDataVolume> It(GetWorld()); It; ++It)
	{
		Volume = *It;
		break;
	}

	if (Volume)
	{
		const FBox VolumeBounds = Volume->GetBounds().GetBox();
		FVector Center = VolumeBounds.GetCenter();
		SetActorLocation(Center);
		
		FVector Size = VolumeBounds.GetSize();
		// Update Decal Size (extents)
		DebugDecal->DecalSize = FVector(Size.Z * 0.5f + 1000.0f, Size.Y * 0.5f, Size.X * 0.5f);
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] PositionActor: Centered on Volume at %s. Size: %s"), *Center.ToString(), *Size.ToString());
	}
}

void AWorldLayersDebugActor::Set3DMode(EWorldLayers3DMode NewMode)
{
	UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Set3DMode: %d"), (int32)NewMode);
	Current3DMode = NewMode;

	DebugMesh->SetVisibility(false);
	DebugDecal->SetVisibility(false);

	UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this);
	FVector2D Size = Subsystem ? Subsystem->GetWorldGridSize() : FVector2D(10000.0f, 10000.0f);
	UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Subsystem Size for Scaling: %s"), *Size.ToString());

	switch (NewMode)
	{
	case EWorldLayers3DMode::Decal:
		DebugDecal->SetVisibility(true);
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Decal Visibility set to TRUE. Size: %s"), *DebugDecal->DecalSize.ToString());
		break;
	case EWorldLayers3DMode::SmallPlane:
		DebugMesh->SetVisibility(true);
		DebugMesh->SetWorldScale3D(FVector(10.0f, 10.0f, 1.0f)); // 1000x1000 units
		{
			float SpawnHeight = 1000.0f;
			if (Subsystem && Subsystem->GetWorldDataVolume())
			{
				SpawnHeight = Subsystem->GetWorldDataVolume()->SmallPlaneSpawnHeight;
			}
			DebugMesh->SetRelativeLocation(FVector(0.0f, 0.0f, SpawnHeight));
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Small Plane Visibility set to TRUE. Scale: 10.0, Hover: %f"), SpawnHeight);
		}
		break;
	case EWorldLayers3DMode::WorldPlane:
		DebugMesh->SetVisibility(true);
		DebugMesh->SetWorldScale3D(FVector(Size.X / 100.0f, Size.Y / 100.0f, 1.0f));
		DebugMesh->SetRelativeLocation(FVector::ZeroVector);
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] World Plane Visibility set to TRUE. Scale: %f, %f"), Size.X / 100.0f, Size.Y / 100.0f);
		break;
	default:
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] 3D Visualization DISABLED."));
		break;
	}
}

void AWorldLayersDebugActor::UpdateDebugWidget()
{
	if (DebugWidgetInstance)
	{
		DebugWidgetInstance->RefreshLayerNames();
	}
}

void AWorldLayersDebugActor::RefreshLayerNames()
{
	if (UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this))
	{
		TArray<FName> NewNames = Subsystem->GetActiveLayerNames();
		
		bool bNamesChanged = (NewNames.Num() != LayerNames.Num());
		if (!bNamesChanged)
		{
			for (int32 i = 0; i < NewNames.Num(); ++i)
			{
				if (NewNames[i] != LayerNames[i])
				{
					bNamesChanged = true;
					break;
				}
			}
		}

		if (bNamesChanged)
		{
			LayerNames = NewNames;
			UpdateDebugWidget();
		}
	}
}

void AWorldLayersDebugActor::Update3DVisualization()
{
	if (Current3DMode == EWorldLayers3DMode::None) return;

	UMaterialInstanceDynamic* TargetMID = (Current3DMode == EWorldLayers3DMode::Decal) ? DecalMID : PlaneMID;
	if (!TargetMID)
	{
		// Attempt to recreate MID if NULL
		if (Current3DMode == EWorldLayers3DMode::Decal)
		{
			if (DebugDecal->GetDecalMaterial()) DecalMID = DebugDecal->CreateDynamicMaterialInstance();
			TargetMID = DecalMID;
		}
		else
		{
			if (DebugMesh->GetMaterial(0)) PlaneMID = DebugMesh->CreateDynamicMaterialInstance(0, DebugMesh->GetMaterial(0));
			TargetMID = PlaneMID;
		}
		
		if (!TargetMID) return;
	}

	UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this);
	if (!Subsystem) return;

	RefreshLayerNames();

	if (LayerNames.Num() > 0)
	{
		SelectedLayerIndex = FMath::Clamp(SelectedLayerIndex, 0, LayerNames.Num() - 1);
		FName TargetLayerName = LayerNames[SelectedLayerIndex];

		const UWorldDataLayer* DataLayer = Subsystem->GetDataLayer(TargetLayerName);
		if (!DataLayer) return;

		// --- OPTIMIZATION START ---
		
		const bool bLayerChanged = (LastLayerName != TargetLayerName);
		const bool bModeChanged = (LastLoggedMode != Current3DMode);
		const bool bDataDirty = DataLayer->bIsDirty;

		static int32 ForceUpdateFrames = 0;
		if (bLayerChanged || bModeChanged)
		{
			ForceUpdateFrames = 2; // Update for 2 frames to ensure RT is ready and MID binds correctly
		}

		if (bLayerChanged || bModeChanged || bDataDirty || ForceUpdateFrames > 0)
		{
			if (ForceUpdateFrames > 0) ForceUpdateFrames--;

			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Update3D [Actor:%p]: Layer='%s' (Changed:%s), Mode=%d (Changed:%s), Dirty=%s, ForceRemaining=%d"), 
				this, *TargetLayerName.ToString(), bLayerChanged ? TEXT("Yes") : TEXT("No"), 
				(int32)Current3DMode, bModeChanged ? TEXT("Yes") : TEXT("No"), 
				bDataDirty ? TEXT("Yes") : TEXT("No"), ForceUpdateFrames);

			// Use Render Target if available
			if (DebugRenderTargetInstance)
			{
				bool bResize = (DebugRenderTargetInstance->SizeX != DataLayer->Resolution.X || DebugRenderTargetInstance->SizeY != DataLayer->Resolution.Y);
				UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Update3D: Updating RT '%s' (Res:%dx%d, NeedsResize:%s)"), 
					*DebugRenderTargetInstance->GetName(), DataLayer->Resolution.X, DataLayer->Resolution.Y, bResize ? TEXT("Yes") : TEXT("No"));
				
				Subsystem->UpdateDebugRenderTarget(TargetLayerName, DebugRenderTargetInstance);
			}

			// Clear the dirty flag
			UWorldDataLayer* MutableLayer = const_cast<UWorldDataLayer*>(DataLayer);
			MutableLayer->bIsDirty = false;

			// Fallback/Legacy: Still get Tex for logging and potential override
			UTexture* Tex = Subsystem->GetLayerGpuTexture(TargetLayerName);
			if (!Tex)
			{
				Tex = Subsystem->GetDebugTextureForLayer(TargetLayerName, DebugTextureInstance);
				DebugTextureInstance = Cast<UTexture2D>(Tex);
			}

			// Use the provided Render Target as the primary source if valid
			UTexture* FinalTex = DebugRenderTargetInstance ? (UTexture*)DebugRenderTargetInstance : Tex;

			if (FinalTex)
			{
				if (TargetMID)
				{
					TargetMID->SetTextureParameterValue(TEXT("Texture"), FinalTex);
					TargetMID->SetTextureParameterValue(TEXT("Diffuse"), FinalTex);
					TargetMID->SetTextureParameterValue(TEXT("BaseColor"), FinalTex);
					
					UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] 3D Viz [Actor:%p]: Applied Texture '%s' [Res:%s] to MID '%s'"), 
						this, *FinalTex->GetName(), 
						FinalTex->GetResource() ? *FString::Printf(TEXT("%dx%d"), FinalTex->GetResource()->GetSizeX(), FinalTex->GetResource()->GetSizeY()) : TEXT("NoResource"),
						*TargetMID->GetName());
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[RancWorldLayers] Update3D: TargetMID is NULL for mode %d"), (int32)Current3DMode);
				}
			}

			LastLayerName = TargetLayerName;
			LastLoggedMode = Current3DMode;
		}
		// --- OPTIMIZATION END ---
	}
}
