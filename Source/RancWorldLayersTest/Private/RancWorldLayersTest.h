#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

// Define a test suite for RancWorldLayers
DEFINE_LOG_CATEGORY_STATIC(LogRancWorldLayersTest, Log, All);

// Base class for RancWorldLayers tests
class FRancWorldLayersTestBase : public FAutomationTestBase
{
public:
	FRancWorldLayersTestBase(const FString& InName, const bool bInComplexTask) : FAutomationTestBase(InName, bInComplexTask) {}

protected:
	// Add common setup/teardown or helper functions here
};

// Minimal FDebugTestResult to mimic RancInventory's test style
struct FDebugTestResult
{
	bool bSuccess;

	FDebugTestResult(bool bInSuccess = true) : bSuccess(bInSuccess) {}

	operator bool() const { return bSuccess; }

	FDebugTestResult& operator&=(bool Other) { bSuccess = bSuccess && Other; return *this; }
};