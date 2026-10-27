// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

using UnrealBuildTool;

public class UnrealGit : ModuleRules
{
	public UnrealGit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"SourceControl",
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"DeveloperSettings",
				"Json",
				"JsonUtilities",
				"Projects",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"SourceControlWindows",
				"AssetTools",
				"ContentBrowser",
				"ToolMenus",
				"DesktopPlatform",
			}
			);

	}
}