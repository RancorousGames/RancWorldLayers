
#include "WorldLayersDebugWidget.h"
#include "Components/Image.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "WorldLayersSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "WorldDataLayerAsset.h"

void UWorldLayersDebugWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (LayerComboBox)
	{
		RefreshLayerNames();
		LayerComboBox->OnSelectionChanged.AddDynamic(this, &UWorldLayersDebugWidget::OnLayerSelectionChanged);
	}

	if (TooltipTextBlock)
	{
		TooltipTextBlock->SetVisibility(ESlateVisibility::Hidden);
	}

	SetDebugMode(CurrentMode);
}

void UWorldLayersDebugWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CurrentMode != EWorldLayersDebugMode::Hidden)
	{
		// Real-time update
		UpdateDebugTexture();
	}
}

FReply UWorldLayersDebugWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (CurrentMode != EWorldLayersDebugMode::Hidden)
	{
		UpdateTooltip(InGeometry, InMouseEvent);
	}
	return FReply::Handled();
}

void UWorldLayersDebugWidget::SetDebugMode(EWorldLayersDebugMode NewMode)
{
	UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Widget: SetDebugMode %d. Current Visibility: %d"), (int32)NewMode, (int32)GetVisibility());
	CurrentMode = NewMode;
	
	if (NewMode == EWorldLayersDebugMode::Hidden)
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		SetVisibility(ESlateVisibility::Visible);
		UpdateDebugTexture();
	}
}

void UWorldLayersDebugWidget::SetSelectedLayer(int32 LayerIndex)
{
	if (LayerComboBox && LayerIndex >= 0 && LayerIndex < LayerComboBox->GetOptionCount())
	{
		LayerComboBox->SetSelectedIndex(LayerIndex);
		UpdateDebugTexture();
	}
}

void UWorldLayersDebugWidget::RefreshLayerNames()
{
	if (!LayerComboBox) return;

	LayerComboBox->ClearOptions();

	UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this);
	if (Subsystem)
	{
		TArray<FName> LayerNames = Subsystem->GetActiveLayerNames();
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Refreshing Combo Box with %d layers."), LayerNames.Num());
		for (const FName& Name : LayerNames)
		{
			LayerComboBox->AddOption(Name.ToString());
		}
	}

	if (LayerComboBox->GetOptionCount() > 0)
	{
		LayerComboBox->SetSelectedIndex(0);
		UpdateDebugTexture();
	}
}

void UWorldLayersDebugWidget::OnLayerSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UpdateDebugTexture();
}

void UWorldLayersDebugWidget::UpdateDebugTexture()
{
	if (LayerComboBox && LayerComboBox->GetSelectedOption() != TEXT(""))
	{
		UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this);
		if (Subsystem && LayerDebugImage)
		{
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Updating Debug Texture for layer: %s"), *LayerComboBox->GetSelectedOption());
			CurrentDebugTexture = Subsystem->GetDebugTextureForLayer(FName(*LayerComboBox->GetSelectedOption()), CurrentDebugTexture);
			LayerDebugImage->SetBrushFromTexture(CurrentDebugTexture);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[RancWorldLayers] UpdateDebugTexture: Subsystem or LayerDebugImage is NULL. Subsystem: %s, Image: %s"), 
				Subsystem ? TEXT("Valid") : TEXT("NULL"), LayerDebugImage ? TEXT("Valid") : TEXT("NULL"));
		}
	}
}

void UWorldLayersDebugWidget::UpdateTooltip(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!LayerDebugImage || !TooltipTextBlock || !LayerComboBox)
	{
		return;
	}

	FVector2D LocalMousePosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

	if (LayerDebugImage->IsHovered())
	{
		FVector2D ImageSize = LayerDebugImage->GetCachedGeometry().GetLocalSize();
		FVector2D UV = LocalMousePosition / ImageSize;

		UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(this);
		if (Subsystem && CurrentDebugTexture && GetWorld())
		{
			FIntPoint PixelCoords(FMath::FloorToInt(UV.X * CurrentDebugTexture->GetSizeX()), FMath::FloorToInt(UV.Y * CurrentDebugTexture->GetSizeY()));
			FLinearColor Value;
			Subsystem->GetValueAtLocation(FName(*LayerComboBox->GetSelectedOption()), FVector2D(PixelCoords.X, PixelCoords.Y), Value);

			FString TooltipText = FString::Printf(TEXT("Coord: (%d, %d)\nValue: R:%.3f G:%.3f B:%.3f A:%.3f"), PixelCoords.X, PixelCoords.Y, Value.R, Value.G, Value.B, Value.A);
			TooltipTextBlock->SetText(FText::FromString(TooltipText));
			TooltipTextBlock->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else
	{
		TooltipTextBlock->SetVisibility(ESlateVisibility::Hidden);
	}
}
