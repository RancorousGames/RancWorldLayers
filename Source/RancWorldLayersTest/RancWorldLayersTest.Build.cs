using UnrealBuildTool;

public class RancWorldLayersTest : ModuleRules
{
	public RancWorldLayersTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RancWorldLayers", // Our main plugin module
				"UnrealEd", // For editor-specific testing utilities
				"Projects" // For IPluginManager
			}
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine"
			}
			);
		
		// Add any modules that your module dynamically loads here
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry"
			}
			);
	}
}
