//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MotionSymphonyEditor.h"

#include "Templates/SharedPointer.h"
#include "MotionSymphonyStyle.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "ISettingsContainer.h"
#include "MotionSymphonySettings.h"

#define LOCTEXT_NAMESPACE "FMotionSymphonyEditorModule"

void FMotionSymphonyEditorModule::StartupModule()
{
	PreProcessToolkit_ToolbarExtManager = MakeShareable(new FExtensibilityManager);
	FMotionSymphonyStyle::Initialize();

	RegisterSettings();
	RegisterAssetTools();
	RegisterMenuExtensions();
	
}

void FMotionSymphonyEditorModule::ShutdownModule()
{
	PreProcessToolkit_ToolbarExtManager.Reset();
	FMotionSymphonyStyle::Shutdown();

	UnRegisterAssetTools();
	UnRegisterMenuExtensions();

	if (UObjectInitialized())
	{
		UnRegisterSettings();
	}
}

void FMotionSymphonyEditorModule::RegisterAssetTools()
{
	auto RegisterAssetTypeAction = [this](const TSharedRef<IAssetTypeActions>& InAssetTypeAction)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		RegisteredAssetTypeActions.Add(InAssetTypeAction);
		AssetTools.RegisterAssetTypeActions(InAssetTypeAction);
	};
	
	RegisterAssetTypeAction(MakeShareable(new FAssetTypeActions_MotionDataAsset()));
	RegisterAssetTypeAction(MakeShareable(new FAssetTypeActions_MotionMatchConfig()));
	RegisterAssetTypeAction(MakeShareable(new FAssetTypeActions_MotionCalibration()));
}

void FMotionSymphonyEditorModule::RegisterMenuExtensions()
{
	
}

void FMotionSymphonyEditorModule::UnRegisterAssetTools()
{
	FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");
	if(AssetToolsModule)
	{
		for(TSharedPtr<IAssetTypeActions> RegisteredAssetTypeAction : RegisteredAssetTypeActions)
		{
			AssetToolsModule->Get().UnregisterAssetTypeActions(RegisteredAssetTypeAction.ToSharedRef());
		}
	}
}

void FMotionSymphonyEditorModule::UnRegisterMenuExtensions()
{
	
}

bool FMotionSymphonyEditorModule::HandleSettingsSaved()
{
	return true;
}

void FMotionSymphonyEditorModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		//Create a new category
		ISettingsContainerPtr SettingsContainer = SettingsModule->GetContainer("Project");

		SettingsContainer->DescribeCategory("MotionSymphonySettings",
			LOCTEXT("RuntimeWDCategoryName", "MotionSymphonySettings"),
			LOCTEXT("RuntimeWDCategoryDescription", "Game configuration for motion symphony plugin module"));

		//Register settings
		ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "MotionSymphony",
			LOCTEXT("RuntimeGeneralSettingsName", "MotionSymphony"),
			LOCTEXT("RuntimeGeneratlSettingsDescription", "Base configuration for motion symphony plugin module"),
			GetMutableDefault<UMotionSymphonySettings>());

		//Register the save handler to your settings for validation checks
		if (SettingsSection.IsValid())
		{
			SettingsSection->OnModified().BindRaw(this, &FMotionSymphonyEditorModule::HandleSettingsSaved);
		}
	}
}

void FMotionSymphonyEditorModule::UnRegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "MotionSymphony");
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMotionSymphonyEditorModule, MotionSymphonyEditor)