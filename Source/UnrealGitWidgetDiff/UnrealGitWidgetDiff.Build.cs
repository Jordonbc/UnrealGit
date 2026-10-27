// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

using UnrealBuildTool;

public class UnrealGitWidgetDiff : ModuleRules
{
	public UnrealGitWidgetDiff(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ImageCore",
				"Slate",
				"SlateCore",
				"UMG",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"UMGEditor",
				"RenderCore",
				"RHI",
				"UnrealGit",
				"AssetTools",
				"ContentBrowser",
				"SourceControl",
				"SourceControlWindows",
				"InputCore",
				"ToolMenus",
				"DesktopPlatform",
				"Projects",
			}
		);
	}
}