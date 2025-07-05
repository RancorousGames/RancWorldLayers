// Copyright Rancorous Games, 2025

#include "RancWorldLayersTestSetup.cpp"
#include "Framework/DebugTestResult.h"
#include "WorldDataLayerAsset.h"
#include "Engine/TextureRenderTarget2D.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#define TestName_GPU "GameTests.RancWorldLayers.GPUIntegration"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancWorldLayersGPUIntegrationTest, TestName_GPU,
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Context class for setting up the test environment
class WorldDataLayersGPUIntegrationTestContext
{
public:
	WorldDataLayersGPUIntegrationTestContext(FRancWorldLayersGPUIntegrationTest* InTest)
		: Test(InTest),
		  TestFixture(FName(*FString(TestName_GPU)), FVector2D(16.0f, 16.0f)) // Set WorldBounds to match Resolution
	{
		Subsystem = TestFixture.GetSubsystem();
		Test->TestNotNull("Subsystem should not be null", Subsystem);
	}

	UWorldLayersSubsystem* GetSubsystem() const { return Subsystem; }
	UWorld* GetWorld() const { return TestFixture.GetWorld(); }
	FRancWorldLayersTestFixture* GetTestFixture() { return &TestFixture; }

private:
	FRancWorldLayersGPUIntegrationTest* Test;
	FRancWorldLayersTestFixture TestFixture;
	UWorldLayersSubsystem* Subsystem;
};

// Class containing individual test scenarios
class FWorldDataLayersGPUIntegrationTestScenarios
{
public:
	FRancWorldLayersGPUIntegrationTest* Test;

	FWorldDataLayersGPUIntegrationTestScenarios(FRancWorldLayersGPUIntegrationTest* InTest)
		: Test(InTest)
	{
	}

	bool TestCPUGPUSyncAndReadback() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersGPUIntegrationTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();

		// Create a new layer asset for GPU testing
		UWorldDataLayerAsset* GPUTestLayerAsset = NewObject<UWorldDataLayerAsset>();
		GPUTestLayerAsset->LayerName = FName("GPUTestLayer");
		GPUTestLayerAsset->ResolutionMode = EResolutionMode::Absolute;
		GPUTestLayerAsset->Resolution = FIntPoint(16, 16); // Small resolution for quick testing
		GPUTestLayerAsset->DataFormat = EDataFormat::RGBA8;
		GPUTestLayerAsset->DefaultValue = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		GPUTestLayerAsset->GPUConfiguration.bKeepUpdatedOnGPU = true;
		GPUTestLayerAsset->GPUConfiguration.bIsGPUWritable = true;
		GPUTestLayerAsset->GPUConfiguration.ReadbackBehavior = EWorldDataLayerReadbackBehavior::Periodic;
		GPUTestLayerAsset->GPUConfiguration.PeriodicReadbackSeconds = 0.1f; // Fast readback
		GPUTestLayerAsset->WorldGridOrigin = FVector2D(0.0f, 0.0f);
		GPUTestLayerAsset->WorldGridSize = FVector2D(16.0f, 16.0f);
		Subsystem->RegisterDataLayer(GPUTestLayerAsset);

		FName TestLayerName = GPUTestLayerAsset->LayerName;

		// 1. Test CPU to GPU sync
		FVector2D TestLocation = FVector2D(5.0f, 5.0f);
		FLinearColor TestColor = FLinearColor(1.0f, 0.5f, 0.25f, 1.0f); // Red, Green, Blue, Alpha
		Subsystem->SetValueAtLocation(TestLayerName, TestLocation, TestColor);

		// Allow some frames for GPU sync to happen
		FPlatformProcess::Sleep(0.2f); 

		UTexture* GpuTexture = Subsystem->GetLayerGpuTexture(TestLayerName);
		Res &= Test->TestNotNull("GPU Texture should exist", GpuTexture);

		// For a proper GPU sync test, we would need to read back from the GPU texture
		// and compare. This is complex in automation tests without rendering context.
		// For now, we rely on the ReadbackTexture function to verify the roundtrip.

		// 2. Test GPU to CPU readback (triggered by periodic behavior)
		// Set a different value on CPU, then let readback overwrite it.
		FLinearColor InitialCPUValue = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		Subsystem->SetValueAtLocation(TestLayerName, TestLocation, InitialCPUValue);

		// Wait for readback to occur
		FPlatformProcess::Sleep(0.5f); 

		FLinearColor ReadbackValue;
		Subsystem->GetValueAtLocation(TestLayerName, TestLocation, ReadbackValue);

		// Due to potential precision differences and color space conversions (FLinearColor to FColor and back)
		// we test with a small tolerance.
		Res &= Test->TestEqual("Readback R value should match original R", ReadbackValue.R, TestColor.R, 0.01f);
		Res &= Test->TestEqual("Readback G value should match original G", ReadbackValue.G, TestColor.G, 0.01f);
		Res &= Test->TestEqual("Readback B value should match original B", ReadbackValue.B, TestColor.B, 0.01f);
		Res &= Test->TestEqual("Readback A value should match original A", ReadbackValue.A, TestColor.A, 0.01f);

		return Res;
	}
};

bool FRancWorldLayersGPUIntegrationTest::RunTest(const FString& Parameters)
{
	FWorldDataLayersGPUIntegrationTestScenarios Scenarios(this);

	bool bResult = true;

	bResult &= Scenarios.TestCPUGPUSyncAndReadback();

	return bResult;
}

#endif // WITH_DEV_AUTOMATION_TESTS
