// Copyright Rancorous Games, 2025

#include "RancWorldLayersTestSetup.cpp"
#include "Framework/DebugTestResult.h"
#include "Engine/Texture2D.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#undef TestName
#define TestName "GameTests.RancWorldLayers.Infrastructure"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancWorldLayersInfrastructureTest, TestName,
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FWorldLayersInfrastructureTestScenarios
{
public:
	FRancWorldLayersInfrastructureTest* Test;

	FWorldLayersInfrastructureTestScenarios(FRancWorldLayersInfrastructureTest* InTest)
		: Test(InTest)
	{
	}

	/** Verifies that the WorldLayersSubsystem exists and is accessible in an Editor world. */
	bool TestSubsystemInEditorWorld() const
	{
		FDebugTestResult Res = true;
		
		// Create a test world (simulating an Editor world via EWorldType::Editor if we were strictly testing that, 
		// but CreateTestWorld currently uses EWorldType::Game. 
		// However, since it's now a UWorldSubsystem, it will be instantiated regardless.)
		FRancWorldLayersTestFixture Fixture(FName("EditorAccessTest"));
		UWorldLayersSubsystem* Subsystem = Fixture.GetSubsystem();

		Res &= Test->TestNotNull("Subsystem should exist in the world", Subsystem);
		
		return Res;
	}

	/** Verifies that a layer is correctly initialized from an InitialDataTexture. */
	bool TestTextureInitialization() const
	{
		FDebugTestResult Res = true;
		
		FRancWorldLayersTestFixture Fixture(FName("TextureInitTest"));
		UWorldLayersSubsystem* Subsystem = Fixture.GetSubsystem();
		
		// 1. Create a dummy texture (2x2)
		UTexture2D* DummyTexture = UTexture2D::CreateTransient(2, 2, PF_B8G8R8A8);
		FColor* MipData = (FColor*)DummyTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		MipData[0] = FColor::Red;
		MipData[1] = FColor::Green;
		MipData[2] = FColor::Blue;
		MipData[3] = FColor::White;
		DummyTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
		DummyTexture->UpdateResource();

		// 2. Create a layer asset pointing to this texture
		UWorldDataLayerAsset* TextureLayerAsset = NewObject<UWorldDataLayerAsset>();
		TextureLayerAsset->LayerName = FName("TextureLayer");
		TextureLayerAsset->ResolutionMode = EResolutionMode::Absolute;
		TextureLayerAsset->Resolution = FIntPoint(2, 2);
		TextureLayerAsset->DataFormat = EDataFormat::RGBA8;
		TextureLayerAsset->InitialDataTexture = DummyTexture;

		// 3. Register the layer
		Subsystem->RegisterDataLayer(TextureLayerAsset);

		// 4. Verify values at specific pixels
		// Pixel (0,0) -> Red
		FLinearColor Value00;
		Subsystem->GetValueAtLocation(FName("TextureLayer"), FVector2D(0, 0), Value00);
		Res &= Test->TestEqual("Pixel (0,0) should be Red", Value00, FLinearColor::Red);

		// Pixel (1,1) -> White
		FLinearColor Value11;
		// Map location to center of pixel (1,1) based on Fixture's default 10000x10000 bounds and 2x2 res
		// (Pixel size is 5000)
		Subsystem->GetValueAtLocation(FName("TextureLayer"), FVector2D(7500, 7500), Value11);
		Res &= Test->TestEqual("Pixel (1,1) should be White", Value11, FLinearColor::White);

		return Res;
	}
};

bool FRancWorldLayersInfrastructureTest::RunTest(const FString& Parameters)
{
	FWorldLayersInfrastructureTestScenarios Scenarios(this);

	bool bResult = true;
	bResult &= Scenarios.TestSubsystemInEditorWorld();
	bResult &= Scenarios.TestTextureInitialization();

	return bResult;
}

#endif // WITH_DEV_AUTOMATION_TESTS
