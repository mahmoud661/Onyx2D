// Copyright Onyx2D. All Rights Reserved.

#include "Assets/OnyxAssetFactory.h"
#include "Assets/OnyxLevelTools.h"
#include "Assets/OnyxLevelValidator.h"
#include "Assets/OnyxPlaceables.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserMenuContexts.h"
#include "Core/Onyx2DSettings.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "PaperTileMap.h"
#include "PaperSprite.h"
#include "PaperTileSet.h"
#include "Assets/OnyxNormalMaps.h"
#include "Lighting/OnyxEditorLightingView.h"
#include "Assets/OnyxFramePacker.h"
#include "Assets/OnyxMaterialBuilder.h"
#include "Examples/OnyxSunnyLandExample.h"
#include "FileHelpers.h"
#include "Assets/OnyxMapPainter.h"
#include "Assets/OnyxTilePadding.h"
#include "Framework/Docking/TabManager.h"
#include "IPlacementModeModule.h"
#include "LevelEditor.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "Style/OnyxEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Subsystems/ImportSubsystem.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/OnyxToolWidgets.h"
#include "Widgets/SOnyxToolkit.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

namespace
{
	const FName OnyxTabName(TEXT("Onyx2DTools"));
	const FName PlacementCategory(TEXT("Onyx2D"));
	const FName MessageLogName(TEXT("Onyx2D"));
}

/**
 * Onyx2D editor integration: the tools panel in its own window, toolbar and menu
 * entries, a Onyx2D category in Place Actors, Content Browser actions and the pixel-art import hook.
 */
class FOnyx2DEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FOnyxEditorStyle::Register();

		FMessageLogModule& MessageLog = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		FMessageLogInitializationOptions LogOptions;
		LogOptions.bShowPages = false;
		LogOptions.bAllowClear = true;
		MessageLog.RegisterLogListing(MessageLogName, INVTEXT("Onyx2D"), LogOptions);

		// The tools live in their own window (a nomad tab): drag its tab into the level editor to dock it if wanted.
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OnyxTabName, FOnSpawnTab::CreateRaw(this, &FOnyx2DEditorModule::SpawnNomadTab))
			.SetDisplayName(INVTEXT("Onyx2D"))
			.SetTooltipText(INVTEXT("Onyx2D tools: character wizard, animations, tiles, parallax and level setup."))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
			.SetIcon(FOnyxEditorStyle::Icon("Onyx2D.Icon"));

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FOnyx2DEditorModule::RegisterMenus));
		LightingView.Start();

		if (GEditor)
		{
			OnEditorReady();
		}
		else
		{
			PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FOnyx2DEditorModule::OnEditorReady);
		}
	}

	virtual void ShutdownModule() override
	{
		LightingView.Stop();
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
		if (GEditor)
		{
			if (UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>())
			{
				ImportSubsystem->OnAssetPostImport.Remove(PostImportHandle);
			}
		}
		if (IPlacementModeModule::IsAvailable())
		{
			IPlacementModeModule::Get().UnregisterPlacementCategory(PlacementCategory);
		}
		if (FMessageLogModule* MessageLog = FModuleManager::GetModulePtr<FMessageLogModule>("MessageLog"))
		{
			MessageLog->UnregisterLogListing(MessageLogName);
		}
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OnyxTabName);
		}
		FOnyxEditorStyle::Unregister();
	}

private:
	// --- Tools panel -------------------------------------------------------------

	TSharedRef<SDockTab> MakeToolsTab(ETabRole Role)
	{
		TSharedRef<SDockTab> Tab = SNew(SDockTab).TabRole(Role).Label(INVTEXT("Onyx2D"));
		Tab->SetTabIcon(FOnyxEditorStyle::Get().GetBrush("Onyx2D.Icon.Small"));
		TSharedRef<SOnyxToolkit> Toolkit = SNew(SOnyxToolkit, Tab);
		Tab->SetContent(Toolkit);
		ToolsTab = Tab;
		ToolkitWidget = Toolkit;
		return Tab;
	}

	TSharedRef<SDockTab> SpawnNomadTab(const FSpawnTabArgs& Args)
	{
		return MakeToolsTab(ETabRole::NomadTab);
	}

	/** Brings the tools up in their own window (or wherever the user last docked them). */
	TSharedPtr<SOnyxToolkit> OpenTools()
	{
		if (TSharedPtr<SDockTab> Existing = ToolsTab.Pin())
		{
			Existing->ActivateInParent(ETabActivationCause::SetDirectly);
			FGlobalTabmanager::Get()->DrawAttention(Existing.ToSharedRef());
			return ToolkitWidget.Pin();
		}
		FGlobalTabmanager::Get()->TryInvokeTab(OnyxTabName);
		return ToolkitWidget.Pin();
	}

	void OpenToolTab(FName TabId)
	{
		if (TSharedPtr<SOnyxToolkit> Toolkit = OpenTools())
		{
			Toolkit->FocusTab(TabId);
		}
	}

	// --- Menus -------------------------------------------------------------------

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		const FSlateIcon Icon = FOnyxEditorStyle::Icon("Onyx2D.Icon");
		const FUIAction Open(FExecuteAction::CreateLambda([this]() { OpenTools(); }));

		// Main toolbar: "Onyx2D" button + dropdown with every tool and the quick actions.
		UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.AssetsToolBar");
		FToolMenuSection& ToolbarSection = Toolbar->FindOrAddSection("Onyx2D");
		FToolMenuEntry Button = FToolMenuEntry::InitToolBarButton("OpenOnyx2D", Open, INVTEXT("Onyx2D"), INVTEXT("Open the Onyx2D tools"), Icon);
		Button.StyleNameOverride = "CalloutToolbar";
		ToolbarSection.AddEntry(Button);
		FToolMenuEntry Combo = FToolMenuEntry::InitComboButton("Onyx2DMenu", FUIAction(),
			FNewToolMenuDelegate::CreateRaw(this, &FOnyx2DEditorModule::FillToolsMenu), INVTEXT("Onyx2D Tools"), INVTEXT("Onyx2D tools and quick actions"), Icon, true);
		Combo.StyleNameOverride = "CalloutToolbar";
		ToolbarSection.AddEntry(Combo);

		// Tools menu.
		UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& MenuSection = ToolsMenu->FindOrAddSection("Onyx2D", INVTEXT("Onyx2D"));
		MenuSection.AddSubMenu("Onyx2DSubMenu", INVTEXT("Onyx2D"), INVTEXT("Onyx2D tools and quick actions"),
			FNewToolMenuDelegate::CreateRaw(this, &FOnyx2DEditorModule::FillToolsMenu), false, Icon);

		// Content Browser: right-click a texture.
		UToolMenu* TextureMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.Texture2D");
		FToolMenuSection& TextureSection = TextureMenu->AddSection("Onyx2D", INVTEXT("Onyx2D"), FToolMenuInsert("GetAssetActions", EToolMenuInsertType::After));
		TextureSection.AddDynamicEntry("Onyx2DTextureActions", FNewToolMenuSectionDelegate::CreateRaw(this, &FOnyx2DEditorModule::FillTextureMenu));

		// Content Browser: right-click sprites / tile sets.
		for (const TCHAR* MenuName : { TEXT("ContentBrowser.AssetContextMenu.PaperSprite"), TEXT("ContentBrowser.AssetContextMenu.PaperTileSet") })
		{
			UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(MenuName);
			FToolMenuSection& Section = Menu->AddSection("Onyx2D", INVTEXT("Onyx2D"), FToolMenuInsert("GetAssetActions", EToolMenuInsertType::After));
			Section.AddDynamicEntry("Onyx2DNormalMap", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
				if (Context && Context->SelectedAssets.Num() > 0)
				{
					AddNormalMapEntry(InSection, Context->SelectedAssets);
				}
			}));
		}

		// Content Browser: right-click tile sets -> fix seams.
		{
			UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.PaperTileSet");
			FToolMenuSection& Section = Menu->FindOrAddSection("Onyx2D");
			Section.AddDynamicEntry("Onyx2DPadTiles", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
				if (!Context || Context->SelectedAssets.IsEmpty())
				{
					return;
				}
				const TArray<FAssetData> Selected = Context->SelectedAssets;
				InSection.AddMenuEntry("OnyxPadTiles", INVTEXT("Fix Tile Seams (Pad Tiles)"),
					INVTEXT("Extrudes every tile's edge pixels so no lines show between tiles while the camera moves. Indices, collision and terrains are kept."),
					FOnyxEditorStyle::Icon("Onyx2D.Tab.Tiles"),
					FUIAction(FExecuteAction::CreateLambda([Selected]()
					{
						int32 Padded = 0;
						for (const FAssetData& Asset : Selected)
						{
							Padded += FOnyxTilePadding::PadTileSet(Cast<UPaperTileSet>(Asset.GetAsset())) ? 1 : 0;
						}
						FOnyxMapPainter::RebuildLevelCollision();
						OnyxTools::Notify(FText::Format(INVTEXT("{0} tile set(s) padded"), Padded), Padded > 0);
					})));
			}));
		}

		// Content Browser: right-click a tile map.
		UToolMenu* TileMapMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.PaperTileMap");
		FToolMenuSection& TileMapSection = TileMapMenu->AddSection("Onyx2D", INVTEXT("Onyx2D"), FToolMenuInsert("GetAssetActions", EToolMenuInsertType::After));
		TileMapSection.AddDynamicEntry("Onyx2DTileMapActions", FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& Section)
		{
			const UContentBrowserAssetContextMenuContext* Context = Section.FindContext<UContentBrowserAssetContextMenuContext>();
			if (!Context || Context->SelectedAssets.Num() != 1)
			{
				return;
			}
			const FAssetData Asset = Context->SelectedAssets[0];
			Section.AddMenuEntry("PaintOnyxLevel", INVTEXT("Paint in Level Painter"), INVTEXT("Open this tile map in the Onyx2D Level Painter."),
				FOnyxEditorStyle::Icon("Onyx2D.Tab.Painter"), FUIAction(FExecuteAction::CreateLambda([this, Asset]()
				{
					if (TSharedPtr<SOnyxToolkit> Toolkit = OpenTools())
					{
						Toolkit->ShowPainter(Cast<UPaperTileMap>(Asset.GetAsset()));
					}
				})));
		}));
	}

	void FillToolsMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Tools = Menu->AddSection("Tools", INVTEXT("Tools"));
		auto AddTool = [this, &Tools](FName Name, const FText& Label, const FText& Tooltip, FName Icon, FName TabId)
		{
			Tools.AddMenuEntry(Name, Label, Tooltip, FOnyxEditorStyle::Icon(Icon), FUIAction(FExecuteAction::CreateLambda([this, TabId]() { OpenToolTab(TabId); })));
		};
		AddTool("Import", INVTEXT("Import Art"), INVTEXT("Drop PNGs or folders: tile sets and characters built for you"), "Onyx2D.Tab.Import", SOnyxToolkit::ImportTabId);
		AddTool("Character", INVTEXT("Character Wizard"), INVTEXT("Sprite sheet to playable character"), "Onyx2D.Tab.Character", SOnyxToolkit::CharacterTabId);
		AddTool("Animations", INVTEXT("Animations"), INVTEXT("Edit animations and frame events"), "Onyx2D.Tab.Animations", SOnyxToolkit::AnimationsTabId);
		AddTool("Tiles", INVTEXT("Tiles"), INVTEXT("Tile sets and tile maps"), "Onyx2D.Tab.Tiles", SOnyxToolkit::TilesTabId);
		AddTool("Painter", INVTEXT("Level Painter"), INVTEXT("Paint tiles and place entities, LDtk style"), "Onyx2D.Tab.Painter", SOnyxToolkit::PainterTabId);
		AddTool("Parallax", INVTEXT("Parallax"), INVTEXT("Parallax background"), "Onyx2D.Tab.Parallax", SOnyxToolkit::ParallaxTabId);
		AddTool("Level", INVTEXT("Level"), INVTEXT("Place actors and check the level"), "Onyx2D.Tab.Level", SOnyxToolkit::LevelTabId);

		FToolMenuSection& Actions = Menu->AddSection("Actions", INVTEXT("Quick Actions"));
		Actions.AddMenuEntry("SetupLevel", INVTEXT("Set Up Level for 2D"), INVTEXT("Add game mode, camera bounds, player start and lights where missing."),
			FOnyxEditorStyle::Icon("Onyx2D.Tab.Level"), FUIAction(FExecuteAction::CreateLambda([]()
			{
				const TArray<FString> Added = FOnyxLevelTools::SetupCurrentLevel();
				OnyxTools::Notify(Added.IsEmpty() ? INVTEXT("The level was already set up.") : FText::FromString(TEXT("Added: ") + FString::Join(Added, TEXT(", "))), true);
			})));
		Actions.AddMenuEntry("ConfigureProject", INVTEXT("Configure Project for 2D"), INVTEXT("Recommended renderer settings for 2D and Onyx Game Mode as default."),
			FOnyxEditorStyle::Icon("Onyx2D.Project"), FUIAction(FExecuteAction::CreateLambda([]()
			{
				FOnyxLevelValidator::ConfigureProjectFor2D();
				OnyxTools::Notify(INVTEXT("Project configured for 2D. Restart the editor for distance-field / shadow changes."), true);
			})));
		Actions.AddMenuEntry("CheckLevel", INVTEXT("Check Level"), INVTEXT("Open the Level tab and look for setup problems."),
			FOnyxEditorStyle::Icon("Onyx2D.Validate"), FUIAction(FExecuteAction::CreateLambda([this]() { OpenToolTab(SOnyxToolkit::LevelTabId); })));
		Actions.AddMenuEntry("RebuildMaterials", INVTEXT("Rebuild Plugin Materials"),
			INVTEXT("Re-creates the Onyx2D materials (lit sprite, 2D lighting composite, background, glow, halo) from C++."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				const TArray<FString> Built = FOnyxMaterialBuilder::EnsureMaterials(true);
				OnyxTools::Notify(FText::Format(INVTEXT("Rebuilt {0} plugin material assets"), Built.Num()), true);
			})));

		FToolMenuSection& Examples = Menu->AddSection("Examples", INVTEXT("Examples"));
		Examples.AddMenuEntry("BuildSunnyLand", INVTEXT("Build Sunny Land Example"),
			INVTEXT("Builds a complete level from the Sunny Land pack (CC0, shipped in the plugin's Extras folder): fox, enemies, items, the original map, lighting. Opens it when done."),
			FOnyxEditorStyle::Icon("Onyx2D.Tab.Level"),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FText Error;
				if (FOnyxSunnyLandExample::Build(Error))
				{
					FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(FOnyxSunnyLandExample::LevelPath, FPackageName::GetMapPackageExtension()));
					OnyxTools::Notify(INVTEXT("Sunny Land example built: press Play"), true);
				}
				else
				{
					OnyxTools::Notify(Error, false);
				}
			})));
	}

	/** "Generate Normal Map" for textures, sprites and tile sets: every selected asset's sheet gets one. */
	static void AddNormalMapEntry(FToolMenuSection& Section, const TArray<FAssetData>& Selected)
	{
		Section.AddMenuEntry("GenerateOnyxNormalMap", INVTEXT("Generate Normal Map (2D Lighting)"),
			INVTEXT("Builds a normal map from the pixel art and plugs it into every sprite and tile set that uses the sheet, so lights shade it in relief."),
			FOnyxEditorStyle::Icon("Onyx2D.Light"),
			FUIAction(FExecuteAction::CreateLambda([Selected]()
			{
				TSet<UTexture2D*> Sheets;
				for (const FAssetData& Asset : Selected)
				{
					UObject* Object = Asset.GetAsset();
					if (UTexture2D* Texture = Cast<UTexture2D>(Object))
					{
						Sheets.Add(Texture);
					}
					else if (const UPaperSprite* Sprite = Cast<UPaperSprite>(Object))
					{
						Sheets.Add(Sprite->GetSourceTexture());
					}
					else if (const UPaperTileSet* TileSet = Cast<UPaperTileSet>(Object))
					{
						Sheets.Add(TileSet->GetTileSheetTexture());
					}
				}
				int32 Assigned = 0;
				for (UTexture2D* Sheet : Sheets)
				{
					Assigned += FOnyxNormalMaps::GenerateForSheet(Sheet, FOnyxNormalMapSettings());
				}
				OnyxTools::Notify(FText::Format(INVTEXT("Normal maps: {0} sheet(s), {1} sprite(s) / tile set(s) updated"), Sheets.Num(), Assigned), Assigned > 0);
			})));
	}

	/** Frame-by-frame images -> one animation strip ("player-run-1..6" -> "T_player_run_Strip"). */
	static void AddPackStripEntry(FToolMenuSection& Section, const TArray<FAssetData>& Selected)
	{
		Section.AddMenuEntry("PackOnyxStrip", INVTEXT("Pack Into Animation Strip"),
			INVTEXT("Puts the selected frame images side by side (in name order) in one texture, ready for the Character Wizard or a flipbook."),
			FOnyxEditorStyle::Icon("Onyx2D.Tab.Animations"),
			FUIAction(FExecuteAction::CreateLambda([Selected]()
			{
				TArray<UTexture2D*> Frames;
				for (const FAssetData& Asset : Selected)
				{
					Frames.Add(Cast<UTexture2D>(Asset.GetAsset()));
				}
				FOnyxFramePacker::SortFrames(Frames);
				if (Frames.IsEmpty())
				{
					return;
				}
				// Name after what the frames share: "player-run-1", "player-run-2" -> "T_player_run_Strip".
				FString Prefix = Frames[0]->GetName();
				for (const UTexture2D* Frame : Frames)
				{
					while (!Frame->GetName().StartsWith(Prefix))
					{
						Prefix.LeftChopInline(1);
					}
				}
				Prefix = Prefix.TrimChar(TEXT('-')).TrimChar(TEXT('_'));
				const FString Folder = FPackageName::GetLongPackagePath(Frames[0]->GetOutermost()->GetName());
				FIntPoint FrameSize;
				FText Error;
				UTexture2D* Strip = FOnyxFramePacker::PackStrip(Frames, Folder, TEXT("T_") + (Prefix.IsEmpty() ? FString(TEXT("Frames")) : Prefix) + TEXT("_Strip"), FrameSize, Error);
				OnyxTools::Notify(Strip
					? FText::Format(INVTEXT("{0}: {1} frames of {2} x {3}"), FText::FromString(Strip->GetName()), Frames.Num(), FrameSize.X, FrameSize.Y)
					: Error, Strip != nullptr);
			})));
	}

	void FillTextureMenu(FToolMenuSection& Section)
	{
		const UContentBrowserAssetContextMenuContext* Context = Section.FindContext<UContentBrowserAssetContextMenuContext>();
		if (Context && Context->SelectedAssets.Num() > 0)
		{
			AddNormalMapEntry(Section, Context->SelectedAssets);
		}
		if (Context && Context->SelectedAssets.Num() > 1)
		{
			AddPackStripEntry(Section, Context->SelectedAssets);
		}
		if (!Context || Context->SelectedAssets.Num() != 1)
		{
			return;
		}
		const FAssetData Asset = Context->SelectedAssets[0];
		Section.AddMenuEntry("CreateOnyxCharacter", INVTEXT("Create Onyx Character..."),
			INVTEXT("Open the Character Wizard with this sprite sheet."), FOnyxEditorStyle::Icon("Onyx2D.Tab.Character"),
			FUIAction(FExecuteAction::CreateLambda([this, Asset]()
			{
				if (TSharedPtr<SOnyxToolkit> Toolkit = OpenTools())
				{
					Toolkit->ShowCharacterWizard(Cast<UTexture2D>(Asset.GetAsset()));
				}
			})));
		Section.AddMenuEntry("CreateOnyxTileSet", INVTEXT("Create Onyx Tile Set..."),
			INVTEXT("Open the Tiles tool with this tile sheet."), FOnyxEditorStyle::Icon("Onyx2D.Tab.Tiles"),
			FUIAction(FExecuteAction::CreateLambda([this, Asset]()
			{
				if (TSharedPtr<SOnyxToolkit> Toolkit = OpenTools())
				{
					Toolkit->ShowTiles(Cast<UTexture2D>(Asset.GetAsset()));
				}
			})));
		Section.AddMenuEntry("ConfigurePixelArt", INVTEXT("Apply Pixel-Art Settings"),
			INVTEXT("Nearest filtering, no mipmaps, uncompressed."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings"),
			FUIAction(FExecuteAction::CreateLambda([Asset]()
			{
				if (UTexture2D* Texture = Cast<UTexture2D>(Asset.GetAsset()))
				{
					FOnyxAssetFactory::ConfigurePixelArtTexture(Texture);
					OnyxTools::Notify(FText::Format(INVTEXT("{0}: pixel-art settings applied"), FText::FromString(Texture->GetName())), true);
				}
			})));
	}

	// --- Place Actors category ------------------------------------------------------

	void RegisterPlacementCategory()
	{
		IPlacementModeModule& Placement = IPlacementModeModule::Get();
		Placement.RegisterPlacementCategory(FPlacementCategoryInfo(INVTEXT("Onyx2D"), FOnyxEditorStyle::Icon("Onyx2D.Icon"), PlacementCategory, TEXT("PMOnyx2D"), 35));
		int32 Order = 0;
		for (const FOnyxPlaceable& Placeable : GetOnyxPlaceables())
		{
			Placement.RegisterPlaceableItem(PlacementCategory, MakeShared<FPlaceableItem>(static_cast<UActorFactory*>(nullptr), FAssetData(Placeable.Class), Order++));
		}
	}

	// --- Startup / import hook --------------------------------------------------------

	void OnEditorReady()
	{
		if (UImportSubsystem* ImportSubsystem = GEditor ? GEditor->GetEditorSubsystem<UImportSubsystem>() : nullptr)
		{
			PostImportHandle = ImportSubsystem->OnAssetPostImport.AddRaw(this, &FOnyx2DEditorModule::HandleAssetPostImport);
		}
		RegisterPlacementCategory();
		// The plugin's materials are built in C++: create any that are missing (fresh checkout, deleted asset).
		if (!FOnyxMaterialBuilder::AreMaterialsPresent())
		{
			FOnyxMaterialBuilder::EnsureMaterials(false);
		}
	}

	void HandleAssetPostImport(UFactory* Factory, UObject* Object)
	{
		if (UOnyx2DSettings::Get()->bAutoConfigurePixelArtTextures)
		{
			FOnyxAssetFactory::ConfigurePixelArtTexture(Cast<UTexture2D>(Object));
		}
	}

	TWeakPtr<SDockTab> ToolsTab;
	TWeakPtr<SOnyxToolkit> ToolkitWidget;
	FDelegateHandle PostEngineInitHandle;
	FDelegateHandle PostImportHandle;
	FOnyxEditorLightingView LightingView;
};

IMPLEMENT_MODULE(FOnyx2DEditorModule, Onyx2DEditor);
