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

		// Create a new layer asset specifically for spatial testing
		UWorldDataLayerAsset* SpatialTestLayerAsset = NewObject<UWorldDataLayerAsset>();
		SpatialTestLayerAsset->LayerName = FName("SpatialTestLayer");
		SpatialTestLayerAsset->ResolutionMode = EResolutionMode::Absolute;
		SpatialTestLayerAsset->Resolution = FIntPoint(100, 100);
		SpatialTestLayerAsset->DataFormat = EDataFormat::R8;
		SpatialTestLayerAsset->DefaultValue = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		SpatialTestLayerAsset->SpatialOptimization.bBuildAccelerationStructure = true;
		SpatialTestLayerAsset->SpatialOptimization.ValuesToTrack.Add(FLinearColor(1.0f, 0.0f, 0.0f, 0.0f)); // Track red
		Subsystem->RegisterDataLayer(SpatialTestLayerAsset);

		FName TestLayerName = SpatialTestLayerAsset->LayerName;

		// --- Setup ---
		// The test fixture creates a 100x100 world centered at (0,0), so the valid world bounds are [-50, 50].
		// The WorldGridOrigin is therefore (-50, -50). Cell size is 1 world unit per pixel.
		const FVector2D Point1_World(-40.0f, -40.0f); // -> Relative (10, 10) -> Pixel (10, 10)
		const FVector2D Point2_World(0.0f, 0.0f);     // -> Relative (50, 50) -> Pixel (50, 50)
		const FVector2D Point3_World(40.0f, 40.0f);   // -> Relative (90, 90) -> Pixel (90, 90)
		Subsystem->SetValueAtLocation(TestLayerName, Point1_World, FLinearColor(1.0f, 0.0f, 0.0f, 0.0f));
		Subsystem->SetValueAtLocation(TestLayerName, Point2_World, FLinearColor(1.0f, 0.0f, 0.0f, 0.0f));
		Subsystem->SetValueAtLocation(TestLayerName, Point3_World, FLinearColor(1.0f, 0.0f, 0.0f, 0.0f));

		// Calculate expected return locations (center of the pixel in world space)
		// Formula: World.X = (Pixel.X + 0.5) * CellSize.X + Origin.X
		const FVector2D ExpectedLocation1(-39.5f, -39.5f); // (10.5 * 1) - 50
		const FVector2D ExpectedLocation2(0.5f, 0.5f);       // (50.5 * 1) - 50
		const FVector2D ExpectedLocation3(40.5f, 40.5f);     // (90.5 * 1) - 50

		FVector2D FoundLocation;
		bool bFound;

		// --- Test 1: Find the point nearest to the origin ---
		bFound = Subsystem->FindNearestPointWithValue(TestLayerName, FVector2D(1.0f, 1.0f), 20.0f, FLinearColor(1.0f, 0.0f, 0.0f, 0.0f), FoundLocation);
		Res &= Test->TestTrue("Should find a point near (1,1) world space", bFound);
		Res &= Test->TestTrue("Found location should be the center of the pixel at (0,0) world space", FoundLocation.Equals(ExpectedLocation2, 0.1f));

		// --- Test 2: Find the point in the negative quadrant ---
		bFound = Subsystem->FindNearestPointWithValue(TestLayerName, FVector2D(-38.0f, -38.0f), 10.0f, FLinearColor(1.0f, 0.0f, 0.0f, 0.0f), FoundLocation);
		Res &= Test->TestTrue("Should find a point near (-38,-38) world space", bFound);
		Res &= Test->TestTrue("Found location should be the center of the pixel at (-40,-40) world space", FoundLocation.Equals(ExpectedLocation1, 0.1f));

		// --- Test 3: Search for a value that is not being tracked ---
		bFound = Subsystem->FindNearestPointWithValue(TestLayerName, FVector2D(0.0f, 0.0f), 100.0f, FLinearColor(0.5f, 0.0f, 0.0f, 0.0f), FoundLocation);
		Res &= Test->TestFalse("Should not find a value that is not tracked", bFound);

		// --- Test 4: Search for a point that is outside the MaxSearchRadius ---
		bFound = Subsystem->FindNearestPointWithValue(TestLayerName, FVector2D(-45.0f, -45.0f), 2.0f, FLinearColor(1.0f, 0.0f, 0.0f, 0.0f), FoundLocation);
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