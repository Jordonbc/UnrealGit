// Some copyright should be here...

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
			}
			);
		
	}
}
