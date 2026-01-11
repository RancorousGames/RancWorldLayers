// Copyright Rancorous Games, 2025

#include "RancWorldLayersTestSetup.cpp"
#include "Framework/DebugTestResult.h"
#include "WorldDataLayerAsset.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#define TestName_Sampling "GameTests.RancWorldLayers.Sampling"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancWorldLayersSamplingTest, TestName_Sampling,
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Context class for setting up the test environment
class WorldDataLayersSamplingTestContext
{
public:
	WorldDataLayersSamplingTestContext(FRancWorldLayersSamplingTest* InTest)
		: Test(InTest),
		  TestFixture(FName(*FString(TestName_Sampling)), FVector2D(100.0f, 100.0f))
	{
		Subsystem = TestFixture.GetSubsystem();
		Test->TestNotNull("Subsystem should not be null", Subsystem);
	}

	UWorldLayersSubsystem* GetSubsystem() const { return Subsystem; }
	UWorld* GetWorld() const { return TestFixture.GetWorld(); }
	FRancWorldLayersTestFixture* GetTestFixture() { return &TestFixture; }

private:
	FRancWorldLayersSamplingTest* Test;
	FRancWorldLayersTestFixture TestFixture;
	UWorldLayersSubsystem* Subsystem;
};

// Class containing individual test scenarios
class FWorldDataLayersSamplingTestScenarios
{
public:
	FRancWorldLayersSamplingTest* Test;

	FWorldDataLayersSamplingTestScenarios(FRancWorldLayersSamplingTest* InTest)
		: Test(InTest)
	{
	}

	bool TestBilinearInterpolation() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersSamplingTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();

		// Create a new layer asset specifically for sampling testing
		UWorldDataLayerAsset* SamplingTestLayerAsset = NewObject<UWorldDataLayerAsset>();
		SamplingTestLayerAsset->LayerName = FName("SamplingTestLayer");
		SamplingTestLayerAsset->ResolutionMode = EResolutionMode::Absolute;
		SamplingTestLayerAsset->Resolution = FIntPoint(10, 10);
		SamplingTestLayerAsset->DataFormat = EDataFormat::R16F; // Use high precision float
		SamplingTestLayerAsset->DefaultValue = FLinearColor::Black;
		Subsystem->RegisterDataLayer(SamplingTestLayerAsset);

		FName TestLayerName = SamplingTestLayerAsset->LayerName;

		// --- Setup ---
		// The test fixture creates a 100x100 world centered at (0,0), so the valid world bounds are [-50, 50].
		// The WorldGridOrigin is therefore (-50, -50). Cell size is 10 world units per pixel (100/10).
		
		// Set values at four neighboring pixel centers
		// Pixel (4,4) center: World (-50 + 4.5*10, -50 + 4.5*10) = (-5, -5)
		// Pixel (5,4) center: World (5, -5)
		// Pixel (4,5) center: World (-5, 5)
		// Pixel (5,5) center: World (5, 5)
		
		Subsystem->SetValueAtLocation(TestLayerName, FVector2D(-5.0f, -5.0f), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)); // V00
		Subsystem->SetValueAtLocation(TestLayerName, FVector2D(5.0f, -5.0f), FLinearColor(1.0f, 0.0f, 0.0f, 0.0f));  // V10
		Subsystem->SetValueAtLocation(TestLayerName, FVector2D(-5.0f, 5.0f), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));  // V01
		Subsystem->SetValueAtLocation(TestLayerName, FVector2D(5.0f, 5.0f), FLinearColor(1.0f, 0.0f, 0.0f, 0.0f));   // V11

		// Query at the midpoint between centers: (0, 0)
		// FracX should be 0.5, FracY should be 0.5
		// Expected result: BiLerp(0, 1, 0, 1, 0.5, 0.5) = Lerp(Lerp(0, 1, 0.5), Lerp(0, 1, 0.5), 0.5) = Lerp(0.5, 0.5, 0.5) = 0.5
		
		FLinearColor OutValue;
		bool bSuccess = Subsystem->GetValueAtLocationInterpolated(TestLayerName, FVector2D(0.0f, 0.0f), OutValue);
		Res &= Test->TestTrue("GetValueAtLocationInterpolated should succeed", bSuccess);
		Res &= Test->TestEqual("Midpoint value should be interpolated correctly", OutValue.R, 0.5f, KINDA_SMALL_NUMBER);

		// Query at 1/4 point: (-2.5, -2.5)
		// PixelX = (-2.5 - (-50)) / 10 - 0.5 = 47.5 / 10 - 0.5 = 4.75 - 0.5 = 4.25
		// PixelY = 4.25
		// X0 = 4, Y0 = 4, FracX = 0.25, FracY = 0.25
		// BiLerp(0, 1, 0, 1, 0.25, 0.25) = Lerp(Lerp(0, 1, 0.25), Lerp(0, 1, 0.25), 0.25) = Lerp(0.25, 0.25, 0.25) = 0.25
		
		bSuccess = Subsystem->GetValueAtLocationInterpolated(TestLayerName, FVector2D(-2.5f, -2.5f), OutValue);
		Res &= Test->TestEqual("Quarter-point value should be interpolated correctly", OutValue.R, 0.25f, KINDA_SMALL_NUMBER);

		return Res;
	}
};

bool FRancWorldLayersSamplingTest::RunTest(const FString& Parameters)
{
	FWorldDataLayersSamplingTestScenarios Scenarios(this);

	bool bResult = true;

	bResult &= Scenarios.TestBilinearInterpolation();

	return bResult;
}

#endif // WITH_DEV_AUTOMATION_TESTS