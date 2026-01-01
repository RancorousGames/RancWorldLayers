// Copyright Rancorous Games, 2025

#include "RancWorldLayersTestSetup.cpp"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/DebugTestResult.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#undef TestName
#define TestName "GameTests.RancWorldLayers.Core"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancWorldLayersCoreTest, TestName,
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Context class for setting up the test environment
class WorldDataLayersCoreTestContext
{
public:
	WorldDataLayersCoreTestContext(FRancWorldLayersCoreTest* InTest)
		: Test(InTest),
		  TestFixture(FName(*FString(TestName)))
	{
		Subsystem = TestFixture.GetSubsystem();
		Test->TestNotNull("Subsystem should not be null", Subsystem);
	}

	UWorldLayersSubsystem* GetSubsystem() const { return Subsystem; }
	UWorld* GetWorld() const { return TestFixture.GetWorld(); }

private:
	FRancWorldLayersCoreTest* Test;
	FRancWorldLayersTestFixture TestFixture;
	UWorldLayersSubsystem* Subsystem;
};

// Class containing individual test scenarios
class FWorldDataLayersCoreTestScenarios
{
public:
	FRancWorldLayersCoreTest* Test;

	FWorldDataLayersCoreTestScenarios(FRancWorldLayersCoreTest* InTest)
		: Test(InTest)
	{
	}
	
	bool TestDataLayerValueManipulation() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersCoreTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();

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
		FVector2D OutOfBoundsLocation = FVector2D(20000.0f, 20000.0f);
		FLinearColor OutOfBoundsValue;
		bool bSuccessOutOfBounds = Subsystem->GetValueAtLocation(TestLayerName, OutOfBoundsLocation, OutOfBoundsValue);
		Res &= Test->TestTrue("GetValueAtLocation should succeed for out of bounds but return default", bSuccessOutOfBounds);
		Res &= Test->TestEqual("Value at out of bounds should be default (0)", OutOfBoundsValue.R, 0.0f, KINDA_SMALL_NUMBER);

		return Res;
	}

	bool TestNonExistentLayerQuery() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersCoreTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();

		FName NonExistentLayerName = FName("NonExistentLayer");
		FVector2D TestLocation = FVector2D(10.0f, 10.0f);
		FLinearColor OutValue;

		bool bSuccess = Subsystem->GetValueAtLocation(NonExistentLayerName, TestLocation, OutValue);
		Res &= Test->TestFalse("GetValueAtLocation should fail for non-existent layer", bSuccess);
		Res &= Test->TestEqual("Value for non-existent layer should be default (0)", OutValue.R, 0.0f, KINDA_SMALL_NUMBER);
		Res &= Test->TestEqual("Value for non-existent layer should be default (0)", OutValue.G, 0.0f, KINDA_SMALL_NUMBER);
		Res &= Test->TestEqual("Value for non-existent layer should be default (0)", OutValue.B, 0.0f, KINDA_SMALL_NUMBER);
		Res &= Test->TestEqual("Value for non-existent layer should be default (0)", OutValue.A, 0.0f, KINDA_SMALL_NUMBER);

		return Res;
	}

	bool TestGetFloatValueAtLocationNonExistentLayer() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersCoreTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();

		FName NonExistentLayerName = FName("AnotherNonExistentLayer");
		FVector2D TestLocation = FVector2D(20.0f, 20.0f);

		float OutFloatValue = Subsystem->GetFloatValueAtLocation(NonExistentLayerName, TestLocation);
		Res &= Test->TestEqual("Float value for non-existent layer should be 0.0f", OutFloatValue, 0.0f, KINDA_SMALL_NUMBER);

		return Res;
	}

	bool TestSetValueAtLocationNonExistentLayer() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersCoreTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();

		FName NonExistentLayerName = FName("YetAnotherNonExistentLayer");
		FVector2D TestLocation = FVector2D(30.0f, 30.0f);
		FLinearColor TestValue = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); // White

		// Attempt to set a value on a non-existent layer. This should ideally do nothing and not crash.
		Subsystem->SetValueAtLocation(NonExistentLayerName, TestLocation, TestValue);

		// Verify that GetValueAtLocation still returns default for this non-existent layer
		FLinearColor OutValue;
		bool bSuccess = Subsystem->GetValueAtLocation(NonExistentLayerName, TestLocation, OutValue);
		Res &= Test->TestFalse("GetValueAtLocation should still fail for non-existent layer after attempted set", bSuccess);
		Res &= Test->TestEqual("Value for non-existent layer should remain default (0)", OutValue.R, 0.0f, KINDA_SMALL_NUMBER);
		Res &= Test->TestEqual("Value for non-existent layer should remain default (0)", OutValue.G, 0.0f, KINDA_SMALL_NUMBER);
		Res &= Test->TestEqual("Value for non-existent layer should remain default (0)", OutValue.B, 0.0f, KINDA_SMALL_NUMBER);
		Res &= Test->TestEqual("Value for non-existent layer should remain default (0)", OutValue.A, 0.0f, KINDA_SMALL_NUMBER);

		return Res;
	}

	bool TestMultipleVolumeWarning() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersCoreTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();
		UWorld* World = Context.GetWorld();

		// Create a second volume
		AWorldDataVolume* SecondVolume = World->SpawnActor<AWorldDataVolume>();
		
		// Expect a warning when initializing from a second volume
		Test->AddExpectedError(TEXT("already has a registered WorldDataVolume"), EAutomationExpectedErrorFlags::Contains, 1);
		
		Subsystem->InitializeFromVolume(SecondVolume);

		return Res;
	}

	bool TestOutOfBoundsDefaultValue() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersCoreTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();

		// Create a layer with a specific default value
		UWorldDataLayerAsset* DefaultLayerAsset = NewObject<UWorldDataLayerAsset>();
		DefaultLayerAsset->LayerName = FName("DefaultValueLayer");
		DefaultLayerAsset->ResolutionMode = EResolutionMode::Absolute;
		DefaultLayerAsset->Resolution = FIntPoint(10, 10);
		DefaultLayerAsset->DataFormat = EDataFormat::R8;
		DefaultLayerAsset->DefaultValue = FLinearColor(0.75f, 0.0f, 0.0f, 0.0f);

		Subsystem->RegisterDataLayer(DefaultLayerAsset);

		// Query location far outside world bounds (Fixture default is 10000x10000 centered at 0,0)
		FVector2D WayOutOfBounds = FVector2D(50000.0f, 50000.0f);
		FLinearColor OutValue;
		bool bSuccess = Subsystem->GetValueAtLocation(FName("DefaultValueLayer"), WayOutOfBounds, OutValue);

		Res &= Test->TestTrue("GetValueAtLocation should succeed for out of bounds", bSuccess);
		Res &= Test->TestEqual("Out of bounds value should be the layer's default value", OutValue.R, 0.75f, KINDA_SMALL_NUMBER);

		return Res;
	}

	bool TestImmutableLayerWrite() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersCoreTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();

		// Create an immutable layer
		UWorldDataLayerAsset* ImmutableLayerAsset = NewObject<UWorldDataLayerAsset>();
		ImmutableLayerAsset->LayerName = FName("ImmutableLayer");
		ImmutableLayerAsset->ResolutionMode = EResolutionMode::Absolute;
		ImmutableLayerAsset->Resolution = FIntPoint(10, 10);
		ImmutableLayerAsset->DataFormat = EDataFormat::R8;
		ImmutableLayerAsset->Mutability = EWorldDataLayerMutability::Immutable;
		ImmutableLayerAsset->DefaultValue = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

		Subsystem->RegisterDataLayer(ImmutableLayerAsset);

		// Expect a warning when trying to write
		Test->AddExpectedError(TEXT("Attempting to write to immutable layer"), EAutomationExpectedErrorFlags::Contains, 1);
		
		Subsystem->SetValueAtLocation(FName("ImmutableLayer"), FVector2D(0.0f, 0.0f), FLinearColor::White);

		// Verify it didn't change
		FLinearColor OutValue;
		Subsystem->GetValueAtLocation(FName("ImmutableLayer"), FVector2D(0.0f, 0.0f), OutValue);
		Res &= Test->TestEqual("Immutable layer value should remain 0", OutValue.R, 0.0f, KINDA_SMALL_NUMBER);

		return Res;
	}
};

bool FRancWorldLayersCoreTest::RunTest(const FString& Parameters)
{
	FWorldDataLayersCoreTestScenarios Scenarios(this);

	bool bResult = true;

	bResult &= Scenarios.TestDataLayerValueManipulation();
	bResult &= Scenarios.TestNonExistentLayerQuery();
	bResult &= Scenarios.TestGetFloatValueAtLocationNonExistentLayer();
	bResult &= Scenarios.TestSetValueAtLocationNonExistentLayer();
	bResult &= Scenarios.TestMultipleVolumeWarning();
	bResult &= Scenarios.TestOutOfBoundsDefaultValue();
	bResult &= Scenarios.TestImmutableLayerWrite();

	return bResult;
}

#endif // WITH_DEV_AUTOMATION_TESTS