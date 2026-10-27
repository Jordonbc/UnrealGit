using System.IO;
using UnrealBuildTool;

public class UnrealGitTests : ModuleRules
{
	public UnrealGitTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, ".."));
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "UnrealGit", "Private"));

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"UnrealGit",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Json",
				"JsonUtilities",
				"SourceControl",
				"UnrealEd",
			}
		);
	}
}

