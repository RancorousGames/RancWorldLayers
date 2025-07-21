#include "RancWorldLayersEditor.h"
#include "WorldDataLayerAssetActions.h"
#include "AssetToolsModule.h"
#include "ActorFactoryWorldDataVolume.h"
#include "Editor/EditorEngine.h"
#include "UnrealEdGlobals.h"

#define LOCTEXT_NAMESPACE "FRancWorldLayersEditorModule"

void FRancWorldLayersEditorModule::StartupModule()
{
	// Register asset actions
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.RegisterAssetTypeActions(MakeShareable(new FWorldDataLayerAssetActions(EAssetTypeCategories::Misc)));

	if (GEditor)
	{
		UActorFactoryWorldDataVolume* WorldDataVolumeFactory = NewObject<UActorFactoryWorldDataVolume>();
		GEditor->ActorFactories.Add(WorldDataVolumeFactory);
	}
}

void FRancWorldLayersEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRancWorldLayersEditorModule, RancWorldLayersEditor)