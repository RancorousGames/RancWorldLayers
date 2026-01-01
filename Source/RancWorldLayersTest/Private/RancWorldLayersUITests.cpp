// Copyright Rancorous Games, 2025

#include "RancWorldLayersTestSetup.cpp"
#include "Framework/DebugTestResult.h"
#include "WorldLayersDebugWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#undef TestName
#define TestName "GameTests.RancWorldLayers.UI"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancWorldLayersUITest, TestName,
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FWorldLayersUITestScenarios
{
public:
	FRancWorldLayersUITest* Test;

	FWorldLayersUITestScenarios(FRancWorldLayersUITest* InTest)
		: Test(InTest)
	{
	}

	bool TestDebugWidgetInitialization() const
	{
		FDebugTestResult Res = true;
		
		FRancWorldLayersTestFixture Fixture(FName("UIInitTest"));
		UWorld* World = Fixture.GetWorld();

		// Instantiate the widget
		UWorldLayersDebugWidget* DebugWidget = NewObject<UWorldLayersDebugWidget>(World);
		Res &= Test->TestNotNull("Widget should be created", DebugWidget);

		// Manually create and bind widgets since we don't have a UWidgetBlueprint here
		DebugWidget->LayerComboBox = NewObject<UComboBoxString>(DebugWidget);
		DebugWidget->LayerDebugImage = NewObject<UImage>(DebugWidget);
		DebugWidget->TooltipTextBlock = NewObject<UTextBlock>(DebugWidget);

		// Trigger initialization
		DebugWidget->NativeConstruct();

		// Verify combo box is populated (Fixture creates one "TestLayer")
		// Note: PopulateLayerComboBox uses AssetRegistry, so it might find real assets too.
		// But it should at least find something if there are layers.
		Res &= Test->TestTrue("Combo box should have options", DebugWidget->LayerComboBox->GetOptionCount() > 0);

		return Res;
	}
};

bool FRancWorldLayersUITest::RunTest(const FString& Parameters)
{
	FWorldLayersUITestScenarios Scenarios(this);

	bool bResult = true;
	bResult &= Scenarios.TestDebugWidgetInitialization();

	return bResult;
}

#endif // WITH_DEV_AUTOMATION_TESTS
