// Copyright Rancorous Games, 2025

#include "RancWorldLayersTestSetup.cpp"
#include "Framework/DebugTestResult.h"
#include "WorldLayersDebugActor.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#undef TestName
#define TestName "GameTests.RancWorldLayers.Structure"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancWorldLayersStructureTest, TestName,
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FWorldLayersStructureTestScenarios
{
public:
	FRancWorldLayersStructureTest* Test;

	FWorldLayersStructureTestScenarios(FRancWorldLayersStructureTest* InTest)
		: Test(InTest)
	{
	}

	bool TestDebugActorComponents() const
	{
		FDebugTestResult Res = true;
		
		FRancWorldLayersTestFixture Fixture(FName("StructureTest"));
		UWorld* World = Fixture.GetWorld();

		AWorldLayersDebugActor* DebugActor = World->SpawnActor<AWorldLayersDebugActor>();
		Res &= Test->TestNotNull("Debug Actor should be spawned", DebugActor);

		// Check Components using FindComponentByClass (since properties are protected/private or we want to verify runtime existence)
		UStaticMeshComponent* MeshComp = DebugActor->FindComponentByClass<UStaticMeshComponent>();
		Res &= Test->TestNotNull("Debug Actor should have a StaticMeshComponent", MeshComp);

		UDecalComponent* DecalComp = DebugActor->FindComponentByClass<UDecalComponent>();
		Res &= Test->TestNotNull("Debug Actor should have a DecalComponent", DecalComp);

		if (MeshComp)
		{
			Res &= Test->TestTrue("Debug Mesh should be invisible by default", !MeshComp->IsVisible());
			// Check if plane mesh was loaded (might fail if Engine Content is missing, but usually present)
			if (MeshComp->GetStaticMesh())
			{
				Res &= Test->TestEqual("Mesh should be Plane", MeshComp->GetStaticMesh()->GetName(), FString("Plane"));
			}
		}

		if (DecalComp)
		{
			Res &= Test->TestTrue("Debug Decal should be invisible by default", !DecalComp->IsVisible());
			Res &= Test->TestEqual("Decal rotation should be -90 pitch", DecalComp->GetRelativeRotation().Pitch, -90.0);
		}

		return Res;
	}
};

bool FRancWorldLayersStructureTest::RunTest(const FString& Parameters)
{
	FWorldLayersStructureTestScenarios Scenarios(this);

	bool bResult = true;
	bResult &= Scenarios.TestDebugActorComponents();

	return bResult;
}

#endif // WITH_DEV_AUTOMATION_TESTS
