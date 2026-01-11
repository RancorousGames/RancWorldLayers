
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WorldLayersDebugWidget.generated.h"

class UImage;
class UComboBoxString;
class UTextBlock;

UENUM(BlueprintType)
enum class EWorldLayersDebugMode : uint8
{
	Hidden,
	MiniMap,
	FullScreen
};

UCLASS()
class RANCWORLDLAYERS_API UWorldLayersDebugWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	void SetDebugMode(EWorldLayersDebugMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	void SetSelectedLayer(int32 LayerIndex);

	/** Wipes and re-populates the layer list. */
	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	void RefreshLayerNames();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RancWorldLayers")
	EWorldLayersDebugMode CurrentMode = EWorldLayersDebugMode::Hidden;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* LayerDebugImage;

	UPROPERTY(meta = (BindWidgetOptional))
	UComboBoxString* LayerComboBox;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TooltipTextBlock;

protected:
	UFUNCTION()
	void OnLayerSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void PopulateLayerComboBox();

private:
	UPROPERTY()
	UTexture2D* CurrentDebugTexture;

	void UpdateDebugTexture();
	void UpdateTooltip(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent);
};
