// Copyright Rancorous Games, 2025

#include "RancWorldLayersTestSetup.cpp"
#include "Framework/DebugTestResult.h"
#include "WorldDataLayerAsset.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#define TestName_Spatial "GameTests.RancWorldLayers.Spatial"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancWorldLayersSpatialTest, TestName_Spatial,
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Context class for setting up the test environment
class WorldDataLayersSpatialTestContext
{
public:
	WorldDataLayersSpatialTestContext(FRancWorldLayersSpatialTest* InTest)
		: Test(InTest),
		  TestFixture(FName(*FString(TestName_Spatial)), FVector2D(100.0f, 100.0f)) // Set WorldBounds to match Resolution
	{
		Subsystem = TestFixture.GetSubsystem();
		Test->TestNotNull("Subsystem should not be null", Subsystem);
	}

	UWorldLayersSubsystem* GetSubsystem() const { return Subsystem; }
	UWorld* GetWorld() const { return TestFixture.GetWorld(); }
	FRancWorldLayersTestFixture* GetTestFixture() { return &TestFixture; }

private:
	FRancWorldLayersSpatialTest* Test;
	FRancWorldLayersTestFixture TestFixture;
	UWorldLayersSubsystem* Subsystem;
};

// Class containing individual test scenarios
class FWorldDataLayersSpatialTestScenarios
{
public:
	FRancWorldLayersSpatialTest* Test;

	FWorldDataLayersSpatialTestScenarios(FRancWorldLayersSpatialTest* InTest)
		: Test(InTest)
	{
	}

	bool TestFindNearestPointWithValue() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersSpatialTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();
		FRancWorldLayersTestFixture* TestFixture = Context.GetTestFixture();

		// Create a new layer asset specifically for spatial testing
		UWorldDataLayerAsset* SpatialTestLayerAsset = NewObject<UWorldDataLayerAsset>();
		SpatialTestLayerAsset->LayerName = FName("SpatialTestLayer");
		SpatialTestLayerAsset->ResolutionMode = EResolutionMode::Absolute;
		SpatialTestLayerAsset->Resolution = FIntPoint(100, 100);
		SpatialTestLayerAsset->DataFormat = EDataFormat::R8;
		SpatialTestLayerAsset->DefaultValue = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		SpatialTestLayerAsset->SpatialOptimization.bBuildAccelerationStructure = true;
		SpatialTestLayerAsset->SpatialOptimization.ValuesToTrack.Add(FLinearColor(1.0f, 0.0f, 0.0f, 0.0f)); // Track red
				SpatialTestLayerAsset->WorldGridOrigin = FVector2D(0.0f, 0.0f); // Origin at (0,0)
		SpatialTestLayerAsset->WorldGridSize = FVector2D(100.0f, 100.0f); // World size matches pixel size
		Subsystem->RegisterDataLayer(SpatialTestLayerAsset);

		FName TestLayerName = SpatialTestLayerAsset->LayerName;

		// Set some points with the tracked value
		Subsystem->SetValueAtLocation(TestLayerName, FVector2D(10.0f, 10.0f), FLinearColor(1.0f, 0.0f, 0.0f, 0.0f));
		Subsystem->SetValueAtLocation(TestLayerName, FVector2D(50.0f, 50.0f), FLinearColor(1.0f, 0.0f, 0.0f, 0.0f));
		Subsystem->SetValueAtLocation(TestLayerName, FVector2D(90.0f, 90.0f), FLinearColor(1.0f, 0.0f, 0.0f, 0.0f));

		FVector2D FoundLocation;
		bool bFound;

		// Test 1: Search near (10,10)
		bFound = Subsystem->FindNearestPointWithValue(TestLayerName, FVector2D(10.0f, 10.0f), 1.0f, FLinearColor(1.0f, 0.0f, 0.0f, 0.0f), FoundLocation);
		Res &= Test->TestTrue("Should find a point near (10,10)", bFound);
		Res &= Test->TestEqual("Found point X should be 10", (float)FoundLocation.X, 10.0f, KINDA_SMALL_NUMBER);
		Res &= Test->TestEqual("Found point Y should be 10", (float)FoundLocation.Y, 10.0f, KINDA_SMALL_NUMBER);

		// Test 2: Search near (50,50)
		bFound = Subsystem->FindNearestPointWithValue(TestLayerName, FVector2D(50.0f, 50.0f), 1.0f, FLinearColor(1.0f, 0.0f, 0.0f, 0.0f), FoundLocation);
		Res &= Test->TestTrue("Should find a point near (50,50)", bFound);
		Res &= Test->TestEqual("Found point X should be 50", (float)FoundLocation.X, 50.0f, KINDA_SMALL_NUMBER);
		Res &= Test->TestEqual("Found point Y should be 50", (float)FoundLocation.Y, 50.0f, KINDA_SMALL_NUMBER);

		// Test 3: Search for a non-existent value
		bFound = Subsystem->FindNearestPointWithValue(TestLayerName, FVector2D(50.0f, 50.0f), 1.0f, FLinearColor(0.5f, 0.0f, 0.0f, 0.0f), FoundLocation);
		Res &= Test->TestFalse("Should not find a non-existent value", bFound);

		// Test 4: Search outside MaxSearchRadius
		bFound = Subsystem->FindNearestPointWithValue(TestLayerName, FVector2D(1.0f, 1.0f), 0.5f, FLinearColor(1.0f, 0.0f, 0.0f, 0.0f), FoundLocation);
		Res &= Test->TestFalse("Should not find a point outside MaxSearchRadius", bFound);
		return Res;
	}
};

bool FRancWorldLayersSpatialTest::RunTest(const FString& Parameters)
{
	FWorldDataLayersSpatialTestScenarios Scenarios(this);

	bool bResult = true;

	bResult &= Scenarios.TestFindNearestPointWithValue();

	return bResult;
}

#endif // WITH_DEV_AUTOMATION_TESTS
