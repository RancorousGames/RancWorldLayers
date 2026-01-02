
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
#include "Components/Image.h"
#include "Components/ComboBoxString.h"
#include "Components/DecalComponent.h"

AWorldLayersDebugActor::AWorldLayersDebugActor()
{
	PrimaryActorTick.bCanEverTick = true;

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

void AWorldLayersDebugActor::BeginPlay()
{
	Super::BeginPlay();
	CreateDebugWidget();
	PositionActor();
	RefreshLayerNames();

	if (DebugMesh->GetStaticMesh())
	{
		UMaterialInterface* BaseMat = DebugMesh->GetMaterial(0);
		if (BaseMat)
		{
			PlaneMID = DebugMesh->CreateDynamicMaterialInstance(0, BaseMat);
		}
	}

	if (DebugDecal->GetDecalMaterial())
	{
		DecalMID = DebugDecal->CreateDynamicMaterialInstance();
	}
	else if (PlaneMID)
	{
		// Fallback: use plane parent material for decal
		DebugDecal->SetDecalMaterial(PlaneMID->Parent);
		DecalMID = DebugDecal->CreateDynamicMaterialInstance();
	}

	Set3DMode(Current3DMode);
}

void AWorldLayersDebugActor::CreateDebugWidget()
{
	if (DebugWidgetClass && !DebugWidgetInstance)
	{
		DebugWidgetInstance = CreateWidget<UWorldLayersDebugWidget>(GetWorld(), DebugWidgetClass);
		if (DebugWidgetInstance)
		{
			DebugWidgetInstance->AddToViewport();
		}
	}
}

void AWorldLayersDebugActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	HandleDebugInput();
	Update3DVisualization();
}

void AWorldLayersDebugActor::PositionActor()
{
	UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this);
	if (!Subsystem) return;

	FVector2D Origin = Subsystem->GetWorldGridOrigin();
	FVector2D Size = Subsystem->GetWorldGridSize();

	if (Size.X > 0 && Size.Y > 0)
	{
		FVector Center = FVector(Origin.X + Size.X * 0.5f, Origin.Y + Size.Y * 0.5f, 1000.0f);
		SetActorLocation(Center);
		
		// Update Decal Size (extents)
		DebugDecal->DecalSize = FVector(2000.0f, Size.Y * 0.5f, Size.X * 0.5f);
	}
}

void AWorldLayersDebugActor::Set3DMode(EWorldLayers3DMode NewMode)
{
	Current3DMode = NewMode;

	DebugMesh->SetVisibility(false);
	DebugDecal->SetVisibility(false);

	UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this);
	FVector2D Size = Subsystem ? Subsystem->GetWorldGridSize() : FVector2D(10000.0f, 10000.0f);

	switch (NewMode)
	{
	case EWorldLayers3DMode::Decal:
		DebugDecal->SetVisibility(true);
		break;
	case EWorldLayers3DMode::SmallPlane:
		DebugMesh->SetVisibility(true);
		DebugMesh->SetWorldScale3D(FVector(10.0f, 10.0f, 1.0f)); // 1000x1000 units
		break;
	case EWorldLayers3DMode::WorldPlane:
		DebugMesh->SetVisibility(true);
		DebugMesh->SetWorldScale3D(FVector(Size.X / 100.0f, Size.Y / 100.0f, 1.0f));
		break;
	default:
		break;
	}
}

void AWorldLayersDebugActor::RefreshLayerNames()
{
	if (UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this))
	{
		LayerNames = Subsystem->GetActiveLayerNames();
	}
}

void AWorldLayersDebugActor::Update3DVisualization()
{
	if (Current3DMode == EWorldLayers3DMode::None) return;

	UMaterialInstanceDynamic* TargetMID = (Current3DMode == EWorldLayers3DMode::Decal) ? DecalMID : PlaneMID;
	if (!TargetMID) return;

	UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this);
	if (!Subsystem) return;

	RefreshLayerNames();

	if (LayerNames.Num() > 0)
	{
		SelectedLayerIndex = FMath::Clamp(SelectedLayerIndex, 0, LayerNames.Num() - 1);
		FName TargetLayerName = LayerNames[SelectedLayerIndex];

		UTexture* Tex = Subsystem->GetLayerGpuTexture(TargetLayerName);
		if (Tex)
		{
			TargetMID->SetTextureParameterValue(TEXT("Texture"), Tex);
		}
	}
}

static bool bLastNumPad0Down = false;
static bool bLastNumPadDotDown = false;
static bool bLastNumPadKeysDown[10] = { false };

#include "Framework/Application/SlateApplication.h"
#include "Components/ComboBoxString.h"

void AWorldLayersDebugActor::HandleDebugInput()
{
	UWorld* World = GetWorld();
	if (!World) return;

	bool bCtrlDown = false;
	if (FSlateApplication::IsInitialized())
	{
		bCtrlDown = FSlateApplication::Get().GetModifierKeys().IsControlDown();
	}

	if (!bCtrlDown) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return; 

	auto IsKeyPressed = [&](const FKey& Key)
	{
		return PC->IsInputKeyDown(Key);
	};

	// Toggle Mode (NumPad 0)
	bool bNumPad0Down = IsKeyPressed(EKeys::NumPadZero);
	if (bNumPad0Down && !bLastNumPad0Down && DebugWidgetInstance)
	{
		int32 NextMode = ((int32)DebugWidgetInstance->CurrentMode + 1) % 3;
		DebugWidgetInstance->SetDebugMode((EWorldLayersDebugMode)NextMode);
	}
	bLastNumPad0Down = bNumPad0Down;

	// Toggle 3D Viz (NumPad .) - Cycle through 4 modes
	bool bNumPadDotDown = IsKeyPressed(EKeys::Decimal);
	if (bNumPadDotDown && !bLastNumPadDotDown)
	{
		int32 Next3DMode = ((int32)Current3DMode + 1) % 4;
		Set3DMode((EWorldLayers3DMode)Next3DMode);
	}
	bLastNumPadDotDown = bNumPadDotDown;

	// Select Layer (NumPad 1-9)
	FKey NumKeys[] = { 
		EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, 
		EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, 
		EKeys::NumPadEight, EKeys::NumPadNine 
	};

	for (int32 i = 1; i <= 9; ++i)
	{
		bool bKeyDown = IsKeyPressed(NumKeys[i]);
		if (bKeyDown && !bLastNumPadKeysDown[i])
		{
			SelectedLayerIndex = i - 1;
			if (DebugWidgetInstance)
			{
				DebugWidgetInstance->SetSelectedLayer(SelectedLayerIndex);
			}
		}
		bLastNumPadKeysDown[i] = bKeyDown;
	}
}
