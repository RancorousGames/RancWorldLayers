// Copyright Rancorous Games, 2025

#include "RancWorldLayersTestSetup.cpp"
#include "Framework/DebugTestResult.h"
#include "WorldDataLayerAsset.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#define TestName_Advanced "GameTests.RancWorldLayers.Advanced"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancWorldLayersAdvancedTest, TestName_Advanced,
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Context class for setting up the test environment
class WorldDataLayersAdvancedTestContext
{
public:
	// Use a new fixture name to get a clean world/subsystem
	WorldDataLayersAdvancedTestContext(FRancWorldLayersAdvancedTest* InTest)
		: Test(InTest),
		  TestFixture(FName(*FString(TestName_Advanced)))
	{
		Subsystem = TestFixture.GetSubsystem();
		Test->TestNotNull("Subsystem should not be null", Subsystem);
	}

	~WorldDataLayersAdvancedTestContext()
	{
		// Clean up temporary files created during tests
		const FString TempDir = FPaths::ProjectSavedDir() / TEXT("TestOutput");
		if (FPaths::DirectoryExists(TempDir))
		{
			IFileManager::Get().DeleteDirectory(*TempDir, false, true);
		}
	}

	UWorldLayersSubsystem* GetSubsystem() const { return Subsystem; }
	UWorld* GetWorld() const { return TestFixture.GetWorld(); }
	FRancWorldLayersTestFixture& GetTestFixture() { return TestFixture; }


private:
	FRancWorldLayersAdvancedTest* Test;
	FRancWorldLayersTestFixture TestFixture;
	UWorldLayersSubsystem* Subsystem;
};

// Class containing individual advanced test scenarios
class FWorldDataLayersAdvancedTestScenarios
{
public:
	FRancWorldLayersAdvancedTest* Test;
	const FString TempDir = FPaths::ProjectSavedDir() / TEXT("TestOutput");

	FWorldDataLayersAdvancedTestScenarios(FRancWorldLayersAdvancedTest* InTest)
		: Test(InTest)
	{
		// Ensure our temp directory for I/O tests exists
		IFileManager::Get().MakeDirectory(*TempDir, true);
	}

	// Test 1: Verifies that the 'RelativeToWorld' resolution mode works as expected.
	bool TestRelativeToWorldResolution() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersAdvancedTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();
		
		UWorldDataLayerAsset* LayerAsset = NewObject<UWorldDataLayerAsset>();
		LayerAsset->LayerName = FName("RelativeResTest");
		LayerAsset->ResolutionMode = EResolutionMode::RelativeToWorld;
		LayerAsset->CellSize = FVector2D(100.f, 100.f); // Each pixel is 1 meter
		LayerAsset->DataFormat = EDataFormat::R8;

		// FIX: Removed the dead code block that was here. It did nothing and was confusing.
		
		Subsystem->RegisterDataLayer(LayerAsset);
		const FVector2D TestLocation(550.f, 550.f); // In a 10000-wide world with origin -5000, this is at world-relative 5550
		const FLinearColor TestValue(0.75f, 0, 0, 0);
		Subsystem->SetValueAtLocation(LayerAsset->LayerName, TestLocation, TestValue);

		FLinearColor OutValue;
		const FVector2D QueryLocation(510.f, 590.f); // Different location, same cell (55, 55)
		Subsystem->GetValueAtLocation(LayerAsset->LayerName, QueryLocation, OutValue);
		
		float ExpectedValue = FMath::RoundToFloat(TestValue.R * 255.f) / 255.f;
		Res &= Test->TestEqual("Value read from same cell should match", OutValue.R, ExpectedValue, KINDA_SMALL_NUMBER);

		return Res;
	}

	// Test 2: Ensures coordinate conversions work correctly with a non-zero world origin.
	bool TestNonZeroWorldOrigin() const
	{
		FDebugTestResult Res = true;
		// FIX: To test a different world origin, we must create a new, separate test fixture
		// that is configured correctly from the start. We can't move the volume after the fact.
		// For this test, we can just use the default fixture which is now correctly centered
		// at world 0,0 and has an origin of (-Extents.X, -Extents.Y).

		WorldDataLayersAdvancedTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();
		
		UWorldDataLayerAsset* LayerAsset = NewObject<UWorldDataLayerAsset>();
		LayerAsset->LayerName = FName("NonZeroOriginTest");
		LayerAsset->ResolutionMode = EResolutionMode::Absolute;
		LayerAsset->Resolution = FIntPoint(100, 100);
		LayerAsset->DataFormat = EDataFormat::R8;

		Subsystem->RegisterDataLayer(LayerAsset);

		// The default fixture creates a 10000x10000 world centered at 0,0.
		// The WorldGridOrigin is therefore (-5000, -5000).
		const FVector2D TestLocation(-4000.f, -4000.f); 
		const FLinearColor TestValue(1.0f, 0, 0, 0);
		Subsystem->SetValueAtLocation(LayerAsset->LayerName, TestLocation, TestValue);

		FLinearColor OutValue;
		Subsystem->GetValueAtLocation(LayerAsset->LayerName, TestLocation, OutValue);
		Res &= Test->TestEqual("Value at negative coordinate should be set correctly", OutValue.R, TestValue.R, KINDA_SMALL_NUMBER);

		return Res;
	}

	// Test 3: Validates the precision of the R16F (float) data format.
	bool TestFloat16Precision() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersAdvancedTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();
		
		UWorldDataLayerAsset* LayerAsset = NewObject<UWorldDataLayerAsset>();
		LayerAsset->LayerName = FName("Float16Test");
		LayerAsset->ResolutionMode = EResolutionMode::Absolute;
		LayerAsset->Resolution = FIntPoint(10, 10);
		LayerAsset->DataFormat = EDataFormat::R16F; // Use float16
		Subsystem->RegisterDataLayer(LayerAsset);

		const FVector2D TestLocation(150.f, 150.f);
		const float TestFloat = 123.456f;
		Subsystem->SetValueAtLocation(LayerAsset->LayerName, TestLocation, FLinearColor(TestFloat, 0, 0, 0));

		const float OutFloat = Subsystem->GetFloatValueAtLocation(LayerAsset->LayerName, TestLocation);
		
		// FFloat16 has limited precision, so we test with a larger tolerance.
		// The actual value will be closer to 123.5.
		Res &= Test->TestEqual("R16F float value should be close to original", OutFloat, TestFloat, 0.1f);

		return Res;
	}

	// Test 4: Verifies that layers are initialized with the specified DefaultValue.
	bool TestNonZeroDefaultValue() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersAdvancedTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();
		
		UWorldDataLayerAsset* LayerAsset = NewObject<UWorldDataLayerAsset>();
		LayerAsset->LayerName = FName("DefaultValueTest");
		LayerAsset->ResolutionMode = EResolutionMode::Absolute;
		LayerAsset->Resolution = FIntPoint(10, 10);
		LayerAsset->DataFormat = EDataFormat::RGBA8;
		LayerAsset->DefaultValue = FLinearColor(0.1f, 0.2f, 0.3f, 0.4f);
		Subsystem->RegisterDataLayer(LayerAsset);

		FLinearColor OutValue;
		// Query a point without setting anything first
		Subsystem->GetValueAtLocation(LayerAsset->LayerName, FVector2D(1,1), OutValue);
		
		// Check each channel, accounting for 8-bit quantization
		float ExpectedR = FMath::RoundToFloat(LayerAsset->DefaultValue.R * 255.f) / 255.f;
		float ExpectedG = FMath::RoundToFloat(LayerAsset->DefaultValue.G * 255.f) / 255.f;
		float ExpectedB = FMath::RoundToFloat(LayerAsset->DefaultValue.B * 255.f) / 255.f;
		float ExpectedA = FMath::RoundToFloat(LayerAsset->DefaultValue.A * 255.f) / 255.f;

		Res &= Test->TestEqual("Default R value should match", OutValue.R, ExpectedR, KINDA_SMALL_NUMBER);
		Res &= Test->TestEqual("Default G value should match", OutValue.G, ExpectedG, KINDA_SMALL_NUMBER);
		Res &= Test->TestEqual("Default B value should match", OutValue.B, ExpectedB, KINDA_SMALL_NUMBER);
		Res &= Test->TestEqual("Default A value should match", OutValue.A, ExpectedA, KINDA_SMALL_NUMBER);

		return Res;
	}

	// Test 5: A full round-trip test of the subsystem's PNG export and import functionality.
	bool TestSubsystemPNGExportImportRoundtrip() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersAdvancedTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();

		// 1. Create source layer and set data
		UWorldDataLayerAsset* SourceAsset = NewObject<UWorldDataLayerAsset>();
		SourceAsset->LayerName = FName("PngSource");
		SourceAsset->Resolution = FIntPoint(32, 32);
		SourceAsset->DataFormat = EDataFormat::RGBA8;
		Subsystem->RegisterDataLayer(SourceAsset);
		const FVector2D TestLoc(16, 16);
		const FLinearColor TestVal(0.8f, 0.6f, 0.4f, 0.2f);
		Subsystem->SetValueAtLocation(SourceAsset->LayerName, TestLoc, TestVal);

		// 2. Export to PNG
		const FString TempFilePath = TempDir / TEXT("SubsystemRoundtrip.png");
		Subsystem->ExportLayerToPNG(SourceAsset, TempFilePath);
		Res &= Test->TestTrue("PNG file should exist on disk", IFileManager::Get().FileExists(*TempFilePath));
		if (!Res) return false;

		// 3. Create destination layer and import
		UWorldDataLayerAsset* DestAsset = NewObject<UWorldDataLayerAsset>();
		DestAsset->LayerName = FName("PngDest");
		DestAsset->Resolution = FIntPoint(32, 32); // Must match!
		DestAsset->DataFormat = EDataFormat::RGBA8;
		Subsystem->RegisterDataLayer(DestAsset);
		Subsystem->ImportLayerFromPNG(DestAsset, TempFilePath);

		// 4. Verify data
		FLinearColor OutValue;
		Subsystem->GetValueAtLocation(DestAsset->LayerName, TestLoc, OutValue);
		float ExpectedR = FMath::RoundToFloat(TestVal.R * 255.f) / 255.f;
		float ExpectedG = FMath::RoundToFloat(TestVal.G * 255.f) / 255.f;
		Res &= Test->TestEqual("Imported R should match original", OutValue.R, ExpectedR, KINDA_SMALL_NUMBER);
		Res &= Test->TestEqual("Imported G should match original", OutValue.G, ExpectedG, 0.01f);

		return Res;
	}
	
	// Test 6: Verifies that querying for a non-existent value in a spatial index returns false.
	bool TestFindNearest_NoPointsOfValueExist() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersAdvancedTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();

		UWorldDataLayerAsset* LayerAsset = NewObject<UWorldDataLayerAsset>();
		LayerAsset->LayerName = FName("FindNearestEmpty");
		LayerAsset->Resolution = FIntPoint(100, 100);
		LayerAsset->SpatialOptimization.bBuildAccelerationStructure = true;
		LayerAsset->SpatialOptimization.ValuesToTrack.Add(FLinearColor::Red);
		Subsystem->RegisterDataLayer(LayerAsset);
		
		// NOTE: We never add any red points to the layer.

		FVector2D FoundLocation;
		bool bFound = Subsystem->FindNearestPointWithValue(LayerAsset->LayerName, FVector2D(50,50), 100.f, FLinearColor::Red, FoundLocation);
		Res &= Test->TestFalse("FindNearest should return false when no points of the tracked value exist", bFound);

		return Res;
	}

	// Test 7: Tests the spatial index's ability to distinguish between multiple tracked values.
	bool TestFindNearest_WithMultipleTrackedValues() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersAdvancedTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();

		UWorldDataLayerAsset* LayerAsset = NewObject<UWorldDataLayerAsset>();
		LayerAsset->LayerName = FName("FindNearestMultiValue");
		LayerAsset->Resolution = FIntPoint(100, 100);
		// FIX: Use a data format that can actually store Red and Blue distinctly.
		LayerAsset->DataFormat = EDataFormat::RGBA8;
		LayerAsset->SpatialOptimization.bBuildAccelerationStructure = true;
		LayerAsset->SpatialOptimization.ValuesToTrack.Add(FLinearColor::Red); // Track Red
		LayerAsset->SpatialOptimization.ValuesToTrack.Add(FLinearColor::Blue); // Track Blue
		Subsystem->RegisterDataLayer(LayerAsset);

		// FIX: Use world locations that map to different pixels.
		// The test world is 10000x10000 with origin (-5000,-5000). Layer res is 100x100, so cell size is 100x100.
		const FVector2D RedLocation(1000.f, 1000.f);  // Maps to pixel (60, 60)
		const FVector2D BlueLocation(4000.f, 4000.f); // Maps to pixel (90, 90)
		Subsystem->SetValueAtLocation(LayerAsset->LayerName, RedLocation, FLinearColor::Red);
		Subsystem->SetValueAtLocation(LayerAsset->LayerName, BlueLocation, FLinearColor::Blue);

		// FIX: The system returns the pixel's center, so we must test against the calculated center location.
		const FVector2D ExpectedRedCenter(1050.f, 1050.f);  // Center of pixel (60,60)
		const FVector2D ExpectedBlueCenter(4050.f, 4050.f); // Center of pixel (90,90)

		FVector2D FoundLocation;
		// Search for RED from a point closer to the red point. Should find the red point.
		bool bFoundRed = Subsystem->FindNearestPointWithValue(LayerAsset->LayerName, FVector2D(1100.f, 1100.f), 5000.f, FLinearColor::Red, FoundLocation);
		Res &= Test->TestTrue("Should find the red point", bFoundRed);
		Res &= Test->TestTrue("Found location should be the red point's pixel center", FoundLocation.Equals(ExpectedRedCenter, 1.f));
		
		// Search for BLUE from a point closer to the blue point. Should find the blue point.
		bool bFoundBlue = Subsystem->FindNearestPointWithValue(LayerAsset->LayerName, FVector2D(4100.f, 4100.f), 5000.f, FLinearColor::Blue, FoundLocation);
		Res &= Test->TestTrue("Should find the blue point", bFoundBlue);
		Res &= Test->TestTrue("Found location should be the blue point's pixel center", FoundLocation.Equals(ExpectedBlueCenter, 1.f));

		return Res;
	}

	// Test 8: Verifies that GetLayerGpuTexture returns nullptr if GPU updates are disabled.
	bool TestGpuTextureIsNullWhenDisabled() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersAdvancedTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();

		UWorldDataLayerAsset* LayerAsset = NewObject<UWorldDataLayerAsset>();
		LayerAsset->LayerName = FName("NoGpuTexture");
		LayerAsset->GPUConfiguration.bKeepUpdatedOnGPU = false; // Explicitly disable GPU texture
		Subsystem->RegisterDataLayer(LayerAsset);

		UTexture* GpuTexture = Subsystem->GetLayerGpuTexture(LayerAsset->LayerName);
		Res &= Test->TestNull("GPU Texture should be null when bKeepUpdatedOnGPU is false", GpuTexture);

		return Res;
	}

	// Test 9: Confirms that registering a layer with a duplicate name overwrites the old one.
	bool TestRegisteringDuplicateLayerName() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersAdvancedTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();
		const FName DuplicateName("DuplicateNameTest");
		const FVector2D TestLocation(1,1);
		
		// 1. Register the first layer and set a value
		UWorldDataLayerAsset* FirstAsset = NewObject<UWorldDataLayerAsset>();
		FirstAsset->LayerName = DuplicateName;
		FirstAsset->DefaultValue = FLinearColor::Black;
		FirstAsset->DataFormat = EDataFormat::RGBA8; // Specify format
		FirstAsset->Resolution = FIntPoint(10, 10);
		Subsystem->RegisterDataLayer(FirstAsset);
		Subsystem->SetValueAtLocation(DuplicateName, TestLocation, FLinearColor::Red);

		// 2. Register a second layer with the same name but different properties
		UWorldDataLayerAsset* SecondAsset = NewObject<UWorldDataLayerAsset>();
		SecondAsset->LayerName = DuplicateName;
		SecondAsset->DefaultValue = FLinearColor::Blue;
		SecondAsset->DataFormat = EDataFormat::RGBA8; // Specify a format that can hold the color
		SecondAsset->Resolution = FIntPoint(10, 10);
		Subsystem->RegisterDataLayer(SecondAsset);

		// 3. Query the layer. It should have been re-initialized with the properties of the *second* asset.
		// Its data should now match the new DefaultValue, not the red value that was previously set.
		FLinearColor OutValue;
		Subsystem->GetValueAtLocation(DuplicateName, TestLocation, OutValue);
		
		Res &= Test->TestTrue("Value should be the default of the second, overwriting layer", OutValue.Equals(SecondAsset->DefaultValue));
		Res &= Test->TestFalse("Value should not be the red from the first layer", OutValue.Equals(FLinearColor::Red));

		return Res;
	}

	// Test 10: Ensures implicit nearest neighbor behavior of coordinate conversion.
	bool TestImplicitNearestNeighbor() const
	{
		FDebugTestResult Res = true;
		WorldDataLayersAdvancedTestContext Context(Test);
		UWorldLayersSubsystem* Subsystem = Context.GetSubsystem();
		
		UWorldDataLayerAsset* LayerAsset = NewObject<UWorldDataLayerAsset>();
		LayerAsset->LayerName = FName("NearestNeighborTest");
		LayerAsset->Resolution = FIntPoint(100, 100);
		LayerAsset->DataFormat = EDataFormat::RGBA8; // <-- THE FIX IS HERE
		Subsystem->RegisterDataLayer(LayerAsset);
		
		// Set a value at a location that will resolve to pixel (10, 10)
		Subsystem->SetValueAtLocation(LayerAsset->LayerName, FVector2D(10.5f, 10.5f), FLinearColor::Green);
		
		// Query a different location that should also resolve to pixel (10, 10)
		FLinearColor OutValue;
		Subsystem->GetValueAtLocation(LayerAsset->LayerName, FVector2D(10.9f, 10.1f), OutValue);

		// Because WorldLocationToPixel uses floor(), any location within the pixel's bounds should resolve to the same pixel.
		Res &= Test->TestTrue("Querying within the same pixel should return the set value", OutValue.Equals(FLinearColor::Green));

		return Res;
	}
};

bool FRancWorldLayersAdvancedTest::RunTest(const FString& Parameters)
{
	FWorldDataLayersAdvancedTestScenarios Scenarios(this);

	bool bResult = true;

	AddInfo("Running Test: RelativeToWorldResolution");
	bResult &= Scenarios.TestRelativeToWorldResolution();
	
	AddInfo("Running Test: NonZeroWorldOrigin");
	bResult &= Scenarios.TestNonZeroWorldOrigin();

	AddInfo("Running Test: Float16Precision");
	bResult &= Scenarios.TestFloat16Precision();

	AddInfo("Running Test: NonZeroDefaultValue");
	bResult &= Scenarios.TestNonZeroDefaultValue();
	
	AddInfo("Running Test: SubsystemPNGExportImportRoundtrip");
	bResult &= Scenarios.TestSubsystemPNGExportImportRoundtrip();

	AddInfo("Running Test: FindNearest_NoPointsOfValueExist");
	bResult &= Scenarios.TestFindNearest_NoPointsOfValueExist();

	AddInfo("Running Test: FindNearest_WithMultipleTrackedValues");
	bResult &= Scenarios.TestFindNearest_WithMultipleTrackedValues();

	AddInfo("Running Test: GpuTextureIsNullWhenDisabled");
	bResult &= Scenarios.TestGpuTextureIsNullWhenDisabled();

	AddInfo("Running Test: RegisteringDuplicateLayerName");
	bResult &= Scenarios.TestRegisteringDuplicateLayerName();

	AddInfo("Running Test: ImplicitNearestNeighbor");
	bResult &= Scenarios.TestImplicitNearestNeighbor();

	return bResult;
}

#endif // WITH_DEV_AUTOMATION_TESTS