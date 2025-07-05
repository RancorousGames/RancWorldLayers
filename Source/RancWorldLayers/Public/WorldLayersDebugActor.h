
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldLayersDebugActor.generated.h"

class UWorldLayersDebugWidget;

UCLASS()
class RANCWORLDLAYERS_API AWorldLayersDebugActor : public AActor
{
	GENERATED_BODY()

public:
	AWorldLayersDebugActor();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UWorldLayersDebugWidget> DebugWidgetClass;

private:
	UPROPERTY()
	UWorldLayersDebugWidget* DebugWidgetInstance;
};
