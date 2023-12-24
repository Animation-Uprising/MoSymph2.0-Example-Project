//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

using UnrealBuildTool;

public class MotionSymphonyEditor : ModuleRules
{
	public MotionSymphonyEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PrivateIncludePaths.AddRange(
			new string[] {
                "MotionSymphony/Private",
                "MotionSymphony/Private/AnimGraph",
                "MotionSymphony/Private/Data",
                "MotionSymphony/Private/Objects/Tags",
                "MotionSymphony/Public/Objects/Tags",

                "MotionSymphonyEditor/Private",
                "MotionSymphonyEditor/Private/AssetTools",
                "MotionSymphonyEditor/Private/Factories",
                "MotionSymphonyEditor/Private/Toolkits",
                "MotionSymphonyEditor/Private/GUI"
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "AnimGraph",
                "AnimGraphRuntime",
                "MotionSymphony",
                "Persona",
                "AnimationModifiers"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "CoreUObject",
                "Slate",
                "SlateCore",
                "Engine",
                "InputCore",
                "UnrealEd", // for FAssetEditorManager
				"KismetWidgets",
                "Kismet",  // for FWorkflowCentricApplication
				"PropertyEditor",
                "ContentBrowser",
                "WorkspaceMenuStructure",
                "EditorStyle",
                "EditorWidgets",
                "Projects",
                "MotionSymphony",
                "AnimGraph",
                "BlueprintGraph",
                "AssetRegistry",
                "AdvancedPreviewScene",
                "AnimGraphRuntime",
                "ToolMenus",
                "SequencerWidgets",
                "Persona",
                "TimeManagement",
                "AnimationModifiers",
                "AnimationBlueprintLibrary",
                "ApplicationCore"
				// ... add private dependencies that you statically link with here ...	
			}
			);

        PrivateIncludePathModuleNames.AddRange(
           new string[] {
                "AssetTools",
           }
           );



        DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
            );
	}
}
