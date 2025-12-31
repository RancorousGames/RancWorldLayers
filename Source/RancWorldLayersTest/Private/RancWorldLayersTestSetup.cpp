#pragma once

#include "CoreMinimal.h"
#include "WorldLayersSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "WorldDataLayerAsset.h"
#include "WorldDataVolume.h"
#include "Engine/BrushBuilder.h"
#include "Builders/CubeBuilder.h" // <-- FIX: Add this include for the concrete builder
#include "Components/BrushComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

static UWorld* CreateTestWorld(const FName& WorldName)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName);
	if (GEngine)
	{
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.SetCurrentWorld(TestWorld);
	}

	AWorldSettings* WorldSettings = TestWorld->GetWorldSettings();
	if (WorldSettings)
	{
		WorldSettings->SetActorTickEnabled(true);
	}

	if (!TestWorld->bIsWorldInitialized)
	{
		TestWorld->InitializeNewWorld(UWorld::InitializationValues()
									  .ShouldSimulatePhysics(false)
									  .AllowAudioPlayback(false)
									  .RequiresHitProxies(false)
									  .CreatePhysicsScene(true)
									  .CreateNavigation(false)
									  .CreateAISystem(false));
	}

	TestWorld->InitializeActorsForPlay(FURL());

	return TestWorld;
}

static UWorldLayersSubsystem* EnsureSubsystem(UWorld* World, const FName& TestName)
{
	if (!World) return nullptr;
	return World->GetSubsystem<UWorldLayersSubsystem>();
}

// Test fixture that sets up the persistent test world and subsystem
class FRancWorldLayersTestFixture
{
public:
	FRancWorldLayersTestFixture(FName TestName, FVector2D InWorldBounds = FVector2D(10000.0f, 10000.0f))
	{
		World = CreateTestWorld(TestName);
		
		DataVolume = World->SpawnActor<AWorldDataVolume>();
		
		DataVolume->SetActorLocation(FVector::ZeroVector);

		DataVolume->GetBrushComponent()->SetWorldScale3D(FVector(InWorldBounds.X / 100.0f, InWorldBounds.Y / 100.0f, 1.0f));
		
		// FIX: Reorder initialization. Get the subsystem first.
		Subsystem = EnsureSubsystem(World, TestName);
		
		// Initialize the subsystem's world bounds from the volume. No layers will be loaded here, which is correct for our test setup.
		Subsystem->InitializeFromVolume(DataVolume);

		// FIX: Now that the subsystem is valid, create and register the default test layer.
		CreateTestLayerAsset();
	}

	~FRancWorldLayersTestFixture()
	{
		if (World)
		{
			World->DestroyWorld(false);
		}
	}

	UWorld* GetWorld() const { return World; }
	UWorldLayersSubsystem* GetSubsystem() const { return Subsystem; }
	UWorldDataLayerAsset* GetTestLayerAsset() const { return TestLayerAsset; }
	AWorldDataVolume* GetDataVolume() const { return DataVolume; }

private:
	void CreateTestLayerAsset()
	{
		// FIX: The guard clause now works correctly as Subsystem is initialized.
		if (!Subsystem) return;

		TestLayerAsset = NewObject<UWorldDataLayerAsset>();
		TestLayerAsset->LayerName = FName("TestLayer");
		TestLayerAsset->ResolutionMode = EResolutionMode::Absolute;
		TestLayerAsset->Resolution = FIntPoint(100, 100);
		TestLayerAsset->DataFormat = EDataFormat::R8;
		TestLayerAsset->DefaultValue = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		TestLayerAsset->bAllowPNG_IO = true;

		// FIX: Directly register the transient asset. Do not add it to the Volume's TSoftObjectPtr array.
		// This ensures the WorldDataLayers map is populated for the tests.
		Subsystem->RegisterDataLayer(TestLayerAsset);
	}
	
	UWorld* World = nullptr;
	UWorldLayersSubsystem* Subsystem = nullptr;
	AWorldDataVolume* DataVolume = nullptr;
	UPROPERTY()
	TObjectPtr<UWorldDataLayerAsset> TestLayerAsset = nullptr;
};

#endif // #if WITH_DEV_AUTOMATION_TESTS