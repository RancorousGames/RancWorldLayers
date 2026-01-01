
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

AWorldLayersDebugActor::AWorldLayersDebugActor()
{
	PrimaryActorTick.bCanEverTick = true;

	DebugMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugMesh"));
	RootComponent = DebugMesh;
	DebugMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	DebugMesh->SetCastShadow(false);
	DebugMesh->SetVisibility(false); // Default hidden

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

	if (DebugMesh->GetStaticMesh())
	{
		UMaterialInterface* BaseMat = DebugMesh->GetMaterial(0);
		if (BaseMat)
		{
			DebugMID = DebugMesh->CreateDynamicMaterialInstance(0, BaseMat);
		}
	}
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
		FVector Center = FVector(Origin.X + Size.X * 0.5f, Origin.Y + Size.Y * 0.5f, 1000.0f); // Floating at Z=1000
		SetActorLocation(Center);
		
		// Scale the plane to match the world grid size. 
		// Plane is 100x100 units by default.
		SetActorScale3D(FVector(Size.X / 100.0f, Size.Y / 100.0f, 1.0f));
	}
}

void AWorldLayersDebugActor::Update3DVisualization()
{
	if (!DebugMID || !DebugWidgetInstance || !DebugMesh->IsVisible()) return;

	// Use the texture from the widget's selected layer
	if (DebugWidgetInstance->LayerDebugImage)
	{
		UTexture* Tex = Cast<UTexture>(DebugWidgetInstance->LayerDebugImage->GetBrush().GetResourceObject());
		if (Tex)
		{
			DebugMID->SetTextureParameterValue(TEXT("Texture"), Tex);
		}
	}
}

static bool bLastNumPad0Down = false;
static bool bLastNumPadDotDown = false;
static bool bLastNumPadKeysDown[10] = { false };

#include "Framework/Application/SlateApplication.h"

void AWorldLayersDebugActor::HandleDebugInput()
{
	UWorld* World = GetWorld();
	if (!World || !DebugWidgetInstance) return;

	bool bCtrlDown = false;
	if (FSlateApplication::IsInitialized())
	{
		bCtrlDown = FSlateApplication::Get().GetModifierKeys().IsControlDown();
	}

	if (!bCtrlDown) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return; // Fallback: In Editor Viewport without PC, we can't easily check NumPad keys here.

	auto IsKeyPressed = [&](const FKey& Key)
	{
		return PC->IsInputKeyDown(Key);
	};

	// Toggle Mode (NumPad 0)
	bool bNumPad0Down = IsKeyPressed(EKeys::NumPadZero);
	if (bNumPad0Down && !bLastNumPad0Down)
	{
		int32 NextMode = ((int32)DebugWidgetInstance->CurrentMode + 1) % 3;
		DebugWidgetInstance->SetDebugMode((EWorldLayersDebugMode)NextMode);
	}
	bLastNumPad0Down = bNumPad0Down;

	// Toggle 3D Viz (NumPad .)
	bool bNumPadDotDown = IsKeyPressed(EKeys::Decimal);
	if (bNumPadDotDown && !bLastNumPadDotDown)
	{
		DebugMesh->SetVisibility(!DebugMesh->IsVisible());
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
			DebugWidgetInstance->SetSelectedLayer(i - 1);
		}
		bLastNumPadKeysDown[i] = bKeyDown;
	}
}
