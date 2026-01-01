// Copyright Rancorous Games, 2025

#include "RancWorldLayersTestSetup.cpp"
#include "Framework/DebugTestResult.h"
#include "WorldLayersDebugActor.h"
#include "WorldDataVolume.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#undef TestName
#define TestName "GameTests.RancWorldLayers.3D"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancWorldLayers3DTest, TestName,
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FWorldLayers3DTestScenarios
{
public:
	FRancWorldLayers3DTest* Test;

	FWorldLayers3DTestScenarios(FRancWorldLayers3DTest* InTest)
		: Test(InTest)
	{
	}

	bool TestDebugActorSpawningAndPositioning() const
	{
		FDebugTestResult Res = true;
		
		FRancWorldLayersTestFixture Fixture(FName("3DSpawnTest"));
		UWorld* World = Fixture.GetWorld();
		AWorldDataVolume* Volume = Fixture.GetDataVolume();

		// Spawn the debug actor
		AWorldLayersDebugActor* DebugActor = World->SpawnActor<AWorldLayersDebugActor>();
		Res &= Test->TestNotNull("Debug Actor should be spawned", DebugActor);

		// Initial implementation might not position it yet, so this test might fail (Red Phase)
		FVector VolumeCenter = Volume->GetActorLocation();
		FVector ActorLocation = DebugActor->GetActorLocation();

		Res &= Test->TestEqual("Debug Actor should be at Volume center", ActorLocation, VolumeCenter);

		return Res;
	}
};

bool FRancWorldLayers3DTest::RunTest(const FString& Parameters)
{
	FWorldLayers3DTestScenarios Scenarios(this);

	bool bResult = true;
	bResult &= Scenarios.TestDebugActorSpawningAndPositioning();

	return bResult;
}

#endif // WITH_DEV_AUTOMATION_TESTS
