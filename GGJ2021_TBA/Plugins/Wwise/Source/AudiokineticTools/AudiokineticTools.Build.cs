// Copyright 1998-2012 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudiokineticTools : ModuleRules
{
#if WITH_FORWARDED_MODULE_RULES_CTOR
    public AudiokineticTools(ReadOnlyTargetRules Target) : base(Target)
#else
    public AudiokineticTools(TargetInfo Target)
#endif
    {
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateIncludePaths.Add("AudiokineticTools/Private");
        PrivateIncludePathModuleNames.AddRange(
            new string[]
            {
                "TargetPlatform",
                "MainFrame",
				"MovieSceneTools",
                "LevelEditor"
            });

        PublicIncludePathModuleNames.AddRange(
            new string[] 
            { 
                "AssetTools",
                "ContentBrowser",
                "Matinee",
#if UE_4_24_OR_LATER
                "ToolMenus"
#endif
            });

        PublicDependencyModuleNames.AddRange(
            new string[] 
            { 
                "AkAudio",
                "Core",
                "InputCore",
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "Slate",
                "SlateCore",
                "Matinee",
                "EditorStyle",
				"Json",
				"XmlParser",
				"WorkspaceMenuStructure",
				"DirectoryWatcher",
                "Projects",
				"Sequencer",
                "PropertyEditor",
                "SharedSettingsWidgets",
#if UE_4_24_OR_LATER
                "ToolMenus"
#endif
            });

        PrivateDependencyModuleNames.AddRange(
            new string[]
			{
				"MovieScene",
				"DesktopPlatform",
				"MovieSceneTools",
				"MovieSceneTracks",
				"RenderCore",
				"SourceControl"
			});

#if !UE_4_25_OR_LATER
        PrivateDependencyModuleNames.Add("MatineeToLevelSequence");
#endif
#if UE_4_26_OR_LATER
        PrivateDependencyModuleNames.Add("RHI");
#endif
    }
}
