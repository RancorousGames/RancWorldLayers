#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WorldDataLayer.h"

#include "Async/Async.h"
#include "RenderGraphUtils.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"

class UWorldDataLayerAsset;
class AWorldDataVolume;
class AWorldLayersDebugActor;

#include "WorldLayersSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnWorldLayersRequestUpdate);

/** Generic interface for project-level actors to provide logic for Derivative layers. */
UINTERFACE(MinimalAPI, Blueprintable)
class UWorldLayersDerivationProvider : public UInterface
{
	GENERATED_BODY()
};

class RANCWORLDLAYERS_API IWorldLayersDerivationProvider
{
	GENERATED_BODY()

public:
	/** Called by the subsystem to allow this provider to populate a derivative layer. Return true if handled. */
	virtual bool OnDeriveLayer(FName LayerName) { return false; }
};

UCLASS()
class RANCWORLDLAYERS_API UWorldLayersSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Delegate called when debug visualization requests a data refresh (e.g. mode cycle). */
	FOnWorldLayersRequestUpdate OnRequestUpdate;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	bool Tick(float DeltaTime);

	static UWorldLayersSubsystem* Get(const UObject* WorldContext);

	// FIX: Add a public, explicit initializer for test environments
	void InitializeFromVolume(AWorldDataVolume* Volume);

	/** Wipes all registered layers and resets the volume reference. */
	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	void ClearAllLayers();

	/** Returns the currently active World Data Volume. */
	UFUNCTION(BlueprintPure, Category = "RancWorldLayers")
	AWorldDataVolume* GetWorldDataVolume() const { return WorldDataVolume.Get(); }

	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	bool GetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, FLinearColor& OutValue) const;

	/** Samples the layer at the given world location using bilinear interpolation. */
	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	bool GetValueAtLocationInterpolated(FName LayerName, const FVector2D& WorldLocation, FLinearColor& OutValue) const;

	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	float GetFloatValueAtLocation(FName LayerName, const FVector2D& WorldLocation) const; // Convenience for single-channel float layers

	// Generic Data Modification (CPU)
	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	void SetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, const FLinearColor& NewValue);

	void RegisterDataLayer(UWorldDataLayerAsset* LayerAsset);

	// GPU Methods
	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	UTexture* GetLayerGpuTexture(FName LayerName) const;

	void SpawnDebugActor();

	// Optimized Spatial Queries
	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	bool FindNearestPointWithValue(FName LayerName, const FVector2D& SearchOrigin, float MaxSearchRadius, const FLinearColor& TargetValue, FVector2D& OutWorldLocation) const;

	// Editor Utility and Debugging
	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	UTexture2D* GetDebugTextureForLayer(FName LayerName, UTexture2D* InDebugTexture = nullptr);

	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	void UpdateDebugRenderTarget(FName LayerName, UTextureRenderTarget2D* RenderTarget);

	void ExportLayerToPNG(UWorldDataLayerAsset* LayerAsset, const FString& FilePath);
	void ImportLayerFromPNG(UWorldDataLayerAsset* LayerAsset, const FString& FilePath);

	/** Recomputes a derivative layer based on its sources. */
	UFUNCTION(BlueprintCallable, Category = "RancWorldLayers")
	void UpdateDerivativeLayer(FName LayerName);

	// Public access for external tools (e.g. PCG nodes)
	const UWorldDataLayer* GetDataLayer(FName LayerName) const;
	TArray<FName> GetActiveLayerNames() const;
	FIntPoint WorldLocationToPixel(const FVector2D& WorldLocation, const UWorldDataLayer* Layer) const;
	FVector2D PixelToWorldLocation(const FIntPoint& PixelLocation, const UWorldDataLayer* Layer) const;

	FVector2D GetWorldGridOrigin() const { return WorldGridOrigin; }
	FVector2D GetWorldGridSize() const { return WorldGridSize; }

private:
	UPROPERTY()
	TMap<FName, UWorldDataLayer*> WorldDataLayers;

	UPROPERTY()
	TWeakObjectPtr<AWorldDataVolume> WorldDataVolume;

	UPROPERTY()
	TObjectPtr<AWorldLayersDebugActor> DebugActor;

	FVector2D WorldGridOrigin;
	FVector2D WorldGridSize;

	FTSTicker::FDelegateHandle TickHandle;

	void SyncCPUToGPU(UWorldDataLayer* DataLayer);
	void ReadbackTexture(UWorldDataLayer* DataLayer);
};