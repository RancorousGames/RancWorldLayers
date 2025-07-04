#include "RancWorldLayersEditor.h"
#include "WorldDataLayerAssetActions.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "FRancWorldLayersEditorModule"

void FRancWorldLayersEditorModule::StartupModule()
{
	// Register asset actions
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.RegisterAssetTypeActions(MakeShareable(new FWorldDataLayerAssetActions(EAssetTypeCategories::Misc)));
}

void FRancWorldLayersEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRancWorldLayersEditorModule, RancWorldLayersEditor)