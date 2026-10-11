using UnrealBuildTool;

public class Onyx2DUI : ModuleRules
{
	public Onyx2DUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"UMG", "Slate", "SlateCore", "DeveloperSettings", "Onyx2DRuntime"
		});
		PrivateDependencyModuleNames.AddRange(new[] { "EngineSettings" });
	}
}
