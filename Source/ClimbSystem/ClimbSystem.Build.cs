// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ClimbSystem : ModuleRules
{
	public ClimbSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ClimbSystem",
			"ClimbSystem/Variant_Platforming",
			"ClimbSystem/Variant_Platforming/Animation",
			"ClimbSystem/Variant_Combat",
			"ClimbSystem/Variant_Combat/AI",
			"ClimbSystem/Variant_Combat/Animation",
			"ClimbSystem/Variant_Combat/Gameplay",
			"ClimbSystem/Variant_Combat/Interfaces",
			"ClimbSystem/Variant_Combat/UI",
			"ClimbSystem/Variant_SideScrolling",
			"ClimbSystem/Variant_SideScrolling/AI",
			"ClimbSystem/Variant_SideScrolling/Gameplay",
			"ClimbSystem/Variant_SideScrolling/Interfaces",
			"ClimbSystem/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
