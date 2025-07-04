using UnrealBuildTool;

public class RancWorldLayersTest : ModuleRules
{
	public RancWorldLayersTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"RancWorldLayers",
			"CoreUObject",
			"UnrealEd",
			"AssetTools",
			"Engine"
		});
	}
}
