
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

	/** Forces the UI to refresh its layer list. */
	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	void UpdateDebugWidget();

	void InitializeActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UWorldLayersDebugWidget> DebugWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	EWorldLayers3DMode Current3DMode = EWorldLayers3DMode::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	int32 SelectedLayerIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization|Assets")
	FString DebugMaterialPath = TEXT("/RancWorldLayers/Debug/M_WorldLayersDebugPlane.M_WorldLayersDebugPlane");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization|Assets")
	FString DebugDecalMaterialPath = TEXT("/RancWorldLayers/Debug/M_WorldLayersDebugDecal.M_WorldLayersDebugDecal");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization|Assets")
	FString DebugRenderTargetPath = TEXT("/RancWorldLayers/Debug/RT_WorldLayersDebugTex.RT_WorldLayersDebugTex");

	/** Called when the debug mode is changed (cycle 2D/3D). Useful for triggering project-specific populators. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Visualization")
	void OnDebugModeChanged(int32 NewCombinedMode);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostActorCreated() override;
	virtual void Destroyed() override;

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

	UPROPERTY()
	UTexture2D* DebugTextureInstance;

	UPROPERTY()
	UTextureRenderTarget2D* DebugRenderTargetInstance;

	TArray<FName> LayerNames;

	void HandleDebugInput();
	void PositionActor();
	void Update3DVisualization();
	void Set3DMode(EWorldLayers3DMode NewMode);
	void RefreshLayerNames();
};
