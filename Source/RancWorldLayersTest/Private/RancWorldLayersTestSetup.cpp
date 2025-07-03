#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "RancWorldLayers/Public/UMyWorldDataSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

// Test fixture that sets up the persistent test world and subsystem
class FRancWorldLayersTestFixture
{
public:
	FRancWorldLayersTestFixture(FName TestName)
	{
		World = CreateTestWorld(TestName);
		Subsystem = EnsureSubsystem(World, TestName);
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
	UMyWorldDataSubsystem* GetSubsystem() const { return Subsystem; }

private:
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

	static UMyWorldDataSubsystem* EnsureSubsystem(UWorld* World, const FName& TestName)
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

		return GameInstance->GetSubsystem<UMyWorldDataSubsystem>();
	}


	UWorld* World = nullptr;
	UMyWorldDataSubsystem* Subsystem = nullptr;

};

#endif // #if WITH_DEV_AUTOMATION_TESTS