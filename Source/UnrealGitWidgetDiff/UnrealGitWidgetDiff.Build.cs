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
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetTools",
				"ContentBrowser",
				"InputCore",
				"RenderCore",
				"Slate",
				"SlateCore",
				"SourceControl",
				"SourceControlWindows",
				"ToolMenus",
				"UMG",
				"UMGEditor",
				"UnrealEd",
				"UnrealGit",
			}
		);
	}
}