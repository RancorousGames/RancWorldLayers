#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "WorldDataLayerAsset.h"

class FWorldDataLayerAssetActions : public FAssetTypeActions_Base
{
public:
	FWorldDataLayerAssetActions(EAssetTypeCategories::Type InAssetCategory);

	// FAssetTypeActions_Base overrides
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	virtual uint32 GetCategories() override;

private:
	EAssetTypeCategories::Type MyAssetCategory;
	/** Handler for the "Export to PNG" action */
	void ExecuteExportToPNG(const TArray<TWeakObjectPtr<UWorldDataLayerAsset>> SelectedAssets);

	/** Handler for the "Import from PNG" action */
	void ExecuteImportFromPNG(const TArray<TWeakObjectPtr<UWorldDataLayerAsset>> SelectedAssets);
};
