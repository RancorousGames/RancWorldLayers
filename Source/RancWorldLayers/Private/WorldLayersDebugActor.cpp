
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

/** Global input processor to catch keys in the Editor even without focus/PIE. */
class FWorldLayersInputProcessor : public IInputProcessor
{
public:
	TSet<FKey> PressedKeys;

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		FKey Key = InKeyEvent.GetKey();
		PressedKeys.Add(Key);
		UE_LOG(LogTemp, VeryVerbose, TEXT("[RancWorldLayers] InputProcessor KeyDown: %s"), *Key.ToString());
		return false; // Don't consume the event
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		FKey Key = InKeyEvent.GetKey();
		PressedKeys.Remove(Key);
		UE_LOG(LogTemp, VeryVerbose, TEXT("[RancWorldLayers] InputProcessor KeyUp: %s"), *Key.ToString());
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("WorldLayersInputProcessor"); }
};

TSharedPtr<FWorldLayersInputProcessor> InputProcessorInstance;

AWorldLayersDebugActor::AWorldLayersDebugActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bIsEditorOnlyActor = false;

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
	
	if (FSlateApplication::IsInitialized() && !InputProcessorInstance.IsValid())
	{
		InputProcessorInstance = MakeShareable(new FWorldLayersInputProcessor());
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessorInstance);
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Global Input Processor Registered (PostActorCreated)."));
	}

	// In Editor, we want to immediately create the widget and position
	if (GIsEditor && !GetWorld()->IsGameWorld())
	{
		InitializeActor();
	}
}

void AWorldLayersDebugActor::Destroyed()
{
	if (FSlateApplication::IsInitialized() && InputProcessorInstance.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessorInstance);
		InputProcessorInstance.Reset();
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Global Input Processor Unregistered (Destroyed)."));
	}
	Super::Destroyed();
}

void AWorldLayersDebugActor::BeginPlay()
{
	Super::BeginPlay();
	
	if (FSlateApplication::IsInitialized() && !InputProcessorInstance.IsValid())
	{
		InputProcessorInstance = MakeShareable(new FWorldLayersInputProcessor());
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessorInstance);
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Global Input Processor Registered (BeginPlay)."));
	}

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
	if (FSlateApplication::IsInitialized() && InputProcessorInstance.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessorInstance);
		InputProcessorInstance.Reset();
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Global Input Processor Unregistered (EndPlay)."));
	}
	Super::EndPlay(EndPlayReason);
}

void AWorldLayersDebugActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Sanity log to verify the actor is alive and ticking
	static float LastTickLogTime = 0.0f;
	if (GetWorld()->GetTimeSeconds() - LastTickLogTime > 5.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Debug Actor Tick: Mode=%d, Layer=%d, Pos=%s, WidgetValid=%s, ProcessorValid=%s"), 
			(int32)Current3DMode, SelectedLayerIndex, *GetActorLocation().ToString(),
			DebugWidgetInstance ? TEXT("True") : TEXT("False"), 
			InputProcessorInstance.IsValid() ? TEXT("True") : TEXT("False"));
		LastTickLogTime = GetWorld()->GetTimeSeconds();
	}

	HandleDebugInput();
	Update3DVisualization();
}

static bool bLastNumPad0Down = false;
static bool bLastNumPadDotDown = false;
static bool bLastNumPadKeysDown[10] = { false };

void AWorldLayersDebugActor::HandleDebugInput()
{
	if (!InputProcessorInstance.IsValid()) return;

	auto IsKeyDown = [&](const FKey& Key) { return InputProcessorInstance->PressedKeys.Contains(Key); };

	const bool bCtrlDown = IsKeyDown(EKeys::LeftControl) || IsKeyDown(EKeys::RightControl);
	if (!bCtrlDown) return;

	// Consolidated Cycle (NumPad 0)
	bool bNumPad0Down = IsKeyDown(EKeys::NumPadZero);
	if (bNumPad0Down && !bLastNumPad0Down)
	{
		// Unified Cycle Logic
		// States: 
		// 0: All Hidden
		// 1: UI MiniMap
		// 2: 3D Decal
		// 3: 3D Small Plane
		// 4: 3D World Plane

		static int32 CombinedMode = 0;
		CombinedMode = (CombinedMode + 1) % 5;

		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Debug Mode Cycled: NewMode=%d"), CombinedMode);

		OnDebugModeChanged(CombinedMode);

		if (UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this))
		{
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
		}

		switch (CombinedMode)
		{
		case 0: // All Hidden
			if (DebugWidgetInstance) DebugWidgetInstance->SetDebugMode(EWorldLayersDebugMode::Hidden);
			Set3DMode(EWorldLayers3DMode::None);
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Now showing: None"));
			break;
		case 1: // UI MiniMap
			if (DebugWidgetInstance) DebugWidgetInstance->SetDebugMode(EWorldLayersDebugMode::MiniMap);
			Set3DMode(EWorldLayers3DMode::None);
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Now showing: UI MiniMap"));
			break;
		case 2: // 3D Decal
			if (DebugWidgetInstance) DebugWidgetInstance->SetDebugMode(EWorldLayersDebugMode::Hidden);
			Set3DMode(EWorldLayers3DMode::Decal);
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Now showing: 3D Decal"));
			break;
		case 3: // 3D Small Plane
			if (DebugWidgetInstance) DebugWidgetInstance->SetDebugMode(EWorldLayersDebugMode::Hidden);
			Set3DMode(EWorldLayers3DMode::SmallPlane);
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Now showing: 3D Small Plane"));
			break;
		case 4: // 3D World sized Plane
			if (DebugWidgetInstance) DebugWidgetInstance->SetDebugMode(EWorldLayersDebugMode::Hidden);
			Set3DMode(EWorldLayers3DMode::WorldPlane);
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Now showing: World sized Plane"));
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
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] HandleDebugInput: NumPad %d Detected. Selecting Layer %d."), i, i-1);
			NewSelectedLayer = i - 1;
			bAnyDebugKeyPressed = true;
		}
		bLastNumPadKeysDown[i] = bKeyDown;
	}

	// FULL AUTO-REFRESH LOGIC
	if (bAnyDebugKeyPressed)
	{
		if (UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this))
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
				// Ensure it doesn't get GC'd in the editor if it's not in a viewport
				DebugWidgetInstance->SetFlags(RF_Standalone);

				FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
				TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveLevelViewport();
				if (ActiveLevelViewport.IsValid())
				{
					TSharedRef<SWidget> SlateWidget = DebugWidgetInstance->TakeWidget();
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
		if (NewNames.Num() != LayerNames.Num())
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
		static FName LastLayerName;
		static EWorldLayers3DMode LastLoggedMode = EWorldLayers3DMode::None;
		
		// We only want to update the RenderTarget and apply parameters if something CHANGED
		// or if the data is DIRTY (meaning it was updated on CPU and needs to be pushed to GPU)
		const bool bLayerChanged = (LastLayerName != TargetLayerName);
		const bool bModeChanged = (LastLoggedMode != Current3DMode);
		const bool bDataDirty = DataLayer->bIsDirty;

		if (bLayerChanged || bModeChanged || bDataDirty)
		{
			// Use Render Target if available
			if (DebugRenderTargetInstance)
			{
				Subsystem->UpdateDebugRenderTarget(TargetLayerName, DebugRenderTargetInstance);
				
				// Clear the dirty flag so we don't update every frame if nothing changed on CPU
				// Note: We use const_cast because GetDataLayer returns const pointer but we need to clear the flag
				UWorldDataLayer* MutableLayer = const_cast<UWorldDataLayer*>(DataLayer);
				MutableLayer->bIsDirty = false;
			}

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
				// Apply to the active MID
				TargetMID->SetTextureParameterValue(TEXT("Texture"), FinalTex);
				TargetMID->SetTextureParameterValue(TEXT("Diffuse"), FinalTex);
				TargetMID->SetTextureParameterValue(TEXT("BaseColor"), FinalTex);
				
				UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] 3D Viz: Applied Layer '%s' (Dirty: %s) to MID '%s' (Mode: %d)"), 
					*TargetLayerName.ToString(), bDataDirty ? TEXT("True") : TEXT("False"), 
					*TargetMID->GetName(), (int32)Current3DMode);

				if (Current3DMode == EWorldLayers3DMode::WorldPlane && (bLayerChanged || bModeChanged))
				{
					// --- DIAGNOSTIC SAMPLING START ---
					UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] DIAG: Layer Res: %dx%d"), DataLayer->Resolution.X, DataLayer->Resolution.Y);
					FIntPoint Corners[] = { 
						{0, 0}, 
						{DataLayer->Resolution.X - 1, 0}, 
						{0, DataLayer->Resolution.Y - 1}, 
						{DataLayer->Resolution.X - 1, DataLayer->Resolution.Y - 1} 
					};
					for (int32 i = 0; i < 4; ++i)
					{
						FLinearColor V = DataLayer->GetValueAtPixel(Corners[i]);
						UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] DIAG: Layer Corner %d (%d, %d) = %s"), i, Corners[i].X, Corners[i].Y, *V.ToString());
					}

					UTexture2D* Tex2D = Cast<UTexture2D>(Tex);
					if (Tex2D && Tex2D->GetPlatformData() && Tex2D->GetPlatformData()->Mips.Num() > 0)
					{
						const void* RawDataPtr = Tex2D->GetPlatformData()->Mips[0].BulkData.LockReadOnly();
						if (RawDataPtr)
						{
							int32 TW = Tex2D->GetSizeX();
							int32 TH = Tex2D->GetSizeY();
							EPixelFormat PF = Tex2D->GetPixelFormat();
							if (PF == PF_B8G8R8A8)
							{
								const FColor* ColorData = static_cast<const FColor*>(RawDataPtr);
								// Export to PNG
								TArray64<uint8> CompressedData;
								FImageView ImageView(ColorData, TW, TH);
								if (FImageUtils::CompressImage(CompressedData, TEXT("png"), ImageView))
								{
									FString Path = FPaths::ProjectSavedDir() / TEXT("Debug_WorldLayer.png");
									if (FFileHelper::SaveArrayToFile(CompressedData, *Path))
									{
										UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] DIAG: Exported texture to %s"), *Path);
									}
								}
							}
							Tex2D->GetPlatformData()->Mips[0].BulkData.Unlock();
						}
					}
					// --- DIAGNOSTIC SAMPLING END ---
				}
			}

			LastLayerName = TargetLayerName;
			LastLoggedMode = Current3DMode;
		}
		// --- OPTIMIZATION END ---
	}
}
