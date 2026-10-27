// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class UnrealGitTests : ModuleRules
{
	public UnrealGitTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, ".."));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "UnrealGit", "Private"));

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"SourceControl",
				"UnrealEd",
				"UnrealGit",
			}
		);
	}
}

