
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldLayersDebugActor.generated.h"

class UWorldLayersDebugWidget;
class UDecalComponent;

UENUM(BlueprintType)
enum class EWorldLayers3DMode : uint8
{
	None,
	Decal,
	SmallPlane,
	WorldPlane
};

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	EWorldLayers3DMode Current3DMode = EWorldLayers3DMode::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	int32 SelectedLayerIndex = 0;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Visualization")
	TObjectPtr<UStaticMeshComponent> DebugMesh;

	UPROPERTY(VisibleAnywhere, Category = "Visualization")
	TObjectPtr<UDecalComponent> DebugDecal;

private:
	UPROPERTY()
	UWorldLayersDebugWidget* DebugWidgetInstance;

	UPROPERTY()
	UMaterialInstanceDynamic* PlaneMID;

	UPROPERTY()
	UMaterialInstanceDynamic* DecalMID;

	TArray<FName> LayerNames;

	void HandleDebugInput();
	void PositionActor();
	void Update3DVisualization();
	void Set3DMode(EWorldLayers3DMode NewMode);
	void RefreshLayerNames();
};
