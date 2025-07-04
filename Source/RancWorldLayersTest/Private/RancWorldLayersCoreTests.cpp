// Copyright Rancorous Games, 2025

#include "RancWorldLayersTestSetup.cpp"
#include "Framework/DebugTestResult.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#define TestName "GameTests.RancWorldLayers.1_CoreDataFramework"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancWorldLayersTest, TestName,
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Context class for setting up the test environment
class WorldDataLayersTestContext
{
public:
	WorldDataLayersTestContext(FRancWorldLayersTest* InTest)
		: Test(InTest),
		  TestFixture(FName(*FString(TestName)))
	{
		Subsystem = TestFixture.GetSubsystem();
		Test->TestNotNull("Subsystem should not be null", Subsystem);
	}

	UMyWorldDataSubsystem* GetSubsystem() const { return Subsystem; }
	UWorld* GetWorld() const { return TestFixture.GetWorld(); }

private:
	FRancWorldLayersTest* Test;
	FRancWorldLayersTestFixture TestFixture;
	UMyWorldDataSubsystem* Subsystem;
};

// Class containing individual test scenarios
class FWorldDataLayersTestScenarios
{
public:
	FRancWorldLayersTest* Test;

	FWorldDataLayersTestScenarios(FRancWorldLayersTest* InTest)
		: Test(InTest)
	{
	}
	
	bool TestDataLayerValueManipulation() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersTestContext Context(Test);
		UMyWorldDataSubsystem* Subsystem = Context.GetSubsystem();

		// IMPORTANT: This test assumes a UWorldDataLayerAsset named "TestLayer" exists in the Content Browser.
		// It should have ResolutionMode = Absolute, Resolution = (100,100), DataFormat = R8, DefaultValue = (0,0,0,0)
		FName TestLayerName = FName("TestLayer");

		// Test setting and getting a value
		FVector2D TestLocation1 = FVector2D(10.0f, 10.0f);
		FLinearColor TestValue1 = FLinearColor(1.0f, 0.0f, 0.0f, 0.0f); // Red for R8 format
		Subsystem->SetValueAtLocation(TestLayerName, TestLocation1, TestValue1);

		FLinearColor OutValue1;
		bool bSuccess1 = Subsystem->GetValueAtLocation(TestLayerName, TestLocation1, OutValue1);
		Res &= Test->TestTrue("GetValueAtLocation should succeed for TestLocation1", bSuccess1);
		Res &= Test->TestEqual("Value at TestLocation1 should match TestValue1", OutValue1.R, TestValue1.R, KINDA_SMALL_NUMBER);

		// Test another location and value
		FVector2D TestLocation2 = FVector2D(50.0f, 50.0f);
		FLinearColor TestValue2 = FLinearColor(0.5f, 0.0f, 0.0f, 0.0f); // Half-red for R8 format
		Subsystem->SetValueAtLocation(TestLayerName, TestLocation2, TestValue2);

		FLinearColor OutValue2;
		bool bSuccess2 = Subsystem->GetValueAtLocation(TestLayerName, TestLocation2, OutValue2);
		Res &= Test->TestTrue("GetValueAtLocation should succeed for TestLocation2", bSuccess2);

		// Account for R8 precision loss
		float ExpectedValue2 = FMath::RoundToFloat(TestValue2.R * 255.f) / 255.f;
		Res &= Test->TestEqual("Value at TestLocation2 should match TestValue2", OutValue2.R, ExpectedValue2, KINDA_SMALL_NUMBER);

		// Test a location outside the assumed 1024x1024 world bounds (which maps to 100x100 pixels)
		FVector2D OutOfBoundsLocation = FVector2D(2000.0f, 2000.0f);
		FLinearColor OutOfBoundsValue;
		bool bSuccessOutOfBounds = Subsystem->GetValueAtLocation(TestLayerName, OutOfBoundsLocation, OutOfBoundsValue);
		Res &= Test->TestTrue("GetValueAtLocation should succeed for out of bounds but return default", bSuccessOutOfBounds);
		Res &= Test->TestEqual("Value at out of bounds should be default (0)", OutOfBoundsValue.R, 0.0f, KINDA_SMALL_NUMBER);

		return Res;
	}
};

bool FRancWorldLayersTest::RunTest(const FString& Parameters)
{
	FWorldDataLayersTestScenarios Scenarios(this);

	bool bResult = true;

	bResult &= Scenarios.TestDataLayerValueManipulation();

	return bResult;
}

#endif // WITH_DEV_AUTOMATION_TESTS