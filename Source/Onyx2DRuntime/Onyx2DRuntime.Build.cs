using UnrealBuildTool;

public class Onyx2DRuntime : ModuleRules
{
	public Onyx2DRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"Paper2D", "ProceduralMeshComponent", "DeveloperSettings"
		});
	}
}
