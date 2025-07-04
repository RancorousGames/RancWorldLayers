// Copyright RancDev, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Forward declaration of our scenarios class
class FRancWorldLayersFileManipulationTestScenarios;

// The main test class registered with the Automation Framework.
// It acts as a simple orchestrator.
// FIX #1: Added a semicolon at the end of the macro definition.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancWorldLayersFileManipulationTest, "GameTests.RancWorldLayers.FileManipulation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

/**
 * This class contains all the actual test logic and scenarios.
 * It is instantiated by the main test runner and performs all operations.
 */
class FRancWorldLayersFileManipulationTestScenarios
{
public:
	// Constructor takes the test instance to allow calling TestTrue, etc.
	FRancWorldLayersFileManipulationTestScenarios(FAutomationTestBase* InTest) : Test(InTest) {}

	// --- Public Test Cases ---

	/**
	 * Tests the ability to create a transient texture, write pixel data to it,
	 * and then read that same pixel data back accurately.
	 */
	bool Test_CreateAndReadTransientTexture()
	{
		const int32 TextureSize = 128;

		// 1. Create a transient texture from scratch. No file loading.
		UTexture2D* TransientTexture = CreateTestTexture(TextureSize, TextureSize);
		if (!Test->TestNotNull(TEXT("Transient Texture should be created."), TransientTexture))
		{
			return false;
		}

		// 2. Read a pixel from a known location.
		FColor ReadColor;
		const int32 TestX = 10, TestY = 20;
		if (!GetPixelColorFromTexture2D(TransientTexture, TestX, TestY, ReadColor))
		{
			Test->AddError( "Failed to read pixel from a valid transient texture.");
			return false;
		}

		// 3. Verify the color is what we expect from our generation logic.
		FColor ExpectedColor = GenerateColorForPixel(TestX, TestY, TextureSize);
		if (!Test->TestEqual(TEXT("Read pixel color should match the generated pattern."), ReadColor, ExpectedColor))
		{
			return false;
		}

		Test->AddInfo(TEXT("Test passed successfully."));
		return true;
	}

	/**
	 * Performs a full round-trip test:
	 * 1. Generate pixel data in memory.
	 * 2. Encode it as a PNG.
	 * 3. Save the PNG to a temporary file.
	 * 4. Load the PNG from the file.
	 * 5. Decode it back to pixel data.
	 * 6. Verify the decoded data matches the original.
	 */
	bool Test_PNGReadWriteRoundtrip()
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		TSharedPtr<IImageWrapper> PngWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		if (!PngWrapper.IsValid())
		{
			Test->AddError(TEXT("Could not create PNG Image Wrapper."));
			return false;
		}

		// 1. Generate source pixel data
		const int32 TextureSize = 64;
		TArray<FColor> SourcePixels;
		SourcePixels.SetNum(TextureSize * TextureSize);
		for (int32 Y = 0; Y < TextureSize; ++Y)
		{
			for (int32 X = 0; X < TextureSize; ++X)
			{
				SourcePixels[Y * TextureSize + X] = GenerateColorForPixel(X, Y, TextureSize);
			}
		}

		// 2. Encode to PNG format
		if (!PngWrapper->SetRaw(SourcePixels.GetData(), SourcePixels.Num() * sizeof(FColor), TextureSize, TextureSize, ERGBFormat::BGRA, 8))
		{
			Test->AddError(TEXT("Failed to set raw data on PNG wrapper."));
			return false;
		}
		const TArray64<uint8>& PngData = PngWrapper->GetCompressed(100);

		// 3. Save to a temporary file
		const FString TempDir = FPaths::ProjectSavedDir() / TEXT("TestOutput");
		const FString TempFilePath = TempDir / TEXT("TestRoundtrip.png");
		IFileManager::Get().MakeDirectory(*TempDir, true);

		if (!FFileHelper::SaveArrayToFile(PngData, *TempFilePath))
		{
			Test->AddError(TEXT("Failed to save PNG data to temporary file"));
			return false;
		}

		// 4. Load from file
		TArray<uint8> LoadedPngData;
		if (!FFileHelper::LoadFileToArray(LoadedPngData, *TempFilePath))
		{
			Test->AddError(TEXT("Failed to load PNG data from temporary file: %s"));
			IFileManager::Get().Delete(*TempFilePath); // Cleanup
			return false;
		}

		// 5. Decode back to pixel data
		if (!PngWrapper->SetCompressed(LoadedPngData.GetData(), LoadedPngData.Num()))
		{
			Test->AddError(TEXT("Failed to set compressed data on PNG wrapper from loaded file."));
			IFileManager::Get().Delete(*TempFilePath); // Cleanup
			return false;
		}
		
		// FIX #2: Decode into a TArray<uint8> as required by the GetRaw function.
		TArray<uint8> DecodedRawData;
		if (!PngWrapper->GetRaw(ERGBFormat::BGRA, 8, DecodedRawData))
		{
			Test->AddError(TEXT("Failed to get raw data from decoded PNG."));
			IFileManager::Get().Delete(*TempFilePath); // Cleanup
			return false;
		}

		// 6. Verify
		// First, check that the total number of bytes is correct.
		const int32 ExpectedByteCount = SourcePixels.Num() * sizeof(FColor);
		bool bIsEqual = Test->TestEqual(TEXT("Decoded byte count matches source"), DecodedRawData.Num(), ExpectedByteCount);
		
		if (bIsEqual)
		{
			// Then, compare the raw memory of the two arrays.
			bIsEqual &= Test->TestEqual(TEXT("Decoded pixel data matches source"), FMemory::Memcmp(DecodedRawData.GetData(), SourcePixels.GetData(), ExpectedByteCount), 0);
		}

		// Cleanup
		IFileManager::Get().Delete(*TempFilePath);

		if(bIsEqual) Test->AddInfo(TEXT("Test passed successfully."));
		return bIsEqual;
	}


private:
	FAutomationTestBase* Test;

	// Generates a predictable color based on pixel coordinates.
	FColor GenerateColorForPixel(int32 X, int32 Y, int32 TextureSize) const
	{
		// Simple gradient for predictable, non-uniform color
		const uint8 Red = (uint8)(((float)X / TextureSize) * 255);
		const uint8 Green = (uint8)(((float)Y / TextureSize) * 255);
		return FColor(Red, Green, 128, 255);
	}

	// Creates a UTexture2D in memory with a generated color pattern.
	UTexture2D* CreateTestTexture(int32 Width, int32 Height) const
	{
		// Create the transient texture object configured for CPU access
		UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		if (!Texture)
		{
			return nullptr;
		}

		// Generate the pixel data
		TArray<FColor> Pixels;
		Pixels.SetNum(Width * Height);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				Pixels[Y * Width + X] = GenerateColorForPixel(X, Y, Width);
			}
		}

		// Lock the texture for writing and copy the data
		void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(TextureData, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();

		// Update the resource to make it usable
		Texture->UpdateResource();

		return Texture;
	}

	// Safely reads a pixel color from a UTexture2D.
	bool GetPixelColorFromTexture2D(const UTexture2D* Texture, int32 X, int32 Y, FColor& OutColor) const
	{
		if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.IsEmpty())
		{
			return false;
		}

		const FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		if (X < 0 || X >= Mip.SizeX || Y < 0 || Y >= Mip.SizeY)
		{
			return false;
		}

		// We created the texture, so we know it's already configured for CPU access.
		const FColor* FormattedImageData = reinterpret_cast<const FColor*>(Mip.BulkData.LockReadOnly());
		if (!FormattedImageData)
		{
			return false;
		}

		OutColor = FormattedImageData[Y * Mip.SizeX + X];
		Mip.BulkData.Unlock();
		return true;
	}
};


// The main RunTest function now simply orchestrates the scenario calls.
bool FRancWorldLayersFileManipulationTest::RunTest(const FString& Parameters)
{
	FRancWorldLayersFileManipulationTestScenarios Scenarios(this);

	bool bOverallResult = true;

	bOverallResult &= Scenarios.Test_CreateAndReadTransientTexture();
	bOverallResult &= Scenarios.Test_PNGReadWriteRoundtrip();

	return bOverallResult;
}