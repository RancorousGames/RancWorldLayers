
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
	virtual void Tick(float DeltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	void CreateDebugWidget();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UWorldLayersDebugWidget> DebugWidgetClass;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Visualization")
	TObjectPtr<UStaticMeshComponent> DebugMesh;

private:
	UPROPERTY()
	UWorldLayersDebugWidget* DebugWidgetInstance;

	UPROPERTY()
	UMaterialInstanceDynamic* DebugMID;

	void HandleDebugInput();
	void PositionActor();
	void Update3DVisualization();
};
