
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WorldLayersDebugWidget.generated.h"

class UImage;
class UComboBoxString;
class UTextBlock;

UCLASS()
class RANCWORLDLAYERS_API UWorldLayersDebugWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

protected:
	UPROPERTY(meta = (BindWidget))
	UImage* LayerDebugImage;

	UPROPERTY(meta = (BindWidget))
	UComboBoxString* LayerComboBox;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TooltipTextBlock;

	UFUNCTION()
	void OnLayerSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void PopulateLayerComboBox();

private:
	UPROPERTY()
	UTexture2D* CurrentDebugTexture;

	void UpdateDebugTexture();
	void UpdateTooltip(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent);
};
