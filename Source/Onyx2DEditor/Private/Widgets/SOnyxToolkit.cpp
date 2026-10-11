// Copyright Onyx2D. All Rights Reserved.

#include "Widgets/SOnyxToolkit.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Style/OnyxEditorStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SOnyxAnimationTool.h"
#include "Widgets/SOnyxCharacterTool.h"
#include "Widgets/SOnyxImportTool.h"
#include "Widgets/SOnyxLevelTool.h"
#include "Widgets/SOnyxPainterTool.h"
#include "Widgets/SOnyxParallaxTool.h"
#include "Widgets/SOnyxTileTool.h"

const FName SOnyxToolkit::ImportTabId(TEXT("Onyx2D.Import"));
const FName SOnyxToolkit::CharacterTabId(TEXT("Onyx2D.CharacterWizard"));
const FName SOnyxToolkit::AnimationsTabId(TEXT("Onyx2D.Animations"));
const FName SOnyxToolkit::TilesTabId(TEXT("Onyx2D.Tiles"));
const FName SOnyxToolkit::PainterTabId(TEXT("Onyx2D.Painter"));
const FName SOnyxToolkit::ParallaxTabId(TEXT("Onyx2D.Parallax"));
const FName SOnyxToolkit::LevelTabId(TEXT("Onyx2D.Level"));

namespace
{
	const TCHAR* LayoutName = TEXT("Onyx2DTools_Layout_v4");
}

void SOnyxToolkit::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& OwnerTab)
{
	TabManager = FGlobalTabmanager::Get()->NewTabManager(OwnerTab);
	TabManager->SetAllowWindowMenuBar(false);
	RegisterTabs();

	TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(LayoutName)
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(ImportTabId, ETabState::OpenedTab)
				->AddTab(CharacterTabId, ETabState::OpenedTab)
				->AddTab(AnimationsTabId, ETabState::OpenedTab)
				->AddTab(TilesTabId, ETabState::OpenedTab)
				->AddTab(PainterTabId, ETabState::OpenedTab)
				->AddTab(ParallaxTabId, ETabState::OpenedTab)
				->AddTab(LevelTabId, ETabState::OpenedTab)
				->SetForegroundTab(ImportTabId)
			)
		);
	Layout = FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, Layout);

	OwnerTab->SetOnPersistVisualState(SDockTab::FOnPersistVisualState::CreateSP(this, &SOnyxToolkit::SaveLayout));

	ChildSlot
	[
		TabManager->RestoreFrom(Layout, OwnerTab->GetParentWindow()).ToSharedRef()
	];
}

SOnyxToolkit::~SOnyxToolkit()
{
	if (TabManager.IsValid())
	{
		SaveLayout();
		for (const FName TabId : { ImportTabId, CharacterTabId, AnimationsTabId, TilesTabId, PainterTabId, ParallaxTabId, LevelTabId })
		{
			TabManager->UnregisterTabSpawner(TabId);
		}
	}
}

void SOnyxToolkit::RegisterTabs()
{
	auto Register = [this](FName TabId, const FText& Label, const FText& Tooltip, FName Icon, TFunction<TSharedRef<SWidget>()> MakeContent)
	{
		TabManager->RegisterTabSpawner(TabId, FOnSpawnTab::CreateLambda([Label, MakeContent](const FSpawnTabArgs&)
		{
			return SNew(SDockTab).TabRole(ETabRole::PanelTab).Label(Label)[MakeContent()];
		}))
		.SetDisplayName(Label)
		.SetTooltipText(Tooltip)
		.SetIcon(FOnyxEditorStyle::Icon(Icon));
	};

	Register(ImportTabId, INVTEXT("Import"), INVTEXT("Drop PNGs or folders: tile sets, characters, sprites and backgrounds are built for you."), "Onyx2D.Tab.Import",
		[]() { return SNew(SOnyxImportTool); });
	Register(CharacterTabId, INVTEXT("Character Wizard"), INVTEXT("Turn a sprite sheet into a playable character."), "Onyx2D.Tab.Character",
		[this]() { return SAssignNew(CharacterTool, SOnyxCharacterTool); });
	Register(AnimationsTabId, INVTEXT("Animations"), INVTEXT("Edit a character's animations and frame events."), "Onyx2D.Tab.Animations",
		[]() { return SNew(SOnyxAnimationTool); });
	Register(TilesTabId, INVTEXT("Tiles"), INVTEXT("Create tile sets and tile maps."), "Onyx2D.Tab.Tiles",
		[this]() { return SAssignNew(TileTool, SOnyxTileTool).OnTileMapCreated_Lambda([this](UPaperTileMap* Map) { ShowPainter(Map); }); });
	Register(PainterTabId, INVTEXT("Level Painter"), INVTEXT("Paint tiles and place entities, LDtk style."), "Onyx2D.Tab.Painter",
		[this]() { return SAssignNew(PainterTool, SOnyxPainterTool); });
	Register(ParallaxTabId, INVTEXT("Parallax"), INVTEXT("Build the level's parallax background."), "Onyx2D.Tab.Parallax",
		[]() { return SNew(SOnyxParallaxTool); });
	Register(LevelTabId, INVTEXT("Level"), INVTEXT("Set up, populate and check the level."), "Onyx2D.Tab.Level",
		[]() { return SNew(SOnyxLevelTool); });
}

void SOnyxToolkit::SaveLayout()
{
	if (TabManager.IsValid())
	{
		FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, TabManager->PersistLayout());
	}
}

void SOnyxToolkit::ShowCharacterWizard(UTexture2D* SpriteSheet)
{
	TabManager->TryInvokeTab(CharacterTabId);
	if (SpriteSheet && CharacterTool.IsValid())
	{
		CharacterTool->SetSpriteSheet(SpriteSheet);
	}
}

void SOnyxToolkit::ShowTiles(UTexture2D* TileSheet)
{
	TabManager->TryInvokeTab(TilesTabId);
	if (TileSheet && TileTool.IsValid())
	{
		TileTool->SetTileSheet(TileSheet);
	}
}

void SOnyxToolkit::FocusTab(FName TabId)
{
	TabManager->TryInvokeTab(TabId);
}

void SOnyxToolkit::ShowPainter(UPaperTileMap* TileMap)
{
	TabManager->TryInvokeTab(PainterTabId);
	if (TileMap && PainterTool.IsValid())
	{
		PainterTool->SetTileMap(TileMap);
	}
}
