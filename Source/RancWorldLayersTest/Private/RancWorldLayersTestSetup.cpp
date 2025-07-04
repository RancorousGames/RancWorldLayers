#pragma once

#include "CoreMinimal.h"
#include "WorldLayersSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "RancWorldLayers/Public/WorldDataLayerAsset.h"

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
	FRancWorldLayersTestFixture(FName TestName)
	{
		World = CreateTestWorld(TestName);
		Subsystem = EnsureSubsystem(World, TestName);
		RegisterTestLayer();
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
	// ADD THIS GETTER
	UWorldDataLayerAsset* GetTestLayerAsset() const { return TestLayerAsset; }

private:
	void RegisterTestLayer()
	{
		if (!Subsystem) return;

		// The asset is now stored in our member variable
		TestLayerAsset = NewObject<UWorldDataLayerAsset>();
		TestLayerAsset->LayerName = FName("TestLayer");
		TestLayerAsset->ResolutionMode = EResolutionMode::Absolute; // Be explicit for clarity
		TestLayerAsset->Resolution = FIntPoint(100, 100);
		TestLayerAsset->DataFormat = EDataFormat::R8;
		TestLayerAsset->DefaultValue = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		TestLayerAsset->bAllowPNG_IO = true; // Ensure this is true for the test

		Subsystem->RegisterDataLayer(TestLayerAsset);
	}
	
	// ... CreateTestWorld and EnsureSubsystem functions remain the same ...

	UWorld* World = nullptr;
	UWorldLayersSubsystem* Subsystem = nullptr;
	// ADD THIS MEMBER VARIABLE
	UPROPERTY() // UPROPERTY to prevent garbage collection
	TObjectPtr<UWorldDataLayerAsset> TestLayerAsset = nullptr;
};

#endif // #if WITH_DEV_AUTOMATION_TESTS