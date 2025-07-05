
#include "WorldLayersDebugActor.h"
#include "WorldLayersDebugWidget.h"
#include "Blueprint/UserWidget.h"

AWorldLayersDebugActor::AWorldLayersDebugActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWorldLayersDebugActor::BeginPlay()
{
	Super::BeginPlay();

	if (DebugWidgetClass)
	{
		DebugWidgetInstance = CreateWidget<UWorldLayersDebugWidget>(GetWorld(), DebugWidgetClass);
		if (DebugWidgetInstance)
		{
			DebugWidgetInstance->AddToViewport();
		}
	}
}
