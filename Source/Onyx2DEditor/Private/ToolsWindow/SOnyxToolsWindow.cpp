// Copyright Onyx2D. All Rights Reserved.

#include "ToolsWindow/SOnyxToolsWindow.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/ConfigCacheIni.h"
#include "Style/OnyxEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolsWindow/OnyxToolWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "MovesetEditor/SOnyxAnimationEditor.h"
#include "ToolsWindow/SOnyxHomeTool.h"
#include "ToolsWindow/SOnyxImportTool.h"
#include "ToolsWindow/SOnyxLevelTool.h"
#include "ToolsWindow/SOnyxLibraryTool.h"
#include "ToolsWindow/SOnyxPainterTool.h"
#include "ToolsWindow/SOnyxParallaxTool.h"
#include "ToolsWindow/SOnyxTileTool.h"

const FName SOnyxToolsWindow::HomeTabId(TEXT("Onyx2D.Home"));
const FName SOnyxToolsWindow::ImportTabId(TEXT("Onyx2D.Import"));
const FName SOnyxToolsWindow::LibraryTabId(TEXT("Onyx2D.Library"));
const FName SOnyxToolsWindow::AnimationsTabId(TEXT("Onyx2D.Animations"));
const FName SOnyxToolsWindow::TilesTabId(TEXT("Onyx2D.Tiles"));
const FName SOnyxToolsWindow::PainterTabId(TEXT("Onyx2D.Painter"));
const FName SOnyxToolsWindow::ParallaxTabId(TEXT("Onyx2D.Parallax"));
const FName SOnyxToolsWindow::LevelTabId(TEXT("Onyx2D.Level"));

namespace
{
	const TCHAR* LayoutName = TEXT("Onyx2DTools_Layout_v7");
	const TCHAR* ConfigSection = TEXT("Onyx2D.Tools");

	struct FToolInfo
	{
		FName Id;
		FText Label;
		FText Tooltip;
		FName Icon;
		/** Sidebar group: 0 Start, 1 Create, 2 Level. */
		int32 Group;
		/** Its documentation page. */
		const TCHAR* DocsPage;
	};

	const FText GroupNames[] = { INVTEXT("START"), INVTEXT("CREATE"), INVTEXT("LEVEL") };

	/** Every tool, in tool bar and tab order. */
	const TArray<FToolInfo>& GetTools()
	{
		static const TArray<FToolInfo> Tools = {
			{ SOnyxToolsWindow::HomeTabId, INVTEXT("Home"), INVTEXT("Where to start: the steps from art to a playable level, and how to extend Onyx2D with Blueprints or C++."), "Onyx2D.Tab.Home", 0, TEXT("getting-started/quick-start") },
			{ SOnyxToolsWindow::ImportTabId, INVTEXT("Import"), INVTEXT("Drop PNGs or folders: tile sets, characters, sprites and backgrounds are built for you."), "Onyx2D.Tab.Import", 0, TEXT("getting-started/tools-window#import") },
			{ SOnyxToolsWindow::LibraryTabId, INVTEXT("Library"), INVTEXT("Browse and preview the project's characters, enemies, tile sets and animations; place them in the level."), "Onyx2D.Tab.Library", 0, TEXT("getting-started/tools-window#library") },
			{ SOnyxToolsWindow::AnimationsTabId, INVTEXT("Movesets"), INVTEXT("Moveset Editor: cut clips from sprite sheets, timeline with frame durations and events (messages, sounds, effects), make characters."), "Onyx2D.Tab.Animations", 1, TEXT("animation/moveset-editor") },
			{ SOnyxToolsWindow::TilesTabId, INVTEXT("Tiles"), INVTEXT("Create tile sets and tile maps."), "Onyx2D.Tab.Tiles", 1, TEXT("levels/tiles-collision") },
			{ SOnyxToolsWindow::PainterTabId, INVTEXT("Level Painter"), INVTEXT("Paint tiles and place entities, LDtk style."), "Onyx2D.Tab.Painter", 2, TEXT("levels/level-painter") },
			{ SOnyxToolsWindow::ParallaxTabId, INVTEXT("Parallax"), INVTEXT("Build the level's parallax background."), "Onyx2D.Tab.Parallax", 2, TEXT("levels/parallax") },
			{ SOnyxToolsWindow::LevelTabId, INVTEXT("Level"), INVTEXT("Set up, populate and check the level."), "Onyx2D.Tab.Level", 2, TEXT("getting-started/tools-window#level") },
		};
		return Tools;
	}

	/**
	 * One stack with only the Open tools as tabs. The other tools are listed as closed so that opening one later puts it
	 * in the same stack instead of a new window.
	 */
	TSharedRef<FTabManager::FLayout> MakeLayout(const TArray<FName>& Open, FName Front)
	{
		TSharedRef<FTabManager::FStack> Stack = FTabManager::NewStack();
		for (const FToolInfo& Tool : GetTools())
		{
			Stack->AddTab(Tool.Id, Open.Contains(Tool.Id) ? ETabState::OpenedTab : ETabState::ClosedTab);
		}
		Stack->SetForegroundTab(Front);
		return FTabManager::NewLayout(LayoutName)->AddArea(FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)->Split(Stack));
	}
}

void SOnyxToolsWindow::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& OwnerTab)
{
	OwnerTabWeak = OwnerTab;
	TabManager = FGlobalTabmanager::Get()->NewTabManager(OwnerTab);
	TabManager->SetAllowWindowMenuBar(false);
	RegisterTabs();

	const TSharedRef<FTabManager::FLayout> Layout = FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, MakeLayout({ HomeTabId }, HomeTabId));
	OwnerTab->SetOnPersistVisualState(SDockTab::FOnPersistVisualState::CreateSP(this, &SOnyxToolsWindow::SaveLayout));
	GConfig->GetBool(ConfigSection, TEXT("SidebarHidden"), bSidebarHidden, GEditorPerProjectIni);

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			MakeSidebar()
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			SAssignNew(TabArea, SBox)
			[
				TabManager->RestoreFrom(Layout, OwnerTab->GetParentWindow()).ToSharedRef()
			]
		]
	];
}

SOnyxToolsWindow::~SOnyxToolsWindow()
{
	if (TabManager.IsValid())
	{
		SaveLayout();
		for (const FToolInfo& Tool : GetTools())
		{
			TabManager->UnregisterTabSpawner(Tool.Id);
		}
	}
}

TSharedRef<SWidget> SOnyxToolsWindow::MakeSidebar()
{
	TSharedRef<SVerticalBox> Items = SNew(SVerticalBox);
	int32 Group = INDEX_NONE;
	for (const FToolInfo& Tool : GetTools())
	{
		if (Tool.Group != Group)
		{
			Group = Tool.Group;
			Items->AddSlot().AutoHeight().Padding(10.f, Group == 0 ? 8.f : 14.f, 10.f, 4.f)
			[
				SNew(SBox).HeightOverride(14.f)
				[
					SNew(STextBlock).Text(GroupNames[Group]).Font(OnyxTools::Font(8, true)).ColorAndOpacity(OnyxTools::Muted())
				]
			];
		}
		const FName Id = Tool.Id;
		Items->AddSlot().AutoHeight().Padding(4.f, 1.f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(0.f)
			.ToolTipText(FText::Format(INVTEXT("{0}\n{1}"), Tool.Label, Tool.Tooltip))
			.OnClicked_Lambda([this, Id]() { FocusTab(Id); return FReply::Handled(); })
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
				.BorderBackgroundColor_Lambda([this, Id]()
				{
					return IsTabInFront(Id) ? OnyxTools::Accent().CopyWithNewOpacity(0.22f) : FLinearColor::Transparent;
				})
				.Padding(FMargin(0.f, 5.f, 8.f, 5.f))
				[
					SNew(SHorizontalBox)
					// Open tools get a bar on the left: bright for the one in front.
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(3.f)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
							.BorderBackgroundColor_Lambda([this, Id]()
							{
								return IsTabInFront(Id) ? OnyxTools::Accent() : (IsTabOpen(Id) ? OnyxTools::Muted().CopyWithNewOpacity(0.6f) : FLinearColor::Transparent);
							})
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
					[
						SNew(SImage).Image(FOnyxEditorStyle::Get().GetBrush(Tool.Icon)).DesiredSizeOverride(FVector2D(16.f, 16.f))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock).Text(Tool.Label)
					]
				]
			]
		];
	}

	auto FooterButton = [](const FText& Label, const FText& Tooltip, TFunction<FName()> GetIcon, TFunction<void()> OnClicked)
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(Tooltip)
			.ContentPadding(FMargin(12.f, 5.f))
			.OnClicked_Lambda([OnClicked]() { OnClicked(); return FReply::Handled(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image_Lambda([GetIcon]() { return FAppStyle::GetBrush(GetIcon()); })
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock).Text(Label)
				]
			];
	};

	const auto SetHidden = [this](bool bHidden)
	{
		bSidebarHidden = bHidden;
		GConfig->SetBool(ConfigSection, TEXT("SidebarHidden"), bSidebarHidden, GEditorPerProjectIni);
	};
	// Hidden, only this strip is left: its arrow brings the sidebar back.
	const TSharedRef<SWidget> Strip = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Header"))
		.Padding(0.f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(1.f, 8.f))
			.VAlign(VAlign_Top)
			.ToolTipText(INVTEXT("Show the Onyx2D sidebar"))
			.OnClicked_Lambda([SetHidden]() { SetHidden(false); return FReply::Handled(); })
			[
				SNew(SImage).Image(FAppStyle::GetBrush("Icons.ChevronRight")).ColorAndOpacity(FSlateColor::UseForeground()).DesiredSizeOverride(FVector2D(12.f, 12.f))
			]
		];

	const TSharedRef<SWidget> Bar = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Header"))
		.Padding(0.f)
		[
			SNew(SBox)
			.WidthOverride(176.f)
			[
				SNew(SVerticalBox)
				// At the top: the arrow that hides the sidebar (the strip it leaves brings it back).
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 4.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock).Text(INVTEXT("ONYX2D")).Font(OnyxTools::Font(8, true)).ColorAndOpacity(OnyxTools::Muted())
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.ContentPadding(FMargin(4.f))
						.ToolTipText(INVTEXT("Hide the sidebar: the tools get the whole window (the arrow on the strip it leaves brings it back)"))
						.OnClicked_Lambda([SetHidden]() { SetHidden(true); return FReply::Handled(); })
						[
							SNew(SImage).Image(FAppStyle::GetBrush("Icons.ChevronLeft")).ColorAndOpacity(FSlateColor::UseForeground())
						]
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SScrollBox) + SScrollBox::Slot()[Items]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
				[
					FooterButton(INVTEXT("Help"), INVTEXT("Open the documentation for the tool in front (opens your browser)."),
						[]() { return FName("Icons.Help"); },
						[this]()
						{
							FString Page = TEXT("getting-started/tools-window");
							for (const FToolInfo& Tool : GetTools())
							{
								if (IsTabInFront(Tool.Id))
								{
									Page = Tool.DocsPage;
									break;
								}
							}
							OnyxTools::OpenDocs(Page);
						})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 6.f)
				[
					FooterButton(INVTEXT("Reset Layout"), INVTEXT("Close every tool and start again from Home."),
						[]() { return FName("Icons.Refresh"); }, [this]() { ResetLayout(); })
				]
			]
		];

	return SNew(SWidgetSwitcher)
		.WidgetIndex_Lambda([this]() { return bSidebarHidden ? 1 : 0; })
		+ SWidgetSwitcher::Slot()[Bar]
		+ SWidgetSwitcher::Slot()[Strip];
}

void SOnyxToolsWindow::RegisterTabs()
{
	for (const FToolInfo& Tool : GetTools())
	{
		const FName Id = Tool.Id;
		const FText Label = Tool.Label;
		TabManager->RegisterTabSpawner(Id, FOnSpawnTab::CreateLambda([this, Id, Label](const FSpawnTabArgs&)
		{
			TSharedRef<SWidget> Content = SNullWidget::NullWidget;
			if (Id == HomeTabId)
			{
				Content = SNew(SOnyxHomeTool).OnOpenTool_Lambda([this](FName Tab) { FocusTab(Tab); });
			}
			else if (Id == ImportTabId)
			{
				Content = SNew(SOnyxImportTool);
			}
			else if (Id == LibraryTabId)
			{
				Content = SNew(SOnyxLibraryTool)
					.OnEditAnimations_Lambda([this](UObject* Asset) { ShowAnimations(Asset); })
					.OnPaintTileMap_Lambda([this](UPaperTileMap* Map) { ShowPainter(Map); })
					.OnPaintTileSet_Lambda([this](UPaperTileSet* TileSet) { ShowPainterWithTileSet(TileSet); });
			}
			else if (Id == AnimationsTabId)
			{
				Content = SAssignNew(AnimationEditor, SOnyxAnimationEditor);
			}
			else if (Id == TilesTabId)
			{
				Content = SAssignNew(TileTool, SOnyxTileTool).OnTileMapCreated_Lambda([this](UPaperTileMap* Map) { ShowPainter(Map); });
			}
			else if (Id == PainterTabId)
			{
				Content = SAssignNew(PainterTool, SOnyxPainterTool);
			}
			else if (Id == ParallaxTabId)
			{
				Content = SNew(SOnyxParallaxTool);
			}
			else if (Id == LevelTabId)
			{
				Content = SNew(SOnyxLevelTool);
			}
			return SNew(SDockTab).TabRole(ETabRole::PanelTab).Label(Label)[Content];
		}))
		.SetDisplayName(Tool.Label)
		.SetTooltipText(Tool.Tooltip)
		.SetIcon(FOnyxEditorStyle::Icon(Tool.Icon));
	}
}

bool SOnyxToolsWindow::IsTabOpen(FName TabId) const
{
	return TabManager.IsValid() && TabManager->FindExistingLiveTab(FTabId(TabId)).IsValid();
}

bool SOnyxToolsWindow::IsTabInFront(FName TabId) const
{
	const TSharedPtr<SDockTab> Tab = TabManager.IsValid() ? TabManager->FindExistingLiveTab(FTabId(TabId)) : nullptr;
	return Tab.IsValid() && Tab->IsForeground();
}

void SOnyxToolsWindow::FocusTab(FName TabId)
{
	// With every tab closed there is no stack left to open into: start a fresh one holding just this tool.
	const bool bAnyOpen = GetTools().ContainsByPredicate([this](const FToolInfo& Tool) { return IsTabOpen(Tool.Id); });
	if (!bAnyOpen)
	{
		RestoreLayout(MakeLayout({ TabId }, TabId));
		return;
	}
	TabManager->TryInvokeTab(TabId);
}

void SOnyxToolsWindow::ResetLayout()
{
	RestoreLayout(MakeLayout({ HomeTabId }, HomeTabId));
}

void SOnyxToolsWindow::RestoreLayout(const TSharedRef<FTabManager::FLayout>& Layout)
{
	const TSharedPtr<SDockTab> OwnerTab = OwnerTabWeak.Pin();
	if (!TabManager.IsValid() || !OwnerTab.IsValid() || !TabArea.IsValid())
	{
		return;
	}
	TabManager->CloseAllAreas();
	TabArea->SetContent(TabManager->RestoreFrom(Layout, OwnerTab->GetParentWindow()).ToSharedRef());
	SaveLayout();
}

void SOnyxToolsWindow::SaveLayout()
{
	if (TabManager.IsValid())
	{
		FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, TabManager->PersistLayout());
	}
}

void SOnyxToolsWindow::ShowNewAnimation(UTexture2D* SpriteSheet)
{
	FocusTab(AnimationsTabId);
	if (AnimationEditor.IsValid())
	{
		AnimationEditor->NewAnimationFrom(SpriteSheet);
	}
}

void SOnyxToolsWindow::ShowTiles(UTexture2D* TileSheet)
{
	FocusTab(TilesTabId);
	if (TileSheet && TileTool.IsValid())
	{
		TileTool->SetTileSheet(TileSheet);
	}
}

void SOnyxToolsWindow::ShowAnimations(UObject* Asset)
{
	FocusTab(AnimationsTabId);
	if (Asset && AnimationEditor.IsValid())
	{
		AnimationEditor->Edit(Asset);
	}
}

void SOnyxToolsWindow::ShowPainter(UPaperTileMap* TileMap)
{
	FocusTab(PainterTabId);
	if (TileMap && PainterTool.IsValid())
	{
		PainterTool->SetTileMap(TileMap);
	}
}

void SOnyxToolsWindow::ShowPainterWithTileSet(UPaperTileSet* TileSet)
{
	FocusTab(PainterTabId);
	if (TileSet && PainterTool.IsValid())
	{
		PainterTool->SetTileSet(TileSet);
	}
}
