using UnrealBuildTool;

public class Onyx2DEditor : ModuleRules
{
	public Onyx2DEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// The builder API in Public (content kit, asset utilities, tile and material builders, map painter) is used by games'
		// own editor modules to build their content in C++.
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd", "Paper2D", "Onyx2DRuntime" });

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"InputCore", "Slate", "SlateCore", "ToolMenus",
			"PropertyEditor", "AssetRegistry", "LevelEditor", "WorkspaceMenuStructure", "Settings",
			"Projects", "Onyx2DUI", "ContentBrowser", "PlacementMode", "MessageLog", "EditorFramework", "EngineSettings", "ToolWidgets", "AssetTools", "EditorWidgets", "Json", "MaterialEditor", "ImageCore", "DesktopPlatform", "GameProjectGeneration", "AssetDefinition", "BlueprintGraph", "EnhancedInput", "Kismet", "UMG", "UMGEditor", "AppFramework", "RHI"
		});
	}
}
