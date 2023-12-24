//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "IAssetTools.h"
#include "AssetTypeActions_MotionDataAsset.h"
#include "AssetTypeActions_MotionMatchCalibration.h"
#include "AssetTypeActions_MotionMatchConfig.h"

class FMotionSymphonyEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual TSharedPtr<FExtensibilityManager> GetPreProcessToolkitToolbarExtensibilityManager() { return PreProcessToolkit_ToolbarExtManager;}

private:
	TSharedPtr<FExtensibilityManager> PreProcessToolkit_ToolbarExtManager;

	void RegisterAssetTools();
	void RegisterMenuExtensions();
	
	template<class AssetType>
	void RegisterAssetTypeAction(IAssetTools& AssetTools, SharedPointerInternals::TRawPtrProxy<AssetType> TypeActions);

	void UnRegisterAssetTools();
	void UnRegisterMenuExtensions();

	bool HandleSettingsSaved();
	void RegisterSettings();
	void UnRegisterSettings();

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;
};