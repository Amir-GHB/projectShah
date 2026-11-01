// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ClimbSystemEditor : ModuleRules
{
    public ClimbSystemEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "ClimbSystem"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "EditorFramework",
            "LevelEditor",
            "Slate",
            "SlateCore",
            "InputCore"
        });
    }
}
