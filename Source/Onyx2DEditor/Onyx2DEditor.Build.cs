using UnrealBuildTool;

public class Onyx2DEditor : ModuleRules
{
	public Onyx2DEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "UnrealEd", "Slate", "SlateCore", "ToolMenus",
			"Paper2D", "PropertyEditor", "AssetRegistry", "LevelEditor", "WorkspaceMenuStructure", "Settings",
			"Projects", "Onyx2DRuntime", "ContentBrowser", "PlacementMode", "MessageLog", "EditorFramework", "EngineSettings", "ToolWidgets", "AssetTools", "EditorWidgets", "Json", "MaterialEditor", "ImageCore", "DesktopPlatform", "GameProjectGeneration", "AssetDefinition"
		});
	}
}
