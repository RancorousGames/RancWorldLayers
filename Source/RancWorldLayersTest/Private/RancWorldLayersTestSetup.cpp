#pragma once

#include "CoreMinimal.h"
#include "WorldLayersSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "WorldDataLayerAsset.h"
#include "WorldDataVolume.h"
#include "Engine/BrushBuilder.h"

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
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("World is null in EnsureSubsystem."));
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		GameInstance = NewObject<UGameInstance>(World);
		World->SetGameInstance(GameInstance);
		GameInstance->Init();
	}

	return GameInstance->GetSubsystem<UWorldLayersSubsystem>();
}

// Test fixture that sets up the persistent test world and subsystem
class FRancWorldLayersTestFixture
{
public:
	FRancWorldLayersTestFixture(FName TestName, FVector2D InWorldBounds = FVector2D(10000.0f, 10000.0f))
	{
		World = CreateTestWorld(TestName);
		
		// Spawn and configure the AWorldDataVolume, which now drives the subsystem
		DataVolume = World->SpawnActor<AWorldDataVolume>();
		DataVolume->SetActorLocation(FVector(InWorldBounds.X / 2.0, InWorldBounds.Y / 2.0, 0));
		DataVolume->SetActorScale3D(FVector(InWorldBounds.X / 100.0, InWorldBounds.Y / 100.0, 100.0)); // Scale is in brush units (200cm)
		DataVolume->BrushBuilder->Build(World, DataVolume);

		// Get the subsystem, which will now initialize from the volume
		Subsystem = EnsureSubsystem(World, TestName);
		
		// Now create and register the test layer via the volume
		CreateAndRegisterTestLayer();
	}

	~FRancWorldLayersTestFixture()
	{
		// Clean up if needed; note that in many tests the engine will tear down the world.
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
	void CreateAndRegisterTestLayer()
	{
		if (!Subsystem || !DataVolume) return;

		// The asset is now stored in our member variable
		TestLayerAsset = NewObject<UWorldDataLayerAsset>();
		TestLayerAsset->LayerName = FName("TestLayer");
		TestLayerAsset->ResolutionMode = EResolutionMode::Absolute;
		TestLayerAsset->Resolution = FIntPoint(100, 100);
		TestLayerAsset->DataFormat = EDataFormat::R8;
		TestLayerAsset->DefaultValue = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		TestLayerAsset->bAllowPNG_IO = true;

		// Add the asset to the volume's list and re-initialize the subsystem
		DataVolume->LayerAssets.Add(TestLayerAsset);
		Subsystem->RegisterDataLayer(TestLayerAsset);
	}
	
	UWorld* World = nullptr;
	UWorldLayersSubsystem* Subsystem = nullptr;
	AWorldDataVolume* DataVolume = nullptr;
	UPROPERTY() // UPROPERTY to prevent garbage collection
	TObjectPtr<UWorldDataLayerAsset> TestLayerAsset = nullptr;
};

#endif // #if WITH_DEV_AUTOMATION_TESTS