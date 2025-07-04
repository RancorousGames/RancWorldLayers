#include "WorldDataLayerAssetActions.h"
#include "WorldDataLayerAsset.h"
#include "DesktopPlatformModule.h"
#include "ImageUtils.h"
#include "EditorFramework/AssetImportData.h"
#include "WorldLayersSubsystem.h" // Include for UMyWorldDataSubsystem
#include "Engine/Texture2D.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FWorldDataLayerAssetActions::FWorldDataLayerAssetActions(EAssetTypeCategories::Type InAssetCategory)
	: MyAssetCategory(InAssetCategory)
{
}

FText FWorldDataLayerAssetActions::GetName() const
{
	return LOCTEXT("FWorldDataLayerAssetActionsName", "World Data Layer");
}

FColor FWorldDataLayerAssetActions::GetTypeColor() const
{
	return FColor::Cyan;
}

UClass* FWorldDataLayerAssetActions::GetSupportedClass() const
{
	return UWorldDataLayerAsset::StaticClass();
}

void FWorldDataLayerAssetActions::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto Assets = GetTypedWeakObjectPtrs<UWorldDataLayerAsset>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("WorldDataLayer_ExportToPNG", "Export to PNG"),
		LOCTEXT("WorldDataLayer_ExportToPNG_Tooltip", "Exports the data layer to a PNG file."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FWorldDataLayerAssetActions::ExecuteExportToPNG, Assets)
		)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("WorldDataLayer_ImportFromPNG", "Import from PNG"),
		LOCTEXT("WorldDataLayer_ImportFromPNG_Tooltip", "Imports the data layer from a PNG file."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FWorldDataLayerAssetActions::ExecuteImportFromPNG, Assets)
		)
	);
}

uint32 FWorldDataLayerAssetActions::GetCategories()
{
	return MyAssetCategory;
}

void FWorldDataLayerAssetActions::ExecuteExportToPNG(TArray<TWeakObjectPtr<UWorldDataLayerAsset>> Objects)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if (UWorldDataLayerAsset* LayerAsset = Cast<UWorldDataLayerAsset>(Object))
			{
				TArray<FString> SaveFilenames;
				bool bSaved = DesktopPlatform->SaveFileDialog(
					FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
					LOCTEXT("ExportAssetDialogTitle", "Choose a destination for the PNG file").ToString(),
					FPaths::ProjectContentDir(),
					FString::Printf(TEXT("%s.png"), *LayerAsset->GetName()),
					TEXT("PNG File|*.png"),
					EFileDialogFlags::None,
					SaveFilenames
				);

				if (bSaved && SaveFilenames.Num() > 0)
				{
					UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(Objects[0].Get());
					if (Subsystem)
					{
						Subsystem->ExportLayerToPNG(LayerAsset, SaveFilenames[0]);
					}
				}
			}
		}
	}
}

void FWorldDataLayerAssetActions::ExecuteImportFromPNG(TArray<TWeakObjectPtr<UWorldDataLayerAsset>> Objects)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if (UWorldDataLayerAsset* LayerAsset = Cast<UWorldDataLayerAsset>(Object))
			{
				TArray<FString> OpenFilenames;
				bool bOpened = DesktopPlatform->OpenFileDialog(
					FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
					LOCTEXT("ImportAssetDialogTitle", "Choose a PNG file to import").ToString(),
					FPaths::ProjectContentDir(),
					TEXT(""),
					TEXT("PNG File|*.png"),
					EFileDialogFlags::None,
					OpenFilenames
				);

				if (bOpened && OpenFilenames.Num() > 0)
				{
					UWorldLayersSubsystem* Subsystem = UWorldLayersSubsystem::Get(Objects[0].Get());
					if (Subsystem)
					{
						Subsystem->ImportLayerFromPNG(LayerAsset, OpenFilenames[0]);
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
