// Copyright Onyx2D. All Rights Reserved.

#include "ToolsWindow/SOnyxPainterTool.h"
#include "Builders/OnyxImageAnalysis.h"
#include "Engine/Texture2D.h"
#include "Level/OnyxLevelTools.h"
#include "Misc/ConfigCacheIni.h"
#include "ScopedTransaction.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SComboButton.h"
#include "World/OnyxParallaxBackground.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/SWindow.h"
#include "Builders/OnyxTerrains.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "AssetToolsModule.h"
#include "Builders/OnyxAssetUtils.h"
#include "Builders/OnyxTileBuilder.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "IDetailsView.h"
#include "Misc/TransactionObjectEvent.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PaperTileMap.h"
#include "PaperTileMapActor.h"
#include "PaperTileMapComponent.h"
#include "PaperTileSet.h"
#include "PropertyCustomizationHelpers.h"
#include "SAssetDropTarget.h"
#include "Style/OnyxEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "ToolsWindow/OnyxToolWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SOnyxTilePalette.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "World/OnyxTileCollision.h"

using namespace OnyxTools;

namespace
{
	/** Where the painter keeps its panel toggles (per project). */
	const TCHAR* PainterConfig = TEXT("Onyx2D.Painter");
	/** Wider than this, the toolbar shows its labels; narrower, icons only. */
	constexpr float ToolbarLabelsWidth = 1120.f;

	constexpr int32 EntitiesItem = -1;
	constexpr int32 DecorItem = -2;
	constexpr int32 BackgroundItem = -3;
	/** Image layers: -100 is the backdrop's layer 0, -101 its layer 1... */
	constexpr int32 ImageItemBase = -100;
	bool IsImageItem(int32 Item) { return Item <= ImageItemBase; }
	int32 ImageIndexOf(int32 Item) { return ImageItemBase - Item; }
	int32 ImageItemOf(int32 LayerIndex) { return ImageItemBase - LayerIndex; }
	bool IsTextureAsset(const FAssetData& Asset) { return Asset.IsInstanceOf(UTexture2D::StaticClass()); }

	/** Edges of an area object (water) grabbed to resize it. */
	enum EOnyxAreaEdge : int32
	{
		AreaEdgeLeft = 1,
		AreaEdgeRight = 2,
		AreaEdgeTop = 4,
		AreaEdgeBottom = 8,
	};
	/** How close (screen pixels) the mouse must be to an edge of the selected area to grab it. */
	constexpr float AreaEdgeGrab = 7.f;

	FName ObjectLayerOf(int32 LayerItem)
	{
		switch (LayerItem)
		{
		case EntitiesItem:   return FOnyxBrushLibrary::EntitiesLayer;
		case DecorItem:      return FOnyxBrushLibrary::DecorLayer;
		case BackgroundItem: return FOnyxBrushLibrary::BackgroundLayer;
		default:             return NAME_None;
		}
	}

	/** The backdrop's layers front to back (nearest first), as the game draws them over one another. */
	TArray<int32> BackdropOrder(const AOnyxParallaxBackground* Background)
	{
		TArray<int32> Order;
		if (Background)
		{
			for (int32 Index = 0; Index < Background->BackgroundLayers.Num(); ++Index)
			{
				Order.Add(Index);
			}
			const TArray<FOnyxParallaxLayer>& Layers = Background->BackgroundLayers;
			Order.StableSort([&Layers](int32 A, int32 B) { return Layers[A].Depth < Layers[B].Depth; });
		}
		return Order;
	}

	FText BackdropLayerKind(const FOnyxParallaxLayer& Layer)
	{
		return !Layer.Material.IsNull() ? INVTEXT("Mist") : (!Layer.Texture ? INVTEXT("Colour") : INVTEXT("Image"));
	}

	FText BackdropLayerTooltip(const FOnyxParallaxLayer& Layer)
	{
		return FText::Format(INVTEXT("{0} layer behind the tiles, {1} m back; it follows the camera {2}% sideways, {3}% up and down (100% = stays on screen, like a sky).\nClick it to edit it: drag its picture on the map, its corners to resize. Double-click to rename, right-click for more."),
			BackdropLayerKind(Layer), FText::AsNumber(FMath::RoundToInt(Layer.Depth / 100.f)),
			FText::AsNumber(FMath::RoundToInt(Layer.Factor.X * 100.f)), FText::AsNumber(FMath::RoundToInt(Layer.Factor.Y * 100.f)));
	}

	TSharedRef<SWidget> Chip(const TAttribute<FText>& Text)
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
			.Padding(FMargin(8.f, 3.f))
			[
				SNew(STextBlock).Text(Text).Font(Font(9)).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
	}

	TSharedRef<SWidget> IconButton(FName Icon, const FText& Tooltip, TFunction<void()> OnClicked, bool bOnyxStyle = false)
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(3.f))
			.ToolTipText(Tooltip)
			.OnClicked_Lambda([OnClicked]() { OnClicked(); return FReply::Handled(); })
			[
				SNew(SImage)
				.Image(bOnyxStyle ? FOnyxEditorStyle::Get().GetBrush(Icon) : FAppStyle::GetBrush(Icon))
				.ColorAndOpacity(FSlateColor::UseForeground())
			];
	}

	FText BrushKind(const FOnyxPainterBrush& Brush)
	{
		if (Brush.bProject)
		{
			return INVTEXT("Character");
		}
		if (Brush.IsArea())
		{
			return INVTEXT("Area · drag it out");
		}
		if (!Brush.bCustom)
		{
			return INVTEXT("Gameplay");
		}
		const UObject* Asset = Brush.Asset.ResolveObject();
		return Asset && Asset->IsA<UPaperFlipbook>() ? INVTEXT("Animated")
			: Asset && Asset->IsA<UPaperSprite>() ? INVTEXT("Sprite")
			: Asset && Asset->IsA<UBlueprint>() ? INVTEXT("Blueprint")
			: INVTEXT("Mesh");
	}
}

void SOnyxPainterTool::Construct(const FArguments& InArgs)
{
	Inspector = MakeDetailsView(nullptr);
	ShowOnlyCategories(Inspector.ToSharedRef(), AActor::StaticClass(), { TEXT("Onyx"), TEXT("TransformCommon"), TEXT("Sprite"), TEXT("Default") });
	RefreshBrushes();

	GConfig->GetBool(PainterConfig, TEXT("ShowPalette"), bShowPalette, GEditorPerProjectIni);
	GConfig->GetBool(PainterConfig, TEXT("ShowSidePanels"), bShowSidePanels, GEditorPerProjectIni);
	GConfig->GetBool(PainterConfig, TEXT("ParallaxPreview"), bShowParallax, GEditorPerProjectIni);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			// The toolbar with its labels when there is room, icons only when the panel is narrow (nothing hidden in a "more" menu).
			ToolbarStrip(SAssignNew(ToolbarArea, SBox)
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda([this]() { return ToolbarArea.IsValid() && ToolbarArea->GetCachedGeometry().GetLocalSize().X < ToolbarLabelsWidth ? 1 : 0; })
					+ SWidgetSwitcher::Slot()[MakeToolbar(true)]
					+ SWidgetSwitcher::Slot()[MakeToolbar(false)]
				], MakePanelToggles())
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			.PhysicalSplitterHandleSize(2.f)
			+ SSplitter::Slot().Value(0.25f)
			[
				SNew(SBox)
				.Visibility_Lambda([this]() { return bShowPalette ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					MakePalettePanel()
				]
			]
			+ SSplitter::Slot().Value(0.53f)
			[
				MakeCanvasArea()
			]
			+ SSplitter::Slot().Value(0.22f)
			[
				SNew(SBox)
				.Visibility_Lambda([this]() { return bShowSidePanels ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SSplitter)
					.Orientation(Orient_Vertical)
					.PhysicalSplitterHandleSize(2.f)
					+ SSplitter::Slot().Value(0.62f)[MakeLayersPanel()]
					+ SSplitter::Slot().Value(0.38f)[MakeInspector()]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			StatusBar(TAttribute<FText>::CreateSP(this, &SOnyxPainterTool::GetStatus))
		]
	];

	TransactedHandle = FCoreUObjectDelegates::OnObjectTransacted.AddSP(this, &SOnyxPainterTool::HandleObjectTransacted);
	RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SOnyxPainterTool::PollBackdrop));
	LibraryHandle = FOnyxBrushLibrary::Get().OnChanged.AddSP(this, &SOnyxPainterTool::RefreshBrushes);

	// Start on the first tile map already in the level.
	if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
	{
		for (TActorIterator<APaperTileMapActor> It(World); It; ++It)
		{
			UPaperTileMap* Map = It->GetRenderComponent() ? It->GetRenderComponent()->TileMap.Get() : nullptr;
			if (Map && Map->IsAsset())
			{
				SetTileMap(Map);
				break;
			}
		}
	}
	RefreshLayers();
}

SOnyxPainterTool::~SOnyxPainterTool()
{
	Painter.EndStroke();
	FCoreUObjectDelegates::OnObjectTransacted.Remove(TransactedHandle);
	FOnyxBrushLibrary::Get().OnChanged.Remove(LibraryHandle);
}

// --- Layout ------------------------------------------------------------------------

TSharedRef<SWidget> SOnyxPainterTool::MakeToolbar(bool bLabels)
{
	FSlimHorizontalToolBarBuilder Toolbar(nullptr, FMultiBoxCustomization::None);
	Toolbar.SetLabelVisibility(bLabels ? EVisibility::Visible : EVisibility::Collapsed);
	const FCanExecuteAction HasMap = FCanExecuteAction::CreateLambda([this]() { return Painter.GetTileMap() != nullptr; });

	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxPainterTool::CreateNewMap),
		FCanExecuteAction::CreateLambda([this]() { return Stamp.TileSet.IsValid(); })),
		NAME_None, INVTEXT("New Map"), INVTEXT("Create a tile map from the selected tile set and place it in the level."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"));
	Toolbar.AddSeparator();

	auto AddTool = [this, &Toolbar, &HasMap](EOnyxPaintTool InTool, const FText& Label, const FText& Tooltip, FName Icon)
	{
		Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([this, InTool]() { SetTool(InTool); }), HasMap,
			FIsActionChecked::CreateLambda([this, InTool]() { return Tool == InTool; })),
			NAME_None, Label, Tooltip, FOnyxEditorStyle::Icon(Icon), EUserInterfaceActionType::ToggleButton);
	};
	AddTool(EOnyxPaintTool::Brush, INVTEXT("Brush"), INVTEXT("Paint the selected tiles or object (B). Right-click erases."), "Onyx2D.Tool.Brush");
	AddTool(EOnyxPaintTool::Rect, INVTEXT("Rectangle"), INVTEXT("Drag a rectangle to fill it with the pattern (R). Right-drag clears it."), "Onyx2D.Tool.Rect");
	AddTool(EOnyxPaintTool::Fill, INVTEXT("Fill"), INVTEXT("Flood-fill the connected area with the pattern (F)."), "Onyx2D.Tool.Fill");
	AddTool(EOnyxPaintTool::Erase, INVTEXT("Erase"), INVTEXT("Erase tiles or objects on the active layer (E)."), "Onyx2D.Tool.Erase");
	AddTool(EOnyxPaintTool::Pick, INVTEXT("Pick"), INVTEXT("Pick the tile or object under the cursor as the brush (I)."), "Onyx2D.Tool.Pick");
	AddTool(EOnyxPaintTool::Select, INVTEXT("Select"), INVTEXT("Select and drag objects; edit them in the Inspector (S). Delete removes."), "Onyx2D.Tool.Select");
	AddTool(EOnyxPaintTool::Terrain, INVTEXT("Terrain"), INVTEXT("Auto-tile (T): paint rough shapes with the terrain picked in the palette; edges and corners choose themselves. Right-click erases."), "Onyx2D.Layer.Tiles");
	Toolbar.AddSeparator();

	// Brush options.
	auto AddOption = [&Toolbar](const FText& Label, const FText& Tooltip, FName Icon, TFunction<bool&()> Flag)
	{
		Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([Flag]() { Flag() = !Flag(); }), FCanExecuteAction(),
			FIsActionChecked::CreateLambda([Flag]() { return Flag(); })),
			NAME_None, Label, Tooltip, FSlateIcon(FAppStyle::GetAppStyleSetName(), Icon), EUserInterfaceActionType::ToggleButton);
	};
	AddOption(INVTEXT("Flip X"), INVTEXT("Paint the tiles mirrored left-right (X)."), "Icons.ArrowLeft", [this]() -> bool& { return Stamp.bFlipX; });
	AddOption(INVTEXT("Flip Y"), INVTEXT("Paint the tiles upside down (Y)."), "Icons.ArrowDown", [this]() -> bool& { return Stamp.bFlipY; });
	AddOption(INVTEXT("Random"), INVTEXT("Paint each cell with a random tile from the selected block: natural ground, grass and rocks (V)."), "Icons.Refresh", [this]() -> bool& { return Stamp.bRandom; });
	Toolbar.AddSeparator();

	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([this]() { bShowGrid = !bShowGrid; }), FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return bShowGrid; })),
		NAME_None, INVTEXT("Grid"), INVTEXT("Show the grid (G)."), FOnyxEditorStyle::Icon("Onyx2D.Tool.Grid"), EUserInterfaceActionType::ToggleButton);
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([this]() { bShowCollision = !bShowCollision; }), FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return bShowCollision; })),
		NAME_None, INVTEXT("Collision"), INVTEXT("Show what collides (C): green = solid, blue = one-way top, red = hazard. Set tile types in the palette."),
		FOnyxEditorStyle::Icon("Onyx2D.Collision"), EUserInterfaceActionType::ToggleButton);
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([this]()
		{
			bShowParallax = !bShowParallax;
			GConfig->SetBool(PainterConfig, TEXT("ParallaxPreview"), bShowParallax, GEditorPerProjectIni);
		}), FCanExecuteAction(), FIsActionChecked::CreateLambda([this]() { return bShowParallax; })),
		NAME_None, INVTEXT("Parallax"), INVTEXT("Parallax preview: the image layers slide as you pan, at their own speeds, as the game's camera sees them from the middle of the view. Off: they stay where they are placed, moving with the map."),
		FOnyxEditorStyle::Icon("Onyx2D.Tab.Parallax"), EUserInterfaceActionType::ToggleButton);
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([this]() { Canvas->RequestFit(); }), HasMap),
		NAME_None, INVTEXT("Fit"), INVTEXT("Frame the whole map (Home)."), FOnyxEditorStyle::Icon("Onyx2D.Tool.Fit"));
	Toolbar.AddSeparator();
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxPainterTool::Save), HasMap),
		NAME_None, INVTEXT("Save"), INVTEXT("Save the tile map and the level."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"));
	return Toolbar.MakeWidget();
}

TSharedRef<SWidget> SOnyxPainterTool::MakeMapPicker()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)
		[
			SNew(STextBlock).Text(INVTEXT("Tile map")).Font(Font(8)).ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(UPaperTileMap::StaticClass())
			.DisplayThumbnail(false)
			.AllowCreate(false)
			.ObjectPath_Lambda([this]() { return Painter.GetTileMap() ? Painter.GetTileMap()->GetPathName() : FString(); })
			.OnObjectChanged_Lambda([this](const FAssetData& Asset) { SetTileMap(Cast<UPaperTileMap>(Asset.GetAsset())); })
		];
}

TSharedRef<SWidget> SOnyxPainterTool::MakePanelToggles()
{
	auto Toggle = [this](const FText& Label, const FText& Tooltip, bool& Flag, const TCHAR* Key)
	{
		bool* FlagPtr = &Flag;
		return SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.ToolTipText(Tooltip)
			.IsChecked_Lambda([FlagPtr]() { return *FlagPtr ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([FlagPtr, Key](ECheckBoxState State)
			{
				*FlagPtr = State == ECheckBoxState::Checked;
				GConfig->SetBool(PainterConfig, Key, *FlagPtr, GEditorPerProjectIni);
			})
			[
				SNew(SBox).Padding(FMargin(8.f, 1.f))[SNew(STextBlock).Text(Label).Font(Font(9))]
			];
	};
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 2.f, 0.f)
		[
			Toggle(INVTEXT("Palette"), INVTEXT("Show or hide the palette (tiles and objects) on the left"), bShowPalette, TEXT("ShowPalette"))
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			Toggle(INVTEXT("Panels"), INVTEXT("Show or hide the layers, the backdrop and the inspector on the right"), bShowSidePanels, TEXT("ShowSidePanels"))
		];
}

TSharedRef<SWidget> SOnyxPainterTool::MakePalettePanel()
{
	// Tiles / Objects: what the left panel paints with (switches between the tile layer and the Entities layer).
	auto ModeToggle = [this](const FText& Label, const FText& Tooltip, bool bObjects)
	{
		return SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.ToolTipText(Tooltip)
			.IsChecked_Lambda([this, bObjects]() { return !IsImageLayer() && IsObjectLayer() == bObjects ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this, bObjects](ECheckBoxState)
			{
				if (IsImageLayer() || IsObjectLayer() != bObjects)
				{
					SelectLayer(bObjects ? EntitiesItem : Painter.GetLayer());
					RefreshLayers();
				}
			})
			[
				SNew(SBox).Padding(FMargin(2.f, 2.f)).HAlign(HAlign_Center)[SNew(STextBlock).Text(Label).Font(Font(9))]
			];
	};
	// Tiles | Objects across the whole panel, so both names fit even when it is narrow.
	const TSharedRef<SWidget> ModeRow = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 2.f, 0.f)
		[
			ModeToggle(INVTEXT("Tiles"), INVTEXT("Paint tiles on the selected tile layer."), false)
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			ModeToggle(INVTEXT("Objects"), INVTEXT("Place gameplay objects (player start, foes, pickups, lights...) and your own Blueprints. Select tool (S) picks and drags placed objects on any layer."), true)
		];

	// Zoom the tiles (smaller, fit the panel, bigger), in the header where there is room even in a narrow panel.
	const TSharedRef<SWidget> ZoomButtons = SNew(SHorizontalBox)
		.Visibility_Lambda([this]() { return IsObjectLayer() || IsImageLayer() ? EVisibility::Collapsed : EVisibility::Visible; })
		+ SHorizontalBox::Slot().AutoWidth()[IconButton("Icons.Minus", INVTEXT("Zoom out"), [this]() { PaletteZoom = FMath::Max((PaletteZoom > 0.f ? PaletteZoom : 2.f) - 1.f, 1.f); })]
		+ SHorizontalBox::Slot().AutoWidth()[IconButton("Onyx2D.Tool.Fit.Small", INVTEXT("Fit to the panel"), [this]() { PaletteZoom = 0.f; }, true)]
		+ SHorizontalBox::Slot().AutoWidth()[IconButton("Icons.Plus", INVTEXT("Zoom in"), [this]() { PaletteZoom = FMath::Min((PaletteZoom > 0.f ? PaletteZoom : 2.f) + 1.f, 12.f); })];

	return Section(TAttribute<FText>::CreateSP(this, &SOnyxPainterTool::GetPaletteTitle),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 6.f, 8.f, 0.f)
		[
			ModeRow
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex_Lambda([this]() { return IsImageLayer() ? 2 : (IsObjectLayer() ? 1 : 0); })
			+ SWidgetSwitcher::Slot()[MakeTilePalette()]
			+ SWidgetSwitcher::Slot()[MakeObjectPalette()]
			+ SWidgetSwitcher::Slot()[MakeImageLayerPanel()]
		],
		ZoomButtons);
}

TSharedRef<SWidget> SOnyxPainterTool::MakeTilePalette()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 6.f)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(UPaperTileSet::StaticClass())
			.DisplayThumbnail(false)
			.AllowCreate(false)
			.ObjectPath_Lambda([this]() { return Stamp.TileSet.IsValid() ? Stamp.TileSet->GetPathName() : FString(); })
			.OnObjectChanged_Lambda([this](const FAssetData& Asset) { SetTileSet(Cast<UPaperTileSet>(Asset.GetAsset())); })
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			// Fit mode follows the panel width (vertical scroll only); zoomed in, the sheet scrolls both ways.
			SNew(SWidgetSwitcher)
			.WidgetIndex_Lambda([this]() { return PaletteZoom > 0.f ? 1 : 0; })
			+ SWidgetSwitcher::Slot()
			[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)
				+ SScrollBox::Slot()[MakeTileSheetView()]
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)
				+ SScrollBox::Slot()
				[
					SNew(SScrollBox)
					.Orientation(Orient_Horizontal)
					+ SScrollBox::Slot()[MakeTileSheetView()]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeCollisionBar()
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeTerrainPanel()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 6.f)
		[
			SNew(STextBlock)
			.Font(Font(9))
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text_Lambda([this]()
			{
				return Stamp.Size == FIntPoint(1, 1)
					? INVTEXT("Click a tile, or drag across tiles to paint with a block.")
					: FText::Format(INVTEXT("Stamp: {0} x {1} tiles. Brush, Rectangle and Fill repeat it."), Stamp.Size.X, Stamp.Size.Y);
			})
		];
}

TSharedRef<SWidget> SOnyxPainterTool::MakeTileSheetView()
{
	return SNew(SOnyxTilePalette)
		.TileSet_Lambda([this]() { return Stamp.TileSet.Get(); })
		.Selection_Lambda([this]() { return FIntRect(Stamp.Min, Stamp.Min + Stamp.Size); })
		.Zoom_Lambda([this]() { return PaletteZoom; })
		.ShowCollision_Lambda([this]() { return bShowCollision; })
		.OnTilesSelected_Lambda([this](FIntPoint Min, FIntPoint Size)
		{
			Stamp.Min = Min;
			Stamp.Size = Size;
			if (Tool == EOnyxPaintTool::Erase || Tool == EOnyxPaintTool::Pick || Tool == EOnyxPaintTool::Select)
			{
				SetTool(EOnyxPaintTool::Brush);
			}
		});
}

TSharedRef<SWidget> SOnyxPainterTool::MakeCollisionBar()
{
	auto TypeButton = [this](EOnyxTileCollision Type, const FText& Tooltip) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ToolTipText(Tooltip)
			.ContentPadding(FMargin(6.f, 2.f))
			.IsEnabled_Lambda([this]() { return Stamp.IsValid(); })
			.OnClicked_Lambda([this, Type]() { Painter.SetTileCollision(Stamp, Type); return FReply::Handled(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SBox).WidthOverride(10.f).HeightOverride(10.f)
					[
						SNew(SImage).Image(FAppStyle::GetBrush("WhiteBrush")).ColorAndOpacity(FOnyxTileCollision::GetColor(Type))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FOnyxTileCollision::GetDisplayName(Type)).Font(Font(9))
				]
			];
	};

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Header"))
		.Padding(FMargin(8.f, 6.f))
		.Visibility_Lambda([this]() { return bShowCollision ? EVisibility::Visible : EVisibility::Collapsed; })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(STextBlock)
				.Font(Font(9, true))
				.Text_Lambda([this]()
				{
					const FPaperTileInfo Tile = Stamp.GetSourceTile(0, 0);
					return Stamp.Size == FIntPoint(1, 1) && Tile.IsValid()
						? FText::Format(INVTEXT("Tile collision: {0}"), FOnyxTileCollision::GetDisplayName(FOnyxTileCollision::Get(Tile.TileSet, Tile.GetTileIndex())))
						: FText::Format(INVTEXT("Set collision of the {0} selected tiles"), Stamp.Size.X * Stamp.Size.Y);
				})
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f))
				+ SWrapBox::Slot()[TypeButton(EOnyxTileCollision::None, INVTEXT("Decoration: characters pass through"))]
				+ SWrapBox::Slot()[TypeButton(EOnyxTileCollision::Solid, INVTEXT("Full block: ground, walls, ceilings"))]
				+ SWrapBox::Slot()[TypeButton(EOnyxTileCollision::OneWay, INVTEXT("Platform: stand on top, jump up through, drop with Down + Jump"))]
				+ SWrapBox::Slot()[TypeButton(EOnyxTileCollision::Hazard, INVTEXT("Hurts on touch (spikes, lava)"))]
				+ SWrapBox::Slot()[TypeButton(EOnyxTileCollision::Edge, INVTEXT("Blocks crossing the tile's sides (cliff rims and fences from above)"))]
				+ SWrapBox::Slot()[TypeButton(EOnyxTileCollision::SlopeUp, INVTEXT("45 degree slope rising to the right: a diagonal row of them makes one smooth ramp"))]
				+ SWrapBox::Slot()[TypeButton(EOnyxTileCollision::SlopeDown, INVTEXT("45 degree slope going down to the right: a diagonal row of them makes one smooth ramp"))]
				+ SWrapBox::Slot()
				[
					SNew(SButton)
					.Text(INVTEXT("Auto"))
					.ToolTipText(INVTEXT("Re-classify the whole tile set from its pixels: mostly opaque tiles become Solid, the rest None"))
					.ContentPadding(FMargin(6.f, 2.f))
					.IsEnabled_Lambda([this]() { return Stamp.TileSet.IsValid(); })
					.OnClicked_Lambda([this]() { Painter.AutoAssignCollision(Stamp.TileSet.Get()); return FReply::Handled(); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				Hint(INVTEXT("Types belong to the tile set, so every map using it updates. Only layers with the collision toggle on (Layers panel) collide."))
			]
		];
}

TSharedRef<SWidget> SOnyxPainterTool::MakeObjectPalette()
{
	SAssignNew(BrushList, SListView<TSharedPtr<FOnyxPainterBrush>>)
		.ListItemsSource(&BrushItems)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SOnyxPainterTool::GenerateBrushRow)
		.OnSelectionChanged_Lambda([this](TSharedPtr<FOnyxPainterBrush> Item, ESelectInfo::Type)
		{
			if (Item.IsValid())
			{
				SelectedBrush = Item;
				if (Tool == EOnyxPaintTool::Erase || Tool == EOnyxPaintTool::Pick || Tool == EOnyxPaintTool::Select)
				{
					SetTool(EOnyxPaintTool::Brush);
				}
			}
		});

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SAssetDropTarget)
			.bSupportsMultiDrop(true)
			.OnAreAssetsAcceptableForDrop_Lambda([](TArrayView<FAssetData> Assets)
			{
				return Assets.ContainsByPredicate([](const FAssetData& Asset) { return FOnyxBrushLibrary::CanBeBrush(Asset); });
			})
			.OnAssetsDropped_Lambda([](const FDragDropEvent&, TArrayView<FAssetData> Assets)
			{
				const int32 Added = FOnyxBrushLibrary::Get().AddAssets(TArray<FAssetData>(Assets));
				Notify(Added > 0 ? FText::Format(INVTEXT("Added {0} object brush(es)"), Added) : INVTEXT("Those are already in the palette."), Added > 0);
			})
			[
				BrushList.ToSharedRef()
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 6.f, 8.f, 2.f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.ToolTipText(INVTEXT("Add the Blueprints, flipbooks, sprites or static meshes selected in the Content Browser"))
			.OnClicked_Lambda([this]() { AddSelectedAssetsAsBrushes(); return FReply::Handled(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SImage).Image(FAppStyle::GetBrush("Icons.Plus")).ColorAndOpacity(FSlateColor::UseForeground())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(INVTEXT("Add Selected Assets"))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 2.f, 8.f, 6.f)
		[
			Hint(INVTEXT("Or drag Blueprints, animated flipbooks, sprites or meshes from the Content Browser into this list. They are painted as real actors, so their animation and Blueprint logic keep working."))
		];
}

TSharedRef<ITableRow> SOnyxPainterTool::GenerateBrushRow(TSharedPtr<FOnyxPainterBrush> Item, const TSharedRef<STableViewBase>& Owner)
{
	const FName Id = Item->Id;
	return SNew(STableRow<TSharedPtr<FOnyxPainterBrush>>, Owner)
		.Padding(FMargin(6.f, 4.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(SBox).WidthOverride(36.f).HeightOverride(36.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SImage).Image_Lambda([this, Item]() { return GetBrushThumbnail(Item); })
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Item->Label).Font(Font(9, true))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(BrushKind(*Item)).Font(Font(8)).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox)
				.Visibility(Item->bCustom && !Item->bProject ? EVisibility::Visible : EVisibility::Collapsed)
				[
					IconButton("Icons.Delete", INVTEXT("Remove from the palette (placed objects stay)"), [Id]() { FOnyxBrushLibrary::Get().Remove(Id); })
				]
			]
		];
}

TSharedRef<SWidget> SOnyxPainterTool::MakeLayersPanel()
{
	SAssignNew(LayerList, SListView<TSharedPtr<int32>>)
		.ListItemsSource(&LayerItems)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SOnyxPainterTool::GenerateLayerRow)
		.OnContextMenuOpening(this, &SOnyxPainterTool::MakeLayerContextMenu)
		.OnSelectionChanged_Lambda([this](TSharedPtr<int32> Item, ESelectInfo::Type) { if (Item.IsValid()) { SelectLayer(*Item); } });

	const TSharedRef<SWidget> AddLayer = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			IconButton("Icons.FolderClosed", INVTEXT("Tidy the World Outliner: what stands in a tile map goes in that map's folder, by kind (Characters, Lights, Props, Pickups, Gameplay). Folders of your own are left alone."), []()
			{
				const FScopedTransaction Transaction(INVTEXT("Tidy World Outliner"));
				const int32 Moved = FOnyxLevelTools::OrganizeOutliner(FOnyxLevelTools::GetEditorWorld());
				Notify(Moved > 0 ? FText::Format(INVTEXT("Filed {0} actor(s) in the World Outliner by map and kind"), Moved) : INVTEXT("The World Outliner is already tidy."), true);
			})
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			MakeAddLayerMenu()
		];
	return Section(INVTEXT("Layers"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 6.f)
		[
			MakeMapPicker()
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			// Textures dropped here become image layers in the middle of the view.
			SNew(SAssetDropTarget)
			.bSupportsMultiDrop(true)
			.OnAreAssetsAcceptableForDrop_Lambda([this](TArrayView<FAssetData> Assets) { return Painter.FindMapActor() && Assets.ContainsByPredicate(IsTextureAsset); })
			.OnAssetsDropped_Lambda([this](const FDragDropEvent&, TArrayView<FAssetData> Assets)
			{
				TArray<UTexture2D*> Textures;
				for (const FAssetData& Asset : Assets)
				{
					if (UTexture2D* Texture = Cast<UTexture2D>(Asset.GetAsset()))
					{
						Textures.Add(Texture);
					}
				}
				AddImageLayers(Textures, {});
			})
			[
				LayerList.ToSharedRef()
			]
		],
		AddLayer);
}

TSharedRef<SWidget> SOnyxPainterTool::MakeAddLayerMenu()
{
	return SNew(SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(3.f))
		.ToolTipText(INVTEXT("Add a layer: tiles, or a picture behind the tiles"))
		.IsEnabled_Lambda([this]() { return Painter.GetTileMap() != nullptr; })
		.ButtonContent()
		[
			SNew(SImage).Image(FAppStyle::GetBrush("Icons.Plus")).ColorAndOpacity(FSlateColor::UseForeground())
		]
		.OnGetMenuContent_Lambda([this]()
		{
			FMenuBuilder Menu(true, nullptr);
			Menu.AddMenuEntry(INVTEXT("Tile Layer"), INVTEXT("A tile layer in front of the others"), FOnyxEditorStyle::Icon("Onyx2D.Layer.Tiles"),
				FUIAction(FExecuteAction::CreateLambda([this]() { Painter.AddLayer(); RefreshLayers(); })));
			Menu.AddMenuEntry(INVTEXT("Image Layer"), INVTEXT("A picture behind the tiles, in the middle of the view: one per texture selected in the Content Browser. Or drop textures on the map, where they should go."),
				FOnyxEditorStyle::Icon("Onyx2D.Tab.Parallax"),
				FUIAction(FExecuteAction::CreateLambda([this]()
				{
					const TArray<UTexture2D*> Textures = GetSelectedTextures();
					if (Textures.IsEmpty())
					{
						Notify(INVTEXT("Select textures in the Content Browser first, or drop them on the map."), false);
						return;
					}
					AddImageLayers(Textures, {});
				})));
			Menu.AddMenuEntry(INVTEXT("Colour Layer"), INVTEXT("A plain colour band behind the tiles, across the level (a sky)"), FOnyxEditorStyle::Icon("Onyx2D.Tab.Parallax"),
				FUIAction(FExecuteAction::CreateLambda([this]() { AddImageLayers({}, {}); })));
			return Menu.MakeWidget();
		});
}

TSharedRef<ITableRow> SOnyxPainterTool::GenerateLayerRow(TSharedPtr<int32> Item, const TSharedRef<STableViewBase>& Owner)
{
	const int32 LayerItem = *Item;
	if (IsImageItem(LayerItem))
	{
		return GenerateImageLayerRow(ImageIndexOf(LayerItem), Item, Owner);
	}
	const bool bObjects = LayerItem < 0;
	return SNew(STableRow<TSharedPtr<int32>>, Owner)
		.Padding(FMargin(4.f, 4.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(2.f))
				.ToolTipText(INVTEXT("Show / hide"))
				.OnClicked_Lambda([this, LayerItem]() { ToggleLayerVisibility(LayerItem); return FReply::Handled(); })
				[
					SNew(SImage).Image_Lambda([this, LayerItem]()
					{
						return FOnyxEditorStyle::Get().GetBrush(IsLayerVisible(LayerItem) ? "Onyx2D.Eye.Small" : "Onyx2D.EyeOff.Small");
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)
			[
				SNew(SImage).Image(FOnyxEditorStyle::Get().GetBrush(bObjects ? "Onyx2D.Layer.Entities.Small" : "Onyx2D.Layer.Tiles.Small"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SInlineEditableTextBlock)
				.Text_Lambda([this, LayerItem]() { return GetLayerTitle(LayerItem); })
				.Font(Font(9, true))
				.IsReadOnly(bObjects)
				.ToolTipText(bObjects ? INVTEXT("Object layer: actors painted onto the map") : INVTEXT("Tile layer. Double-click to rename, right-click for more."))
				.OnTextCommitted_Lambda([this, LayerItem](const FText& Text, ETextCommit::Type) { Painter.RenameLayer(LayerItem, Text); })
			]
			// Collision toggle for tile layers.
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(2.f))
				.Visibility(bObjects ? EVisibility::Collapsed : EVisibility::Visible)
				.ToolTipText_Lambda([this, LayerItem]() { return Painter.GetLayerCollides(LayerItem) ? INVTEXT("Solid: characters collide with these tiles (click to make it decoration)") : INVTEXT("Decoration: no collision (click to make it solid)"); })
				.OnClicked_Lambda([this, LayerItem]() { Painter.SetLayerCollides(LayerItem, !Painter.GetLayerCollides(LayerItem)); return FReply::Handled(); })
				[
					SNew(SImage)
					.Image(FOnyxEditorStyle::Get().GetBrush("Onyx2D.Collision.Small"))
					.ColorAndOpacity_Lambda([this, LayerItem]() { return Painter.GetLayerCollides(LayerItem) ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.18f); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 2.f, 0.f)
			[
				SNew(STextBlock)
				.Text(bObjects ? INVTEXT("Objects") : INVTEXT("Tiles"))
				.Font(Font(8))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
}

TSharedPtr<SWidget> SOnyxPainterTool::MakeLayerContextMenu()
{
	const TArray<TSharedPtr<int32>> Selection = LayerList->GetSelectedItems();
	if (!Selection.IsEmpty() && IsImageItem(*Selection[0]) && GetImageLayer(ImageIndexOf(*Selection[0])))
	{
		const int32 Index = ImageIndexOf(*Selection[0]);
		// The image layers' order in the list (nearest first): moving one swaps its distance with its neighbour's.
		TArray<int32> Images;
		for (const TSharedPtr<int32>& Item : LayerItems)
		{
			if (IsImageItem(*Item))
			{
				Images.Add(ImageIndexOf(*Item));
			}
		}
		const int32 Position = Images.IndexOfByKey(Index);
		const int32 Nearer = Position > 0 ? Images[Position - 1] : INDEX_NONE;
		const int32 Farther = Images.IsValidIndex(Position + 1) ? Images[Position + 1] : INDEX_NONE;
		auto SwapDepth = [this, Index](int32 Other)
		{
			EditBackdrop(INVTEXT("Move Image Layer"), [Index, Other](AOnyxParallaxBackground& Background)
			{
				if (Background.BackgroundLayers.IsValidIndex(Index) && Background.BackgroundLayers.IsValidIndex(Other))
				{
					Swap(Background.BackgroundLayers[Index].Depth, Background.BackgroundLayers[Other].Depth);
				}
			});
			RefreshLayers();
		};
		FMenuBuilder Menu(true, nullptr);
		Menu.AddMenuEntry(INVTEXT("Move Forward"), INVTEXT("Draw it in front of the image layer above it"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.ArrowUp"),
			FUIAction(FExecuteAction::CreateLambda([SwapDepth, Nearer]() { SwapDepth(Nearer); }), FCanExecuteAction::CreateLambda([Nearer]() { return Nearer != INDEX_NONE; })));
		Menu.AddMenuEntry(INVTEXT("Move Back"), INVTEXT("Draw it behind the image layer below it"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.ArrowDown"),
			FUIAction(FExecuteAction::CreateLambda([SwapDepth, Farther]() { SwapDepth(Farther); }), FCanExecuteAction::CreateLambda([Farther]() { return Farther != INDEX_NONE; })));
		Menu.AddMenuEntry(INVTEXT("Use Selected Texture"), INVTEXT("Show the texture selected in the Content Browser on this layer"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"),
			FUIAction(FExecuteAction::CreateLambda([this, Index]()
			{
				const TArray<UTexture2D*> Textures = GetSelectedTextures();
				if (Textures.IsEmpty())
				{
					Notify(INVTEXT("Select a texture in the Content Browser first."), false);
					return;
				}
				EditBackdrop(INVTEXT("Set Image Layer Picture"), [Index, Texture = Textures[0]](AOnyxParallaxBackground& Background)
				{
					if (Background.BackgroundLayers.IsValidIndex(Index))
					{
						Background.BackgroundLayers[Index].Texture = Texture;
					}
				});
			})));
		Menu.AddMenuEntry(INVTEXT("Duplicate"), INVTEXT("A copy of this layer, just in front of it"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Duplicate"),
			FUIAction(FExecuteAction::CreateLambda([this, Index]()
			{
				const int32 Copy = FindBackdrop() ? FindBackdrop()->BackgroundLayers.Num() : INDEX_NONE;
				EditBackdrop(INVTEXT("Duplicate Image Layer"), [Index](AOnyxParallaxBackground& Background)
				{
					if (Background.BackgroundLayers.IsValidIndex(Index))
					{
						FOnyxParallaxLayer Layer = Background.BackgroundLayers[Index];
						Layer.Depth = FMath::Max(Layer.Depth - 1.f, 1.f);
						Background.BackgroundLayers.Add(Layer);
					}
				});
				SelectLayer(ImageItemOf(Copy));
				RefreshLayers();
			})));
		Menu.AddSeparator();
		Menu.AddMenuEntry(INVTEXT("Remove Layer"), INVTEXT("Take this picture out of the level (Delete)"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
			FUIAction(FExecuteAction::CreateLambda([this, Index]() { RemoveImageLayer(Index); })));
		return Menu.MakeWidget();
	}
	if (Selection.IsEmpty() || *Selection[0] < 0)
	{
		return nullptr;
	}
	const int32 LayerIndex = *Selection[0];
	FMenuBuilder Menu(true, nullptr);
	Menu.AddMenuEntry(INVTEXT("Move Forward"), INVTEXT("Draw this layer in front of the one above it"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.ArrowUp"),
		FUIAction(FExecuteAction::CreateLambda([this, LayerIndex]() { Painter.MoveLayer(LayerIndex, -1); RefreshLayers(); }),
			FCanExecuteAction::CreateLambda([LayerIndex]() { return LayerIndex > 0; })));
	Menu.AddMenuEntry(INVTEXT("Move Back"), INVTEXT("Draw this layer behind the one below it"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.ArrowDown"),
		FUIAction(FExecuteAction::CreateLambda([this, LayerIndex]() { Painter.MoveLayer(LayerIndex, 1); RefreshLayers(); }),
			FCanExecuteAction::CreateLambda([this, LayerIndex]() { return LayerIndex < Painter.GetLayerCount() - 1; })));
	Menu.AddMenuEntry(INVTEXT("Toggle Collision"), INVTEXT("Solid ground or decoration"), FOnyxEditorStyle::Icon("Onyx2D.Collision"),
		FUIAction(FExecuteAction::CreateLambda([this, LayerIndex]() { Painter.SetLayerCollides(LayerIndex, !Painter.GetLayerCollides(LayerIndex)); })));
	Menu.AddSeparator();
	Menu.AddMenuEntry(INVTEXT("Delete Layer"), INVTEXT("Remove this tile layer and its tiles"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
		FUIAction(FExecuteAction::CreateLambda([this, LayerIndex]() { Painter.DeleteLayer(LayerIndex); RefreshLayers(); }),
			FCanExecuteAction::CreateLambda([this]() { return Painter.GetLayerCount() > 1; })));
	return Menu.MakeWidget();
}

TSharedRef<ITableRow> SOnyxPainterTool::GenerateImageLayerRow(int32 LayerIndex, const TSharedPtr<int32>& Item, const TSharedRef<STableViewBase>& Owner)
{
	const int32 Index = LayerIndex;
	const int32 LayerItem = *Item;
	auto LayerOf = [this, Index]() -> const FOnyxParallaxLayer*
	{
		const AOnyxParallaxBackground* Background = FindBackdrop();
		return Background && Background->BackgroundLayers.IsValidIndex(Index) ? &Background->BackgroundLayers[Index] : nullptr;
	};
	auto PictureOf = [LayerOf]() -> UTexture2D*
	{
		const FOnyxParallaxLayer* Layer = LayerOf();
		return Layer && Layer->Material.IsNull() ? Layer->Texture.Get() : nullptr;
	};
	// The layer's picture fitted into the row (its shape kept), or a swatch of its colour.
	TSharedRef<FSlateBrush> Picture = MakeShared<FSlateBrush>();
	Picture->DrawAs = ESlateBrushDrawType::Image;
	constexpr float PictureWidth = 32.f;
	constexpr float PictureHeight = 18.f;

	return SNew(STableRow<TSharedPtr<int32>>, Owner)
		.Padding(FMargin(4.f, 4.f))
		.ToolTipText_Lambda([LayerOf]() { const FOnyxParallaxLayer* Layer = LayerOf(); return Layer ? BackdropLayerTooltip(*Layer) : FText::GetEmpty(); })
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(2.f))
				.ToolTipText(INVTEXT("Show / hide (here only: the game still shows it)"))
				.OnClicked_Lambda([this, LayerItem]() { ToggleLayerVisibility(LayerItem); return FReply::Handled(); })
				[
					SNew(SImage).Image_Lambda([this, LayerItem]()
					{
						return FOnyxEditorStyle::Get().GetBrush(IsLayerVisible(LayerItem) ? "Onyx2D.Eye.Small" : "Onyx2D.EyeOff.Small");
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)
			[
				SNew(SBox).WidthOverride(PictureWidth).HeightOverride(PictureHeight)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("WhiteBrush"))
						.Visibility_Lambda([PictureOf]() { return PictureOf() ? EVisibility::Collapsed : EVisibility::Visible; })
						.ColorAndOpacity_Lambda([LayerOf]()
						{
							const FOnyxParallaxLayer* Layer = LayerOf();
							return FSlateColor(Layer ? FLinearColor(Layer->Tint.R, Layer->Tint.G, Layer->Tint.B, 1.f) : FLinearColor::Black);
						})
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Visibility_Lambda([PictureOf]() { return PictureOf() ? EVisibility::Visible : EVisibility::Collapsed; })
						.Image_Lambda([PictureOf, Picture]() -> const FSlateBrush*
						{
							UTexture2D* Texture = PictureOf();
							if (Texture && Picture->GetResourceObject() != Texture)
							{
								Picture->SetResourceObject(Texture);
							}
							const FVector2D Size(FMath::Max(Texture ? Texture->GetSurfaceWidth() : 1.f, 1.f), FMath::Max(Texture ? Texture->GetSurfaceHeight() : 1.f, 1.f));
							const double Scale = FMath::Min(PictureWidth / Size.X, PictureHeight / Size.Y);
							Picture->ImageSize = Size * Scale;
							return &Picture.Get();
						})
					]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SInlineEditableTextBlock)
				.Text_Lambda([LayerOf]() { const FOnyxParallaxLayer* Layer = LayerOf(); return Layer ? FText::FromName(Layer->Name) : FText::GetEmpty(); })
				.Font(Font(9, true))
				.OnTextCommitted_Lambda([this, Index](const FText& Text, ETextCommit::Type)
				{
					EditBackdrop(INVTEXT("Rename Image Layer"), [Index, Name = FName(*Text.ToString())](AOnyxParallaxBackground& Background)
					{
						if (Background.BackgroundLayers.IsValidIndex(Index))
						{
							Background.BackgroundLayers[Index].Name = Name;
						}
					});
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 2.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([LayerOf]() { const FOnyxParallaxLayer* Layer = LayerOf(); return Layer ? BackdropLayerKind(*Layer) : FText::GetEmpty(); })
				.Font(Font(8))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
}

TSharedRef<SWidget> SOnyxPainterTool::MakeImageLayerPanel()
{
	auto Row = [](const FText& Label, const FText& Tooltip, const TSharedRef<SWidget>& Content)
	{
		return SNew(SHorizontalBox)
			.ToolTipText(Tooltip)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(92.f)[SNew(STextBlock).Text(Label)]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				Content
			];
	};
	// A number: dragged (one undo step for the whole drag, the map following it) or typed.
	auto Number = [this](float Min, float Max, float Delta, TFunction<float(const FOnyxParallaxLayer&)> Get, TFunction<void(FOnyxParallaxLayer&, float)> Set, FText Title)
	{
		return SNew(SSpinBox<float>)
			.MinValue(Min)
			.MaxValue(Max)
			.Delta(Delta)
			.Value_Lambda([this, Get]() { const FOnyxParallaxLayer* Layer = GetImageLayer(ActiveImageLayer); return Layer ? Get(*Layer) : 0.f; })
			.OnBeginSliderMovement_Lambda([this, Title]() { BeginImageLayerChange(Title); })
			.OnValueChanged_Lambda([this, Set](float Value)
			{
				if (ImageChange)
				{
					ApplyImageLayerLive([&Set, Value](FOnyxParallaxLayer& Layer) { Set(Layer, Value); });
				}
			})
			.OnEndSliderMovement_Lambda([this](float) { EndImageLayerChange(); })
			.OnValueCommitted_Lambda([this, Set, Title](float Value, ETextCommit::Type)
			{
				if (ImageChange)
				{
					ApplyImageLayerLive([&Set, Value](FOnyxParallaxLayer& Layer) { Set(Layer, Value); });
				}
				else
				{
					EditImageLayer(Title, [&Set, Value](FOnyxParallaxLayer& Layer) { Set(Layer, Value); });
				}
			});
	};
	auto HasPicture = [this]() { const FOnyxParallaxLayer* Layer = GetImageLayer(ActiveImageLayer); return Layer && Layer->Texture && Layer->Material.IsNull(); };

	return SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(10.f, 8.f, 10.f, 4.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
			[
				Hint(INVTEXT("A picture behind the tiles. Drag it on the map to move it, drag a corner to resize it. Drop more textures on the map for more layers."))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				Row(INVTEXT("Picture"), INVTEXT("The texture it shows (none: a plain colour band)"),
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UTexture2D::StaticClass())
					.DisplayThumbnail(false)
					.AllowCreate(false)
					.ObjectPath_Lambda([this]() { const FOnyxParallaxLayer* Layer = GetImageLayer(ActiveImageLayer); return Layer && Layer->Texture ? Layer->Texture->GetPathName() : FString(); })
					.OnObjectChanged_Lambda([this](const FAssetData& Asset)
					{
						UTexture2D* Texture = Cast<UTexture2D>(Asset.GetAsset());
						const FOnyxParallaxLayer* Layer = GetImageLayer(ActiveImageLayer);
						const float Scale = Layer && Layer->Texture ? GetImageLayerScale(*Layer) : 1.f;
						const double CmPerPixel = GetCmPerMapPixel();
						EditImageLayer(INVTEXT("Set Image Layer Picture"), [Texture, Scale, CmPerPixel](FOnyxParallaxLayer& Edited)
						{
							Edited.Texture = Texture;
							if (Texture && CmPerPixel > 0.0)
							{
								Edited.Height = float(Texture->GetImportedSize().Y * CmPerPixel * Scale); // the same size per pixel as before
							}
						});
					}))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				SNew(SBox)
				.Visibility_Lambda([HasPicture]() { return HasPicture() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					Row(INVTEXT("Size"), INVTEXT("1 = its pixels as big as the map's; 2 = twice as big"),
						Number(0.05f, 32.f, 0.05f,
							[this](const FOnyxParallaxLayer& Layer) { return GetImageLayerScale(Layer); },
							[this](FOnyxParallaxLayer& Layer, float Value)
							{
								const double CmPerPixel = GetCmPerMapPixel();
								if (Layer.Texture && CmPerPixel > 0.0)
								{
									Layer.Height = FMath::Max(float(Layer.Texture->GetImportedSize().Y * CmPerPixel * Value), 1.f);
								}
							}, INVTEXT("Resize Image Layer")))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				SNew(SBox)
				.Visibility_Lambda([HasPicture]() { return HasPicture() ? EVisibility::Collapsed : EVisibility::Visible; })
				[
					Row(INVTEXT("Height (m)"), INVTEXT("How tall the band is"),
						Number(0.5f, 1000.f, 0.5f,
							[](const FOnyxParallaxLayer& Layer) { return Layer.Height / 100.f; },
							[](FOnyxParallaxLayer& Layer, float Value) { Layer.Height = FMath::Max(Value * 100.f, 1.f); }, INVTEXT("Resize Image Layer")))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				SNew(SBox)
				.Visibility_Lambda([HasPicture]() { return HasPicture() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					Row(INVTEXT("Repeat"), INVTEXT("Repeat the picture sideways across the level (a skyline). Off: once, where you put it."),
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() { const FOnyxParallaxLayer* Layer = GetImageLayer(ActiveImageLayer); return Layer && Layer->bRepeat ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
						{
							EditImageLayer(INVTEXT("Repeat Image Layer"), [State](FOnyxParallaxLayer& Layer) { Layer.bRepeat = State == ECheckBoxState::Checked; });
						}))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 3.f)
			[
				SNew(STextBlock).Text(INVTEXT("Parallax")).Font(Font(9, true))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				Row(INVTEXT("Sideways"), INVTEXT("How much it follows the camera sideways: 0 = moves with the level, 1 = stays on screen (a sky). Far things: high."),
					Number(0.f, 1.f, 0.01f,
						[](const FOnyxParallaxLayer& Layer) { return float(Layer.Factor.X); },
						[](FOnyxParallaxLayer& Layer, float Value) { Layer.Factor.X = Value; }, INVTEXT("Image Layer Parallax")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				Row(INVTEXT("Up and down"), INVTEXT("How much it follows the camera up and down: 0 = moves with the level, 1 = stays on screen."),
					Number(0.f, 1.f, 0.01f,
						[](const FOnyxParallaxLayer& Layer) { return float(Layer.Factor.Y); },
						[](FOnyxParallaxLayer& Layer, float Value) { Layer.Factor.Y = Value; }, INVTEXT("Image Layer Parallax")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 3.f)
			[
				SNew(STextBlock).Text(INVTEXT("Look")).Font(Font(9, true))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				Row(INVTEXT("Colour"), INVTEXT("Tints the picture (white = as drawn); its alpha fades it. The level's mood tints it too."),
					SNew(SColorBlock)
					.Color_Lambda([this]() { const FOnyxParallaxLayer* Layer = GetImageLayer(ActiveImageLayer); return Layer ? Layer->Tint : FLinearColor::White; })
					.ShowBackgroundForAlpha(true)
					.AlphaDisplayMode(EColorBlockAlphaDisplayMode::Separate)
					.Size(FVector2D(120.f, 18.f))
					.OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent&)
					{
						const FOnyxParallaxLayer* Layer = GetImageLayer(ActiveImageLayer);
						if (!Layer)
						{
							return FReply::Handled();
						}
						BeginImageLayerChange(INVTEXT("Image Layer Colour"));
						FColorPickerArgs Picker(Layer->Tint, FOnLinearColorValueChanged::CreateLambda([this](FLinearColor Color)
						{
							ApplyImageLayerLive([Color](FOnyxParallaxLayer& Edited) { Edited.Tint = Color; });
						}));
						Picker.bUseAlpha = true;
						Picker.OnColorPickerCancelled = FOnColorPickerCancelled::CreateLambda([this](FLinearColor Original)
						{
							ApplyImageLayerLive([Original](FOnyxParallaxLayer& Edited) { Edited.Tint = Original; });
						});
						Picker.OnColorPickerWindowClosed = FOnWindowClosed::CreateLambda([this](const TSharedRef<SWindow>&) { EndImageLayerChange(); });
						OpenColorPicker(Picker);
						return FReply::Handled();
					}))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				Row(INVTEXT("Below (m)"), INVTEXT("Carries the picture's bottom row on down this far, so a skyline still reaches below the ground when the camera rises."),
					Number(0.f, 200.f, 0.5f,
						[](const FOnyxParallaxLayer& Layer) { return Layer.ExtendBelow / 100.f; },
						[](FOnyxParallaxLayer& Layer, float Value) { Layer.ExtendBelow = FMath::Max(Value * 100.f, 0.f); }, INVTEXT("Image Layer Below")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				Row(INVTEXT("In the game"), INVTEXT("Off: the game doesn't draw it (it stays here, dimmed out of the map)."),
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { const FOnyxParallaxLayer* Layer = GetImageLayer(ActiveImageLayer); return Layer && Layer->bVisible ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
					{
						EditImageLayer(INVTEXT("Show Image Layer"), [State](FOnyxParallaxLayer& Layer) { Layer.bVisible = State == ECheckBoxState::Checked; });
					}))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 3.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ToolTipText(INVTEXT("Put it in the middle of the view"))
					.OnClicked_Lambda([this]()
					{
						AOnyxParallaxBackground* Background = FindBackdrop();
						const FOnyxParallaxLayer* Layer = GetImageLayer(ActiveImageLayer);
						if (Background && Layer && Canvas.IsValid())
						{
							const FVector Shown = Painter.MapPixelsToWorld(Canvas->GetViewCenterMapPixels());
							const FVector Delta = Canvas->GetParallaxCameraDelta();
							const FVector Anchor = Background->GetActorLocation();
							const FVector2D Offset(Shown.X - Delta.X * Layer->Factor.X - Anchor.X, Shown.Z - Delta.Z * Layer->Factor.Y - Anchor.Z);
							EditImageLayer(INVTEXT("Move Image Layer"), [Offset](FOnyxParallaxLayer& Edited) { Edited.OffsetX = Offset.X; Edited.OffsetZ = Offset.Y; });
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Text(INVTEXT("To the View"))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ToolTipText(INVTEXT("Take this picture out of the level (Delete)"))
					.OnClicked_Lambda([this]() { RemoveImageLayer(ActiveImageLayer); return FReply::Handled(); })
					[
						SNew(STextBlock).Text(INVTEXT("Remove"))
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Visibility_Lambda([this, HasPicture]() { const FOnyxParallaxLayer* Layer = GetImageLayer(ActiveImageLayer); return Layer && (Layer->bRepeat || !HasPicture()) ? EVisibility::Visible : EVisibility::Collapsed; })
				.ToolTipText(INVTEXT("Repeating layers and colour bands as wide as the level plus a screen on each side, so they cover the view wherever the camera goes"))
				.OnClicked_Lambda([this]() { FitBackdropToLevel(); return FReply::Handled(); })
				[
					SNew(STextBlock).Text(INVTEXT("Across the Whole Level"))
				]
			]
		];
}

TSharedRef<SWidget> SOnyxPainterTool::MakeCanvasArea()
{
	SAssignNew(Canvas, SOnyxMapCanvas)
		.Painter(&Painter)
		.ShowGrid_Lambda([this]() { return bShowGrid; })
		.ShowCollision_Lambda([this]() { return bShowCollision; })
		.ShowParallax_Lambda([this]() { return bShowParallax; })
		.SelectedImageLayer_Lambda([this]() { return ActiveImageLayer; })
		.HiddenImageLayers_Lambda([this]() { return HiddenImageLayers; })
		.HiddenObjectLayers_Lambda([this]() { return HiddenObjectLayers; })
		.HiddenLayers_Lambda([this]() { return HiddenLayers; })
		.PreviewStamp_Lambda([this]()
		{
			const bool bShow = !IsObjectLayer() && !IsImageLayer() && (Tool == EOnyxPaintTool::Brush || Tool == EOnyxPaintTool::Fill);
			return bShow ? Stamp : FOnyxStamp();
		})
		.PreviewBrush_Lambda([this]() { return IsObjectLayer() && Tool == EOnyxPaintTool::Brush ? SelectedBrush : nullptr; })
		.PreviewRect(this, &SOnyxPainterTool::GetPreviewRect)
		.SelectedActor_Lambda([this]() { return SelectedActor; })
		.OnStroke(this, &SOnyxPainterTool::HandleStroke)
		.OnKey(this, &SOnyxPainterTool::HandleKey)
		.OnQueryCursor(this, &SOnyxPainterTool::GetCursorFor)
		.OnResizeMap(this, &SOnyxPainterTool::ResizeMapEdges);

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			// Textures dropped on the map become image layers, their middle where they are dropped.
			SNew(SAssetDropTarget)
			.bSupportsMultiDrop(true)
			.OnAreAssetsAcceptableForDrop_Lambda([this](TArrayView<FAssetData> Assets) { return Painter.FindMapActor() && Assets.ContainsByPredicate(IsTextureAsset); })
			.OnAssetsDropped_Lambda([this](const FDragDropEvent& Event, TArrayView<FAssetData> Assets)
			{
				TArray<UTexture2D*> Textures;
				for (const FAssetData& Asset : Assets)
				{
					if (UTexture2D* Texture = Cast<UTexture2D>(Asset.GetAsset()))
					{
						Textures.Add(Texture);
					}
				}
				AddImageLayers(Textures, Canvas->LocalToMapPixels(Canvas->GetCachedGeometry().AbsoluteToLocal(Event.GetScreenSpacePosition())));
			})
			[
				Canvas.ToSharedRef()
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(10.f)
		[
			Chip(TAttribute<FText>::CreateSP(this, &SOnyxPainterTool::GetCanvasInfo))
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(10.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
			.Padding(FMargin(12.f, 6.f))
			.Visibility_Lambda([this]() { return Painter.GetTileMap() && !Painter.FindMapActor() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(SImage).Image(FAppStyle::GetBrush("Icons.WarningWithColor"))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
				[
					SNew(STextBlock).Text(INVTEXT("This map is not in the level yet, so objects can't be placed."))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(INVTEXT("Place in Level")).OnClicked_Lambda([this]() { Painter.PlaceMapInLevel(); return FReply::Handled(); })
				]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(460.f)
			.Visibility_Lambda([this]() { return Painter.GetTileMap() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 10.f)
				[
					SNew(SBox).WidthOverride(48.f).HeightOverride(48.f)
					[
						SNew(SImage).Image(FOnyxEditorStyle::Get().GetBrush("Onyx2D.Tab.Painter"))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 6.f)
				[
					SNew(STextBlock).Text(INVTEXT("Paint your level")).Font(Font(16, true))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
				[
					SNew(STextBlock)
					.Text(INVTEXT("Pick your tile set in the palette on the left, then press New Map (or choose an existing tile map in the toolbar). "
						"Paint tiles on the tile layers, and gameplay pieces or your own animated objects on the Entities and Decor layers."))
					.AutoWrapText(true)
					.Justification(ETextJustify::Center)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.IsEnabled_Lambda([this]() { return Stamp.TileSet.IsValid(); })
					.OnClicked_Lambda([this]() { CreateNewMap(); return FReply::Handled(); })
					.ToolTipText_Lambda([this]() { return Stamp.TileSet.IsValid() ? INVTEXT("Create a 60 x 24 map with two ground rows") : INVTEXT("Pick a tile set in the palette first (make one in the Tiles tab)"); })
					[
						SNew(STextBlock).Text(INVTEXT("  New Map  ")).Font(Font(10, true))
					]
				]
			]
		];
}

TSharedRef<SWidget> SOnyxPainterTool::MakeInspector()
{
	const TSharedRef<SWidget> MapPanel = SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 10.f, 10.f, 4.f)
			[
				SNew(STextBlock).Text(INVTEXT("Map size (tiles)")).Font(Font(9, true))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SSpinBox<int32>).MinValue(1).MaxValue(FOnyxMapPainter::MaxMapSize)
					.Value_Lambda([this]() { return NewSize.X; })
					.OnValueChanged_Lambda([this](int32 V) { NewSize.X = V; })
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SSpinBox<int32>).MinValue(1).MaxValue(FOnyxMapPainter::MaxMapSize)
					.Value_Lambda([this]() { return NewSize.Y; })
					.OnValueChanged_Lambda([this](int32 V) { NewSize.Y = V; })
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(INVTEXT("Apply"))
					.ToolTipText(INVTEXT("Columns come and go on the right, rows at the top, so the ground stays where it is"))
					.IsEnabled_Lambda([this]() { return Painter.GetTileMap() && NewSize != Painter.GetMapSize(); })
					.OnClicked_Lambda([this]()
					{
						ResizeMapEdges(FOnyxMapEdges::KeepBottomLeft(Painter.GetMapSize(), NewSize));
						Canvas->RequestFit();
						return FReply::Handled();
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 12.f, 10.f, 4.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(INVTEXT("Grow or shrink a side")).Font(Font(9, true))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(STextBlock).Text(INVTEXT("by")).Font(Font(9)).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(56.f)
					[
						SNew(SSpinBox<int32>).MinValue(1).MaxValue(256)
						.ToolTipText(INVTEXT("Cells each + or - button adds or removes"))
						.Value_Lambda([this]() { return EdgeStep; })
						.OnValueChanged_Lambda([this](int32 V) { EdgeStep = FMath::Max(V, 1); })
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 2.f)
			[
				// The four sides around the map's size, where they are on the map.
				SNew(SGridPanel)
				.FillColumn(0, 1.f)
				.FillColumn(2, 1.f)
				+ SGridPanel::Slot(1, 0).Padding(2.f)
				[
					MakeEdgeButtons(INVTEXT("Top"), INVTEXT("at the top"), &FOnyxMapEdges::Top, false)
				]
				+ SGridPanel::Slot(0, 1).Padding(2.f)
				[
					MakeEdgeButtons(INVTEXT("Left"), INVTEXT("on the left"), &FOnyxMapEdges::Left, true)
				]
				+ SGridPanel::Slot(1, 1).Padding(2.f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(FMargin(4.f, 6.f))
					[
						SNew(STextBlock)
						.Font(Font(9, true))
						.Text_Lambda([this]()
						{
							const FIntPoint Size = Painter.GetMapSize();
							return Painter.GetTileMap() ? FText::Format(INVTEXT("{0} x {1}"), Size.X, Size.Y) : INVTEXT("-");
						})
					]
				]
				+ SGridPanel::Slot(2, 1).Padding(2.f)
				[
					MakeEdgeButtons(INVTEXT("Right"), INVTEXT("on the right"), &FOnyxMapEdges::Right, true)
				]
				+ SGridPanel::Slot(1, 2).Padding(2.f)
				[
					MakeEdgeButtons(INVTEXT("Bottom"), INVTEXT("at the bottom"), &FOnyxMapEdges::Bottom, false)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 4.f, 10.f, 0.f)
			[
				Hint(INVTEXT("Or drag the map's border on the canvas. Whatever you change, what you painted stays where it is in the level: tiles, objects, water and zones."))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 8.f)
			[
				SNew(SButton)
				.Text(INVTEXT("Open in Tile Map Editor"))
				.IsEnabled_Lambda([this]() { return Painter.GetTileMap() != nullptr; })
				.OnClicked_Lambda([this]()
				{
					GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Painter.GetTileMap());
					return FReply::Handled();
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 12.f, 10.f, 4.f)
			[
				SNew(STextBlock).Text(INVTEXT("Controls")).Font(Font(9, true))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 0.f, 10.f, 10.f)
			[
				Hint(INVTEXT("Left drag: use tool · Right drag: erase\nMiddle drag or Space + drag: pan · Wheel: zoom\nDrag the map's border: grow or shrink it\nB brush · R rectangle · F fill · E erase · I pick · S select\nG grid · Home fit · Del delete object\n\nSelect an object to edit it here."))
			]
		];

	return Section(INVTEXT("Inspector"),
		SNew(SWidgetSwitcher)
		.WidgetIndex_Lambda([this]() { return SelectedActor.IsValid() ? 1 : 0; })
		+ SWidgetSwitcher::Slot()[MapPanel]
		+ SWidgetSwitcher::Slot()[Inspector.ToSharedRef()]);
}

// --- State ---------------------------------------------------------------------------

void SOnyxPainterTool::SetTileMap(UPaperTileMap* TileMap)
{
	Painter.SetTileMap(TileMap);
	ActiveObjectLayer = NAME_None;
	HiddenLayers.Reset();
	SelectActor(nullptr);
	bWarnedNoActor = false;
	if (TileMap)
	{
		NewSize = MapSizeSeen = Painter.GetMapSize();
		UPaperTileSet* MapTileSet = TileMap->SelectedTileSet.Get();
		for (int32 LayerIndex = 0; !MapTileSet && LayerIndex < TileMap->TileLayers.Num(); ++LayerIndex)
		{
			for (int32 Y = 0; !MapTileSet && Y < TileMap->MapHeight; ++Y)
			{
				for (int32 X = 0; !MapTileSet && X < TileMap->MapWidth; ++X)
				{
					MapTileSet = Painter.GetCell(FIntPoint(X, Y), LayerIndex).TileSet;
				}
			}
		}
		if (MapTileSet && MapTileSet != Stamp.TileSet.Get())
		{
			SetTileSet(MapTileSet);
		}
	}
	if (Canvas.IsValid())
	{
		Canvas->RequestFit();
	}
	RefreshLayers();
}

void SOnyxPainterTool::SetTileSet(UPaperTileSet* InTileSet)
{
	Stamp.TileSet = InTileSet;
	Stamp.Min = FIntPoint::ZeroValue;
	Stamp.Size = FIntPoint(1, 1);
	ActiveTerrain = FOnyxTerrains::GetCount(InTileSet) > 0 ? 0 : INDEX_NONE;
	RefreshTerrains();
}

TSharedRef<SWidget> SOnyxPainterTool::MakeTerrainPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Header"))
		.Padding(FMargin(8.f, 6.f))
		.Visibility_Lambda([this]() { return Stamp.TileSet.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(STextBlock).Font(Font(9, true)).Text(INVTEXT("Terrains (auto-tiling)"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(TerrainButtons, SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f))
				+ SWrapBox::Slot()
				[
					SNew(SButton)
					.Text(INVTEXT("New terrain"))
					.ToolTipText(INVTEXT("Select the 3 x 3 template in the sheet (corners, edges and fill of one ground; 5 x 5 if the sheet has gaps between tiles) and make it a terrain."))
					.IsEnabled_Lambda([this]() { return Stamp.IsValid() && FOnyxTerrains::GetTemplateStride(Stamp.Size, 3) > 0; })
					.OnClicked_Lambda([this]()
					{
						const FScopedTransaction Transaction(INVTEXT("New Terrain"));
						const FString Name = FString::Printf(TEXT("Terrain %d"), FOnyxTerrains::GetCount(Stamp.TileSet.Get()) + 1);
						const int32 Created = FOnyxTerrains::CreateFromBlock(Stamp.TileSet.Get(), Stamp.Min, Stamp.Size, Name);
						if (Created != INDEX_NONE)
						{
							ActiveTerrain = Created;
							SetTool(EOnyxPaintTool::Terrain);
							RefreshTerrains();
							Notify(FText::Format(INVTEXT("{0} created: paint with the Terrain tool (T)"), FText::FromString(Name)), true);
						}
						return FReply::Handled();
					})
				]
				+ SWrapBox::Slot()
				[
					SNew(SButton)
					.Text(INVTEXT("Inner corners"))
					.ToolTipText(INVTEXT("Select the 2 x 2 inner-corner tiles (arranged around a hole; 3 x 3 if the sheet has gaps) for the active terrain."))
					.IsEnabled_Lambda([this]() { return Stamp.IsValid() && FOnyxTerrains::GetTemplateStride(Stamp.Size, 2) > 0 && ActiveTerrain != INDEX_NONE; })
					.OnClicked_Lambda([this]()
					{
						const FScopedTransaction Transaction(INVTEXT("Set Terrain Inner Corners"));
						if (FOnyxTerrains::SetInnerCorners(Stamp.TileSet.Get(), ActiveTerrain, Stamp.Min, Stamp.Size))
						{
							Notify(INVTEXT("Inner corners added"), true);
						}
						return FReply::Handled();
					})
				]
			]
		];
}

void SOnyxPainterTool::RefreshTerrains()
{
	if (!TerrainButtons.IsValid())
	{
		return;
	}
	TerrainButtons->ClearChildren();
	const UPaperTileSet* TileSet = Stamp.TileSet.Get();
	const int32 Count = FOnyxTerrains::GetCount(TileSet);
	if (Count == 0)
	{
		TerrainButtons->AddSlot()
		[
			SNew(STextBlock).Font(Font(9)).ColorAndOpacity(FSlateColor::UseSubduedForeground()).AutoWrapText(true)
			.Text(INVTEXT("None yet: select a 3 x 3 ground template in the sheet, then New terrain."))
		];
		return;
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		TerrainButtons->AddSlot()
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.IsChecked_Lambda([this, Index]() { return ActiveTerrain == Index ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this, Index](ECheckBoxState) { ActiveTerrain = Index; SetTool(EOnyxPaintTool::Terrain); })
			[
				SNew(STextBlock).Margin(FMargin(6.f, 2.f)).Font(Font(9)).Text(FText::FromString(FOnyxTerrains::GetName(TileSet, Index)))
			]
		];
	}
}

void SOnyxPainterTool::RefreshLayers()
{
	LayerItems.Reset();
	if (Painter.GetTileMap())
	{
		// Front to back, like the tile layers: foreground decor, gameplay entities, then tiles, then background scenery.
		LayerItems.Add(MakeShared<int32>(DecorItem));
		LayerItems.Add(MakeShared<int32>(EntitiesItem));
		for (int32 Index = 0; Index < Painter.GetLayerCount(); ++Index)
		{
			LayerItems.Add(MakeShared<int32>(Index));
		}
		LayerItems.Add(MakeShared<int32>(BackgroundItem));
		// Behind it all: the image layers, nearest first.
		for (const int32 Index : BackdropOrder(FindBackdrop()))
		{
			LayerItems.Add(MakeShared<int32>(ImageItemOf(Index)));
		}
	}
	if (LayerList.IsValid())
	{
		LayerList->RequestListRefresh();
		const int32 Active = IsImageLayer() ? ImageItemOf(ActiveImageLayer)
			: ActiveObjectLayer == FOnyxBrushLibrary::EntitiesLayer ? EntitiesItem
			: ActiveObjectLayer == FOnyxBrushLibrary::DecorLayer ? DecorItem
			: ActiveObjectLayer == FOnyxBrushLibrary::BackgroundLayer ? BackgroundItem
			: Painter.GetLayer();
		if (const TSharedPtr<int32>* Item = LayerItems.FindByPredicate([Active](const TSharedPtr<int32>& I) { return *I == Active; }))
		{
			LayerList->SetSelection(*Item, ESelectInfo::Direct);
		}
	}
}

// --- Backdrop ------------------------------------------------------------------------

AOnyxParallaxBackground* SOnyxPainterTool::FindBackdrop() const
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AOnyxParallaxBackground* Background = BackdropActor.Get();
	if (Background && Background->GetWorld() == World)
	{
		return Background;
	}
	Background = nullptr;
	if (World)
	{
		TActorIterator<AOnyxParallaxBackground> It(World);
		Background = It ? *It : nullptr;
	}
	BackdropActor = Background;
	return Background;
}

AOnyxParallaxBackground* SOnyxPainterTool::CreateBackdrop()
{
	if (AOnyxParallaxBackground* Existing = FindBackdrop())
	{
		return Existing;
	}
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	const FScopedTransaction Transaction(INVTEXT("Add Backdrop"));
	// Behind the middle of the map (on its plane: the layers keep their own distance behind it).
	FVector Location = FVector::ZeroVector;
	if (Painter.FindMapActor())
	{
		const FIntPoint Size = Painter.GetMapSize();
		const FIntPoint Tile = Painter.GetTileSize();
		Location = Painter.MapPixelsToWorld(FVector2D(Size.X * Tile.X, Size.Y * Tile.Y) * 0.5);
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transactional;
	AOnyxParallaxBackground* Background = World->SpawnActor<AOnyxParallaxBackground>(Location, FRotator::ZeroRotator, Params);
	if (!Background)
	{
		return nullptr;
	}
	Background->SetActorLabel(TEXT("Backdrop"));
	Background->SetFolderPath(TEXT("Level/Backdrop"));
	Background->BackgroundLayers.Reset(); // only the layers you add
	BackdropActor = Background;
	if (Painter.FindMapActor())
	{
		FitBackdropToLevel();
	}
	Background->RerunConstructionScripts();
	return Background;
}

double SOnyxPainterTool::GetCmPerMapPixel() const
{
	return Painter.FindMapActor() ? (Painter.MapPixelsToWorld(FVector2D(1.0, 0.0)) - Painter.MapPixelsToWorld(FVector2D::ZeroVector)).Size() : 0.0;
}

FOnyxParallaxLayer* SOnyxPainterTool::GetImageLayer(int32 LayerIndex) const
{
	AOnyxParallaxBackground* Background = FindBackdrop();
	return Background && Background->BackgroundLayers.IsValidIndex(LayerIndex) ? &Background->BackgroundLayers[LayerIndex] : nullptr;
}

TArray<UTexture2D*> SOnyxPainterTool::GetSelectedTextures() const
{
	TArray<FAssetData> Selection;
	FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().GetSelectedAssets(Selection);
	TArray<UTexture2D*> Textures;
	for (const FAssetData& Asset : Selection)
	{
		if (UTexture2D* Texture = Cast<UTexture2D>(Asset.GetAsset()))
		{
			Textures.Add(Texture);
		}
	}
	return Textures;
}

int32 SOnyxPainterTool::AddImageLayer(UTexture2D* Texture, TOptional<FVector2D> AtMapPixels)
{
	AOnyxParallaxBackground* Background = FindBackdrop() ? FindBackdrop() : CreateBackdrop();
	if (!Background || !Canvas.IsValid())
	{
		return INDEX_NONE;
	}
	// In front of the nearest image layer so far, a little nearer and following the camera a little less.
	const FOnyxParallaxLayer* Nearest = nullptr;
	for (const FOnyxParallaxLayer& Layer : Background->BackgroundLayers)
	{
		Nearest = !Nearest || Layer.Depth < Nearest->Depth ? &Layer : Nearest;
	}
	FOnyxParallaxLayer Layer;
	Layer.Name = Texture ? FName(*Texture->GetName()) : FName(TEXT("Colour"));
	Layer.Texture = Texture;
	Layer.bRepeat = Texture == nullptr; // a picture goes where you put it; a colour band spans the level
	Layer.Depth = Nearest ? FMath::Max(Nearest->Depth * 0.75f, 30.f) : 2000.f;
	Layer.Factor = Nearest ? FVector2D(FMath::Max(Nearest->Factor.X - 0.15, 0.05), FMath::Max(Nearest->Factor.Y - 0.15, 0.05)) : FVector2D(0.8, 0.8);
	const double CmPerPixel = GetCmPerMapPixel();
	if (Texture)
	{
		Layer.Height = CmPerPixel > 0.0 ? FMath::Max(float(Texture->GetImportedSize().Y * CmPerPixel), 50.f) : 1200.f; // its pixels as big as the map's
	}
	else
	{
		Layer.Tint = FLinearColor(0.05f, 0.06f, 0.09f);
		Layer.Height = 3000.f;
	}
	// Its middle under the drop (or in the middle of the view), as the view shows it now.
	const FVector Shown = Painter.MapPixelsToWorld(AtMapPixels.IsSet() ? *AtMapPixels : Canvas->GetViewCenterMapPixels());
	const FVector Delta = Canvas->GetParallaxCameraDelta();
	const FVector Anchor = Background->GetActorLocation();
	Layer.OffsetX = float(Shown.X - Delta.X * Layer.Factor.X - Anchor.X);
	Layer.OffsetZ = float(Shown.Z - Delta.Z * Layer.Factor.Y - Anchor.Z);
	const int32 Added = Background->BackgroundLayers.Num();
	EditBackdrop(INVTEXT("Add Image Layer"), [&Layer](AOnyxParallaxBackground& Edited) { Edited.BackgroundLayers.Add(Layer); });
	return Added;
}

void SOnyxPainterTool::AddImageLayers(const TArray<UTexture2D*>& Textures, TOptional<FVector2D> AtMapPixels)
{
	if (!Painter.FindMapActor())
	{
		Notify(INVTEXT("Place the tile map in the level first (banner at the top of the map)."), false);
		return;
	}
	const FScopedTransaction Transaction(INVTEXT("Add Image Layers"));
	int32 Last = INDEX_NONE;
	if (Textures.IsEmpty())
	{
		Last = AddImageLayer(nullptr, AtMapPixels);
	}
	for (UTexture2D* Texture : Textures)
	{
		Last = AddImageLayer(Texture, AtMapPixels);
	}
	if (Last != INDEX_NONE)
	{
		SelectLayer(ImageItemOf(Last));
		RefreshLayers();
	}
}

void SOnyxPainterTool::RemoveImageLayer(int32 LayerIndex)
{
	if (!GetImageLayer(LayerIndex))
	{
		return;
	}
	EditBackdrop(INVTEXT("Remove Image Layer"), [LayerIndex](AOnyxParallaxBackground& Background) { Background.BackgroundLayers.RemoveAt(LayerIndex); });
	HiddenImageLayers.Reset();
	if (ActiveImageLayer == LayerIndex)
	{
		SelectLayer(Painter.GetLayer());
	}
	RefreshLayers();
}

float SOnyxPainterTool::GetImageLayerScale(const FOnyxParallaxLayer& Layer) const
{
	const double CmPerPixel = GetCmPerMapPixel();
	const int32 Pixels = Layer.Texture ? Layer.Texture->GetImportedSize().Y : 0;
	return Pixels > 0 && CmPerPixel > 0.0 ? float(Layer.Height / (Pixels * CmPerPixel)) : 1.f;
}

void SOnyxPainterTool::EditImageLayer(const FText& Title, TFunctionRef<void(FOnyxParallaxLayer&)> Edit)
{
	const int32 Index = ActiveImageLayer;
	EditBackdrop(Title, [Index, &Edit](AOnyxParallaxBackground& Background)
	{
		if (Background.BackgroundLayers.IsValidIndex(Index))
		{
			Edit(Background.BackgroundLayers[Index]);
		}
	});
}

void SOnyxPainterTool::BeginImageLayerChange(const FText& Title)
{
	if (AOnyxParallaxBackground* Background = FindBackdrop())
	{
		ImageChange = MakeUnique<FScopedTransaction>(Title);
		Background->Modify();
	}
}

void SOnyxPainterTool::ApplyImageLayerLive(TFunctionRef<void(FOnyxParallaxLayer&)> Edit)
{
	AOnyxParallaxBackground* Background = FindBackdrop();
	if (Background && Background->BackgroundLayers.IsValidIndex(ActiveImageLayer))
	{
		Edit(Background->BackgroundLayers[ActiveImageLayer]);
		Background->RerunConstructionScripts();
	}
}

void SOnyxPainterTool::EndImageLayerChange()
{
	ImageChange.Reset();
}

int32 SOnyxPainterTool::GetImageCornerAt(const FVector2D& MapPixels) const
{
	const TOptional<FBox2D> Rect = Canvas.IsValid() && IsImageLayer() ? Canvas->GetImageLayerRect(ActiveImageLayer) : TOptional<FBox2D>();
	if (!Rect.IsSet())
	{
		return 0;
	}
	const double Reach = 9.0 / FMath::Max(Canvas->GetZoom(), 0.01f); // screen pixels around each handle
	const FVector2D Corners[] = { Rect->Min, FVector2D(Rect->Max.X, Rect->Min.Y), Rect->Max, FVector2D(Rect->Min.X, Rect->Max.Y) };
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		if (FMath::Abs(MapPixels.X - Corners[Corner].X) <= Reach && FMath::Abs(MapPixels.Y - Corners[Corner].Y) <= Reach)
		{
			return Corner + 1;
		}
	}
	return 0;
}

void SOnyxPainterTool::FitBackdropToLevel()
{
	AOnyxParallaxBackground* Background = FindBackdrop();
	const double CmPerPixel = GetCmPerMapPixel();
	if (!Background || CmPerPixel <= 0.0)
	{
		Notify(INVTEXT("Needs a tile map in the level."), false);
		return;
	}
	const FIntPoint Size = Painter.GetMapSize();
	const FIntPoint Tile = Painter.GetTileSize();
	const FVector2D MapPixels(Size.X * Tile.X, Size.Y * Tile.Y);
	const FVector Middle = Painter.MapPixelsToWorld(MapPixels * 0.5);
	const double MapWidth = MapPixels.X * CmPerPixel;
	// A layer is never more than half the level away from the view's middle, wherever it scrolls; a screen on each side more.
	const float Width = float(MapWidth + 6000.0);
	EditBackdrop(INVTEXT("Fit Backdrop to Level"), [Middle, Width](AOnyxParallaxBackground& Edited)
	{
		Edited.SetActorLocation(FVector(Middle.X, Edited.GetActorLocation().Y, Edited.GetActorLocation().Z));
		Edited.TotalWidth = Width;
	});
	Notify(FText::Format(INVTEXT("The backdrop covers the level: {0} m wide."), FText::AsNumber(FMath::RoundToInt(Width / 100.f))), true);
}

void SOnyxPainterTool::EditBackdrop(const FText& Title, TFunctionRef<void(AOnyxParallaxBackground&)> Edit)
{
	AOnyxParallaxBackground* Background = FindBackdrop();
	if (!Background)
	{
		return;
	}
	const FScopedTransaction Transaction(Title);
	Background->Modify();
	Edit(*Background);
	Background->RerunConstructionScripts();
	if (SelectedActor.Get() == Background && Inspector.IsValid())
	{
		Inspector->ForceRefresh();
	}
}

// Console hooks for scripted screenshots (no mouse): named, unity builds merge files.
namespace OnyxPainterConsole
{
	FString PendingLayer;
	FString PendingImage;
	FIntPoint PendingImageCell(INDEX_NONE, INDEX_NONE);

	FAutoConsoleCommand SelectLayerCommand(TEXT("Onyx2D.PainterSelectLayer"),
		TEXT("Selects a layer of the Level Painter by its name: Onyx2D.PainterSelectLayer <name>."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { PendingLayer = FString::Join(Args, TEXT(" ")); }));

	FAutoConsoleCommand AddImageCommand(TEXT("Onyx2D.PainterAddImage"),
		TEXT("Adds a texture as an image layer in the Level Painter: Onyx2D.PainterAddImage <texture path> [column row]."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			PendingImage = Args.IsEmpty() ? FString() : Args[0];
			PendingImageCell = Args.Num() >= 3 ? FIntPoint(FCString::Atoi(*Args[1]), FCString::Atoi(*Args[2])) : FIntPoint(INDEX_NONE, INDEX_NONE);
		}));
}

EActiveTimerReturnType SOnyxPainterTool::PollBackdrop(double, float)
{
	if (!OnyxPainterConsole::PendingImage.IsEmpty() && Painter.GetTileMap())
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *OnyxPainterConsole::PendingImage);
		const FIntPoint Cell = OnyxPainterConsole::PendingImageCell;
		const FIntPoint Tile = Painter.GetTileSize();
		OnyxPainterConsole::PendingImage.Reset();
		if (Texture)
		{
			AddImageLayers({ Texture }, Painter.IsValidCell(Cell) ? TOptional<FVector2D>(FVector2D((Cell.X + 0.5) * Tile.X, (Cell.Y + 0.5) * Tile.Y)) : TOptional<FVector2D>());
		}
	}
	if (!OnyxPainterConsole::PendingLayer.IsEmpty() && Painter.GetTileMap())
	{
		const FString Wanted = OnyxPainterConsole::PendingLayer;
		OnyxPainterConsole::PendingLayer.Reset();
		RefreshLayers();
		if (const TSharedPtr<int32>* Item = LayerItems.FindByPredicate([this, &Wanted](const TSharedPtr<int32>& I) { return GetLayerTitle(*I).ToString().Equals(Wanted, ESearchCase::IgnoreCase); }))
		{
			SelectLayer(**Item);
			RefreshLayers();
		}
	}
	// The image layers in the list as the level has them (added, removed or moved in the Details panel, an undo, another level).
	if (!Painter.GetTileMap() || bDraggingImage || ImageChange)
	{
		return EActiveTimerReturnType::Continue;
	}
	if (IsImageLayer() && !GetImageLayer(ActiveImageLayer))
	{
		SelectLayer(Painter.GetLayer());
		RefreshLayers();
		return EActiveTimerReturnType::Continue;
	}
	TArray<int32> Listed;
	for (const TSharedPtr<int32>& Item : LayerItems)
	{
		if (IsImageItem(*Item))
		{
			Listed.Add(ImageIndexOf(*Item));
		}
	}
	if (Listed != BackdropOrder(FindBackdrop()))
	{
		RefreshLayers();
	}
	return EActiveTimerReturnType::Continue;
}

void SOnyxPainterTool::RefreshBrushes()
{
	const FName Previous = SelectedBrush.IsValid() ? SelectedBrush->Id : FName(TEXT("Coin"));
	BrushItems.Reset();
	// Your own objects first, then the built-in gameplay pieces.
	for (const bool bCustom : { true, false })
	{
		for (const TSharedRef<FOnyxPainterBrush>& Brush : FOnyxBrushLibrary::Get().GetBrushes())
		{
			if (Brush->bCustom == bCustom && !Brush->bHiddenInPalette)
			{
				BrushItems.Add(Brush);
			}
		}
	}
	const TSharedPtr<FOnyxPainterBrush>* Keep = BrushItems.FindByPredicate([Previous](const TSharedPtr<FOnyxPainterBrush>& B) { return B->Id == Previous; });
	SelectedBrush = Keep ? *Keep : (BrushItems.Num() > 0 ? BrushItems[0] : nullptr);
	if (BrushList.IsValid())
	{
		BrushList->RequestListRefresh();
		BrushList->SetSelection(SelectedBrush, ESelectInfo::Direct);
	}
}

void SOnyxPainterTool::SelectLayer(int32 LayerItem)
{
	ActiveImageLayer = IsImageItem(LayerItem) ? ImageIndexOf(LayerItem) : INDEX_NONE;
	ActiveObjectLayer = ObjectLayerOf(LayerItem);
	if (IsImageLayer())
	{
		RectStart.Reset();
		return;
	}
	if (!IsObjectLayer())
	{
		Painter.SetLayer(LayerItem);
	}
	else if (BrushList.IsValid() && SelectedBrush.IsValid())
	{
		BrushList->SetSelection(SelectedBrush, ESelectInfo::Direct);
	}
	RectStart.Reset();
}

void SOnyxPainterTool::SelectActor(AActor* Actor)
{
	SelectedActor = Actor;
	Inspector->SetObject(Actor, true);
	if (Actor && GEditor)
	{
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(Actor, true, true);
	}
}

void SOnyxPainterTool::ToggleLayerVisibility(int32 LayerItem)
{
	if (IsImageItem(LayerItem))
	{
		const int32 Index = ImageIndexOf(LayerItem);
		if (HiddenImageLayers.Contains(Index))
		{
			HiddenImageLayers.Remove(Index);
		}
		else
		{
			HiddenImageLayers.Add(Index);
		}
	}
	else if (LayerItem < 0)
	{
		const FName Layer = ObjectLayerOf(LayerItem);
		if (HiddenObjectLayers.Contains(Layer))
		{
			HiddenObjectLayers.Remove(Layer);
		}
		else
		{
			HiddenObjectLayers.Add(Layer);
		}
	}
	else if (HiddenLayers.Contains(LayerItem))
	{
		HiddenLayers.Remove(LayerItem);
	}
	else
	{
		HiddenLayers.Add(LayerItem);
	}
}

bool SOnyxPainterTool::IsLayerVisible(int32 LayerItem) const
{
	if (IsImageItem(LayerItem))
	{
		return !HiddenImageLayers.Contains(ImageIndexOf(LayerItem));
	}
	return LayerItem < 0 ? !HiddenObjectLayers.Contains(ObjectLayerOf(LayerItem)) : !HiddenLayers.Contains(LayerItem);
}

AActor* SOnyxPainterTool::FindObjectForSelection(const FIntPoint& Cell) const
{
	if (IsObjectLayer())
	{
		return Painter.FindObject(Cell, ActiveObjectLayer);
	}
	for (const FName Layer : { FOnyxBrushLibrary::DecorLayer, FOnyxBrushLibrary::EntitiesLayer, FOnyxBrushLibrary::BackgroundLayer })
	{
		if (AActor* Found = Painter.FindObject(Cell, Layer))
		{
			return Found;
		}
	}
	return nullptr;
}

// --- Editing -------------------------------------------------------------------------

void SOnyxPainterTool::ApplyAt(const FIntPoint& Cell, bool bErase)
{
	if (!Painter.IsValidCell(Cell))
	{
		return;
	}
	if (!IsObjectLayer() && Tool == EOnyxPaintTool::Terrain)
	{
		Painter.PaintTerrain(Cell, Stamp.TileSet.Get(), ActiveTerrain, bErase);
		return;
	}
	if (!IsObjectLayer())
	{
		if (bErase)
		{
			Painter.PaintTile(Cell, FPaperTileInfo());
		}
		else
		{
			// Snap the stamp to its own size from where the stroke started, so blocks tile cleanly.
			const FIntPoint Offset = Cell - StrokeOrigin;
			const FIntPoint PaintSize = Stamp.GetPaintSize();
			const FIntPoint Snapped = StrokeOrigin + FIntPoint(
				FMath::FloorToInt(float(Offset.X) / PaintSize.X) * PaintSize.X,
				FMath::FloorToInt(float(Offset.Y) / PaintSize.Y) * PaintSize.Y);
			Painter.PaintStamp(Snapped, Stamp);
		}
		return;
	}
	if (!Painter.FindMapActor())
	{
		if (!bWarnedNoActor)
		{
			Notify(INVTEXT("Place the tile map in the level first (banner at the top of the canvas)."), false);
			bWarnedNoActor = true;
		}
		return;
	}
	if (bErase)
	{
		if (Painter.FindObject(Cell, ActiveObjectLayer) == SelectedActor.Get())
		{
			SelectActor(nullptr);
		}
		Painter.EraseObject(Cell, ActiveObjectLayer);
	}
	else if (SelectedBrush.IsValid() && !SelectedBrush->IsArea())
	{
		Painter.PlaceObject(Cell, *SelectedBrush, ActiveObjectLayer);
	}
}

void SOnyxPainterTool::PlaceArea(const FIntRect& Cells)
{
	if (!SelectedBrush.IsValid() || !SelectedBrush->IsArea())
	{
		return;
	}
	if (!Painter.FindMapActor())
	{
		Notify(INVTEXT("Place the tile map in the level first (banner at the top of the canvas)."), false);
		return;
	}
	const FIntPoint MapMax = Painter.GetMapSize() - FIntPoint(1, 1);
	const FIntRect Clamped(Cells.Min.ComponentMax(FIntPoint::ZeroValue), Cells.Max.ComponentMin(MapMax));
	const FScopedTransaction Transaction(FText::Format(INVTEXT("Add {0}"), SelectedBrush->Label));
	if (AActor* Actor = Painter.PlaceArea(Clamped, *SelectedBrush, ActiveObjectLayer))
	{
		// Straight to Select, so the handles work: resize by the edges, move by the inside.
		SetTool(EOnyxPaintTool::Select);
		SelectActor(Actor);
		Notify(FText::Format(INVTEXT("{0} added: drag its edges to resize it, or the inside to move it"), SelectedBrush->Label), true);
	}
}

int32 SOnyxPainterTool::GetAreaEdgesAt(const FVector2D& MapPixels) const
{
	const TSharedPtr<FOnyxPainterBrush> Brush = FOnyxBrushLibrary::Get().FindFor(SelectedActor.Get());
	if (!Brush.IsValid() || !Brush->IsArea() || !Canvas.IsValid())
	{
		return 0;
	}
	const FBox Area = Brush->GetArea(SelectedActor.Get());
	const FVector2D Min = Painter.WorldToMapPixels(FVector(Area.Min.X, 0.f, Area.Max.Z));
	const FVector2D Max = Painter.WorldToMapPixels(FVector(Area.Max.X, 0.f, Area.Min.Z));
	const double Reach = AreaEdgeGrab / FMath::Max(Canvas->GetZoom(), 0.01f);
	if (MapPixels.X < Min.X - Reach || MapPixels.X > Max.X + Reach || MapPixels.Y < Min.Y - Reach || MapPixels.Y > Max.Y + Reach)
	{
		return 0;
	}
	int32 Edges = 0;
	Edges |= FMath::Abs(MapPixels.X - Min.X) <= Reach ? AreaEdgeLeft : (FMath::Abs(MapPixels.X - Max.X) <= Reach ? AreaEdgeRight : 0);
	Edges |= FMath::Abs(MapPixels.Y - Min.Y) <= Reach ? AreaEdgeTop : (FMath::Abs(MapPixels.Y - Max.Y) <= Reach ? AreaEdgeBottom : 0);
	return Edges;
}

FIntRect SOnyxPainterTool::GetDraggedArea(const FIntPoint& Cell) const
{
	const FIntPoint Delta = Cell - GrabStartCell;
	FIntRect Area = GrabStartArea;
	if (bMovingArea)
	{
		Area.Min += Delta;
		Area.Max += Delta;
		return Area;
	}
	// Each grabbed edge follows the mouse; the area never gets smaller than one cell.
	if (GrabbedEdges & AreaEdgeLeft)
	{
		Area.Min.X = FMath::Min(GrabStartArea.Min.X + Delta.X, Area.Max.X);
	}
	if (GrabbedEdges & AreaEdgeRight)
	{
		Area.Max.X = FMath::Max(GrabStartArea.Max.X + Delta.X, Area.Min.X);
	}
	if (GrabbedEdges & AreaEdgeTop)
	{
		Area.Min.Y = FMath::Min(GrabStartArea.Min.Y + Delta.Y, Area.Max.Y);
	}
	if (GrabbedEdges & AreaEdgeBottom)
	{
		Area.Max.Y = FMath::Max(GrabStartArea.Max.Y + Delta.Y, Area.Min.Y);
	}
	return Area;
}

TOptional<EMouseCursor::Type> SOnyxPainterTool::GetCursorFor(FVector2D MapPixels) const
{
	if (IsImageLayer())
	{
		// The image layer being edited: its corners resize it, its picture moves it; elsewhere nothing happens.
		const int32 Corner = bDraggingImage ? ImageGrabCorner : GetImageCornerAt(MapPixels);
		if (Corner != 0)
		{
			return Corner == 1 || Corner == 3 ? EMouseCursor::ResizeSouthEast : EMouseCursor::ResizeSouthWest;
		}
		const TOptional<FBox2D> Picture = Canvas.IsValid() ? Canvas->GetImageLayerRect(ActiveImageLayer) : TOptional<FBox2D>();
		if (bDraggingImage || (Picture.IsSet() && Picture->IsInsideOrOn(MapPixels)))
		{
			return EMouseCursor::CardinalCross;
		}
		return {};
	}
	if (Tool != EOnyxPaintTool::Select)
	{
		return {};
	}
	const int32 Edges = bMovingArea || GrabbedEdges != 0 ? GrabbedEdges : GetAreaEdgesAt(MapPixels);
	if (bMovingArea)
	{
		return EMouseCursor::CardinalCross;
	}
	const bool bHorizontal = (Edges & (AreaEdgeLeft | AreaEdgeRight)) != 0;
	const bool bVertical = (Edges & (AreaEdgeTop | AreaEdgeBottom)) != 0;
	if (bHorizontal && bVertical)
	{
		const bool bFalling = ((Edges & AreaEdgeLeft) != 0) == ((Edges & AreaEdgeTop) != 0); // top-left or bottom-right corner
		return bFalling ? EMouseCursor::ResizeSouthEast : EMouseCursor::ResizeSouthWest;
	}
	if (bHorizontal)
	{
		return EMouseCursor::ResizeLeftRight;
	}
	if (bVertical)
	{
		return EMouseCursor::ResizeUpDown;
	}
	return {};
}

TSharedRef<SWidget> SOnyxPainterTool::MakeEdgeButtons(const FText& Side, const FText& Where, int32 FOnyxMapEdges::* Edge, bool bColumns)
{
	auto EdgesFor = [this, Edge](int32 Sign)
	{
		FOnyxMapEdges Edges;
		Edges.*Edge = Sign * EdgeStep;
		return Edges;
	};
	auto Button = [this, Where, bColumns, EdgesFor](int32 Sign) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(3.f))
			.ToolTipText_Lambda([this, Where, bColumns, Sign]()
			{
				const FText Unit = bColumns ? (EdgeStep == 1 ? INVTEXT("column") : INVTEXT("columns")) : (EdgeStep == 1 ? INVTEXT("row") : INVTEXT("rows"));
				return FText::Format(INVTEXT("{0} {1} {2} {3}"), Sign > 0 ? INVTEXT("Add") : INVTEXT("Remove"), EdgeStep, Unit, Where);
			})
			.IsEnabled_Lambda([this, EdgesFor, Sign]()
			{
				const FIntPoint Size = EdgesFor(Sign).Apply(Painter.GetMapSize());
				return Painter.GetTileMap() && Size.X >= 1 && Size.Y >= 1 && Size.X <= FOnyxMapPainter::MaxMapSize && Size.Y <= FOnyxMapPainter::MaxMapSize;
			})
			.OnClicked_Lambda([this, EdgesFor, Sign]()
			{
				ResizeMapEdges(EdgesFor(Sign));
				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(Sign > 0 ? "Icons.Plus" : "Icons.Minus"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			];
	};
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			Button(-1)
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(Side).Font(Font(9))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			Button(1)
		];
}

void SOnyxPainterTool::ResizeMapEdges(const FOnyxMapEdges& Edges)
{
	auto CountObjects = [this]()
	{
		int32 Count = 0;
		Painter.ForEachObject([&Count](const FOnyxPlacedObject&) { ++Count; });
		return Count;
	};
	const int32 ObjectsBefore = CountObjects();
	if (!Painter.ResizeEdges(Edges))
	{
		return;
	}
	NewSize = MapSizeSeen = Painter.GetMapSize();
	RectStart.Reset();
	// Objects stay where they are in the level, so cutting a side can leave some off the map.
	const int32 LeftOut = ObjectsBefore - CountObjects();
	if (LeftOut > 0)
	{
		Notify(FText::Format(INVTEXT("{0} {0}|plural(one=object is,other=objects are) off the map now (still in the level). Undo, or grow that side back to see {0}|plural(one=it,other=them) again."), LeftOut), false);
	}
}

void SOnyxPainterTool::ApplyRect(const FIntRect& Rect, bool bErase)
{
	if (IsObjectLayer() && !bErase && SelectedBrush.IsValid() && SelectedBrush->IsArea())
	{
		PlaceArea(Rect); // one object over the whole rectangle, not one per cell
		return;
	}
	Painter.BeginStroke(bErase ? INVTEXT("Clear Rectangle") : INVTEXT("Fill Rectangle"));
	for (int32 Y = Rect.Min.Y; Y <= Rect.Max.Y; ++Y)
	{
		for (int32 X = Rect.Min.X; X <= Rect.Max.X; ++X)
		{
			const FIntPoint Cell(X, Y);
			if (!IsObjectLayer() && !bErase)
			{
				Painter.PaintTile(Cell, Stamp.GetTile(X - Rect.Min.X, Y - Rect.Min.Y, Cell));
			}
			else
			{
				ApplyAt(Cell, bErase);
			}
		}
	}
	Painter.EndStroke();
}

void SOnyxPainterTool::HandleStroke(EOnyxStrokePhase Phase, FIntPoint Cell, bool bSecondary)
{
	if (!Painter.GetTileMap())
	{
		return;
	}

	// An image layer: drag it to move it, drag a corner to resize it (the opposite corner stays, the picture keeps its shape).
	if (IsImageLayer())
	{
		const FOnyxParallaxLayer* Layer = GetImageLayer(ActiveImageLayer);
		if (bSecondary || !Layer)
		{
			return;
		}
		const FVector2D At = Canvas->GetHoverMapPixels();
		if (Phase == EOnyxStrokePhase::Begin)
		{
			// Only grabbing its picture (or a corner) moves it: a click anywhere else leaves it where it is.
			const TOptional<FBox2D> Rect = Canvas->GetImageLayerRect(ActiveImageLayer);
			ImageGrabCorner = GetImageCornerAt(At);
			if (ImageGrabCorner == 0 && (!Rect.IsSet() || !Rect->IsInsideOrOn(At)))
			{
				return;
			}
			ImageGrabStart = At;
			ImageStartRect = Rect.Get(FBox2D(At, At));
			ImageStartOffset = FVector2D(Layer->OffsetX, Layer->OffsetZ);
			ImageStartHeight = Layer->Height;
			bDraggingImage = true;
			BeginImageLayerChange(ImageGrabCorner != 0 ? INVTEXT("Resize Image Layer") : INVTEXT("Move Image Layer"));
		}
		else if (Phase == EOnyxStrokePhase::Move && bDraggingImage)
		{
			if (ImageGrabCorner == 0)
			{
				const FVector Moved = Painter.MapPixelsToWorld(At) - Painter.MapPixelsToWorld(ImageGrabStart);
				const FVector2D Start = ImageStartOffset;
				ApplyImageLayerLive([&Moved, &Start](FOnyxParallaxLayer& Edited)
				{
					Edited.OffsetX = float(Start.X + Moved.X);
					Edited.OffsetZ = float(Start.Y + Moved.Z);
				});
			}
			else
			{
				const FVector2D Corners[] = { ImageStartRect.Min, FVector2D(ImageStartRect.Max.X, ImageStartRect.Min.Y), ImageStartRect.Max, FVector2D(ImageStartRect.Min.X, ImageStartRect.Max.Y) };
				const FVector2D Fixed = Corners[(ImageGrabCorner + 1) % 4]; // the opposite corner
				const FVector2D StartSize = ImageStartRect.GetSize();
				const bool bWholePicture = Layer->Texture && Layer->Material.IsNull() && !Layer->bRepeat;
				const double Aspect = StartSize.Y > 0.0 ? StartSize.X / StartSize.Y : 1.0;
				// A picture placed once keeps its shape; a repeating one or a colour band only gets taller or shorter.
				const double NewHeight = FMath::Max(bWholePicture ? FMath::Max(FMath::Abs(At.Y - Fixed.Y), FMath::Abs(At.X - Fixed.X) / Aspect) : FMath::Abs(At.Y - Fixed.Y), 2.0);
				const double SignX = At.X >= Fixed.X ? 1.0 : -1.0;
				const double SignY = At.Y >= Fixed.Y ? 1.0 : -1.0;
				const FVector2D NewMiddle(bWholePicture ? Fixed.X + SignX * NewHeight * Aspect * 0.5 : ImageStartRect.GetCenter().X, Fixed.Y + SignY * NewHeight * 0.5);
				const FVector Moved = Painter.MapPixelsToWorld(NewMiddle) - Painter.MapPixelsToWorld(ImageStartRect.GetCenter());
				const float Height = float(ImageStartHeight * NewHeight / FMath::Max(StartSize.Y, 0.001));
				const FVector2D Start = ImageStartOffset;
				ApplyImageLayerLive([&Moved, &Start, Height](FOnyxParallaxLayer& Edited)
				{
					Edited.Height = FMath::Max(Height, 1.f);
					Edited.OffsetX = float(Start.X + Moved.X);
					Edited.OffsetZ = float(Start.Y + Moved.Z);
				});
			}
		}
		else if (Phase == EOnyxStrokePhase::End && bDraggingImage)
		{
			bDraggingImage = false;
			ImageGrabCorner = 0;
			EndImageLayerChange();
		}
		return;
	}

	// Select: click an object, drag it to another cell. Area objects (water) also resize by their edges.
	if (Tool == EOnyxPaintTool::Select && !bSecondary)
	{
		if (Phase == EOnyxStrokePhase::Begin)
		{
			GrabStartCell = Cell;
			GrabbedEdges = GetAreaEdgesAt(Canvas->GetHoverMapPixels());
			if (GrabbedEdges != 0 && Painter.GetAreaCells(SelectedActor.Get(), GrabStartArea))
			{
				Painter.BeginStroke(INVTEXT("Resize Area"));
				return;
			}
			GrabbedEdges = 0;
			AActor* Actor = FindObjectForSelection(Cell);
			SelectActor(Actor);
			if (Actor && Painter.GetAreaCells(Actor, GrabStartArea))
			{
				bMovingArea = true;
				Painter.BeginStroke(INVTEXT("Move Area"));
				return;
			}
			bDraggingActor = Actor != nullptr;
			if (bDraggingActor)
			{
				Painter.BeginStroke(INVTEXT("Move Object"));
			}
		}
		else if (Phase == EOnyxStrokePhase::Move && (bMovingArea || GrabbedEdges != 0))
		{
			Painter.SetAreaCells(SelectedActor.Get(), GetDraggedArea(Cell));
		}
		else if (Phase == EOnyxStrokePhase::Move && bDraggingActor)
		{
			Painter.MoveObject(SelectedActor.Get(), Cell);
		}
		else if (Phase == EOnyxStrokePhase::End && (bDraggingActor || bMovingArea || GrabbedEdges != 0))
		{
			bDraggingActor = false;
			bMovingArea = false;
			GrabbedEdges = 0;
			Painter.EndStroke();
		}
		return;
	}

	// Area brushes (water): drag out the block it fills; a click gives the default size.
	if (IsObjectLayer() && Tool == EOnyxPaintTool::Brush && SelectedBrush.IsValid() && SelectedBrush->IsArea() && !bSecondary)
	{
		if (Phase == EOnyxStrokePhase::Begin)
		{
			RectStart = Cell;
		}
		RectEnd = Cell;
		if (Phase == EOnyxStrokePhase::End && RectStart.IsSet())
		{
			FIntRect Cells(RectStart->ComponentMin(RectEnd), RectStart->ComponentMax(RectEnd));
			if (Cells.Min == Cells.Max)
			{
				Cells.Max = Cells.Min + SelectedBrush->DefaultAreaCells - FIntPoint(1, 1);
			}
			RectStart.Reset();
			PlaceArea(Cells);
		}
		return;
	}

	// Pick: the tile or object under the cursor becomes the brush.
	if (Tool == EOnyxPaintTool::Pick && !bSecondary)
	{
		if (Phase != EOnyxStrokePhase::Begin)
		{
			return;
		}
		if (IsObjectLayer())
		{
			if (const TSharedPtr<FOnyxPainterBrush> Picked = FOnyxBrushLibrary::Get().FindFor(Painter.FindObject(Cell, ActiveObjectLayer)))
			{
				SelectedBrush = Picked;
				BrushList->SetSelection(Picked, ESelectInfo::Direct);
				SetTool(EOnyxPaintTool::Brush);
			}
		}
		else
		{
			const FPaperTileInfo Tile = Painter.GetCell(Cell, Painter.GetLayer());
			if (Tile.IsValid())
			{
				if (Tile.TileSet != Stamp.TileSet.Get())
				{
					SetTileSet(Tile.TileSet);
				}
				const int32 Columns = FMath::Max(Tile.TileSet->GetTileCountX(), 1);
				Stamp.Min = FIntPoint(Tile.GetTileIndex() % Columns, Tile.GetTileIndex() / Columns);
				Stamp.Size = FIntPoint(1, 1);
				SetTool(EOnyxPaintTool::Brush);
			}
		}
		return;
	}

	const bool bErase = bSecondary || Tool == EOnyxPaintTool::Erase;

	if (Tool == EOnyxPaintTool::Rect)
	{
		if (Phase == EOnyxStrokePhase::Begin)
		{
			RectStart = Cell;
			bRectErase = bErase;
		}
		RectEnd = Cell;
		if (Phase == EOnyxStrokePhase::End && RectStart.IsSet())
		{
			const FIntPoint MapMax = Painter.GetMapSize() - FIntPoint(1, 1);
			const FIntRect Rect(RectStart->ComponentMin(RectEnd).ComponentMax(FIntPoint::ZeroValue), RectStart->ComponentMax(RectEnd).ComponentMin(MapMax));
			RectStart.Reset();
			ApplyRect(Rect, bRectErase);
		}
		return;
	}

	if (Tool == EOnyxPaintTool::Fill && !IsObjectLayer())
	{
		if (Phase == EOnyxStrokePhase::Begin)
		{
			Painter.BeginStroke(INVTEXT("Fill Tiles"));
			Painter.Fill(Cell, Stamp, bErase);
			Painter.EndStroke();
		}
		return;
	}

	// Brush / Erase (Fill on an object layer behaves like the brush).
	if (Phase == EOnyxStrokePhase::Begin)
	{
		StrokeOrigin = Cell;
		LastStrokeCell = Cell;
		Stamp.RandomSeed = FMath::Rand();
		Painter.BeginStroke(bErase ? INVTEXT("Erase") : INVTEXT("Paint"));
		ApplyAt(Cell, bErase);
		Painter.FlushChanges();
	}
	else if (Phase == EOnyxStrokePhase::Move)
	{
		// Walk every cell between the last and current mouse position, so fast strokes leave no gaps.
		const FIntPoint Delta = Cell - LastStrokeCell;
		const int32 Steps = FMath::Max(FMath::Abs(Delta.X), FMath::Abs(Delta.Y));
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			const float T = float(Step) / Steps;
			ApplyAt(FIntPoint(LastStrokeCell.X + FMath::RoundToInt(Delta.X * T), LastStrokeCell.Y + FMath::RoundToInt(Delta.Y * T)), bErase);
		}
		LastStrokeCell = Cell;
		Painter.FlushChanges(); // live update in the level viewport
	}
	else
	{
		Painter.EndStroke();
	}
}

FReply SOnyxPainterTool::HandleKey(const FKeyEvent& Event)
{
	if (Event.IsControlDown() || Event.IsAltDown() || Event.IsCommandDown())
	{
		return FReply::Unhandled(); // let the editor handle undo/redo etc.
	}
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Delete && IsImageLayer())
	{
		RemoveImageLayer(ActiveImageLayer);
		return FReply::Handled();
	}
	const TPair<FKey, EOnyxPaintTool> ToolKeys[] = {
		{ EKeys::B, EOnyxPaintTool::Brush }, { EKeys::R, EOnyxPaintTool::Rect }, { EKeys::F, EOnyxPaintTool::Fill },
		{ EKeys::E, EOnyxPaintTool::Erase }, { EKeys::I, EOnyxPaintTool::Pick }, { EKeys::S, EOnyxPaintTool::Select },
		{ EKeys::T, EOnyxPaintTool::Terrain },
	};
	for (const TPair<FKey, EOnyxPaintTool>& Entry : ToolKeys)
	{
		if (Key == Entry.Key)
		{
			SetTool(Entry.Value);
			return FReply::Handled();
		}
	}
	if (Key == EKeys::X || Key == EKeys::Y || Key == EKeys::V)
	{
		bool& Flag = Key == EKeys::X ? Stamp.bFlipX : Key == EKeys::Y ? Stamp.bFlipY : Stamp.bRandom;
		Flag = !Flag;
		return FReply::Handled();
	}
	if (Key == EKeys::G)
	{
		bShowGrid = !bShowGrid;
		return FReply::Handled();
	}
	if (Key == EKeys::C)
	{
		bShowCollision = !bShowCollision;
		return FReply::Handled();
	}
	if (Key == EKeys::Home)
	{
		Canvas->RequestFit();
		return FReply::Handled();
	}
	if (Key == EKeys::Escape)
	{
		SelectActor(nullptr);
		RectStart.Reset();
		return FReply::Handled();
	}
	if (Key == EKeys::Delete && SelectedActor.IsValid())
	{
		const FScopedTransaction Transaction(INVTEXT("Delete Object"));
		AActor* Actor = SelectedActor.Get();
		SelectActor(nullptr);
		Actor->Modify();
		Actor->GetWorld()->EditorDestroyActor(Actor, true);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SOnyxPainterTool::AddSelectedAssetsAsBrushes()
{
	TArray<FAssetData> Selection;
	FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().GetSelectedAssets(Selection);
	const int32 Added = FOnyxBrushLibrary::Get().AddAssets(Selection);
	Notify(Added > 0 ? FText::Format(INVTEXT("Added {0} object brush(es)"), Added)
		: INVTEXT("Select Blueprints (actors), flipbooks, sprites or static meshes in the Content Browser first."), Added > 0);
}

void SOnyxPainterTool::CreateNewMap()
{
	if (!Stamp.TileSet.IsValid())
	{
		Notify(INVTEXT("Pick a tile set in the palette first (create one in the Tiles tab)."), false);
		return;
	}
	FString PackageName;
	FString AssetName;
	FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().CreateUniqueAssetName(TEXT("/Game/Tiles/TM_Level"), FString(), PackageName, AssetName);

	TSharedRef<FOnyxTileMapSpec> Spec = MakeShared<FOnyxTileMapSpec>();
	Spec->TileSet = Stamp.TileSet;
	Spec->Name = AssetName;
	Spec->Folder = TEXT("/Game/Tiles");
	Spec->Width = 60;
	Spec->Height = 24;
	Spec->bFillGround = false;
	const FPaperTileInfo Ground = Stamp.GetSourceTile(0, 0);
	Spec->GroundTile = Ground.IsValid() ? Ground.GetTileIndex() : 0;
	Spec->bPlaceInLevel = true;

	auto Row = [](const FText& Label, const TSharedRef<SWidget>& Content)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(110.f)[SNew(STextBlock).Text(Label)]]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Content];
	};
	TSharedRef<SWindow> Window = SNew(SWindow).Title(INVTEXT("New Tile Map")).ClientSize(FVector2D(420.f, 230.f)).SupportsMinimize(false).SupportsMaximize(false);
	TWeakPtr<SWindow> WeakWindow = Window;
	Window->SetContent(SNew(SBorder).BorderImage(FAppStyle::GetBrush("Brushes.Panel")).Padding(14.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				Row(INVTEXT("Name"), SNew(SEditableTextBox).Text(FText::FromString(Spec->Name))
					.OnTextChanged_Lambda([Spec](const FText& Text) { Spec->Name = FOnyxAssetUtils::SanitizeName(Text.ToString()); }))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				Row(INVTEXT("Size (tiles)"), SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SNumericEntryBox<int32>).MinValue(4).MaxValue(1000).Value_Lambda([Spec]() { return Spec->Width; })
						.OnValueCommitted_Lambda([Spec](int32 Value, ETextCommit::Type) { Spec->Width = FMath::Clamp(Value, 4, 1000); })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)[SNew(STextBlock).Text(INVTEXT("x"))]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SNumericEntryBox<int32>).MinValue(4).MaxValue(1000).Value_Lambda([Spec]() { return Spec->Height; })
						.OnValueCommitted_Lambda([Spec](int32 Value, ETextCommit::Type) { Spec->Height = FMath::Clamp(Value, 4, 1000); })
					])
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				Row(INVTEXT("Ground"), SNew(SCheckBox)
					.ToolTipText(INVTEXT("Fill the bottom two rows with the tile selected in the palette, so there is something to stand on."))
					.IsChecked_Lambda([Spec]() { return Spec->bFillGround ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([Spec](ECheckBoxState State) { Spec->bFillGround = State == ECheckBoxState::Checked; })
					[
						SNew(STextBlock).Text(INVTEXT("Fill the bottom rows with the selected tile"))
					])
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				Hint(FText::Format(INVTEXT("Uses the tile set {0}. The map is placed beside the maps already in the level, so they don't overlap."),
					FText::FromString(Stamp.TileSet->GetName())))
			]
			+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Bottom).HAlign(HAlign_Right)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.Text(INVTEXT("Create Map"))
				.OnClicked_Lambda([this, Spec, WeakWindow]()
				{
					FText Error;
					if (UPaperTileMap* Map = FOnyxTileBuilder::CreateTileMap(*Spec, Error))
					{
						SetTileMap(Map);
						Notify(FText::Format(INVTEXT("Created {0} and placed it in the level"), FText::FromString(Spec->Name)), true);
						if (TSharedPtr<SWindow> Pinned = WeakWindow.Pin())
						{
							Pinned->RequestDestroyWindow();
						}
					}
					else
					{
						Notify(Error, false);
					}
					return FReply::Handled();
				})
			]
		]);
	FSlateApplication::Get().AddModalWindow(Window, AsShared());
}

void SOnyxPainterTool::Save()
{
	TArray<UPackage*> Packages;
	if (UPaperTileMap* Map = Painter.GetTileMap())
	{
		Packages.Add(Map->GetPackage());
	}
	if (Stamp.TileSet.IsValid())
	{
		Packages.Add(Stamp.TileSet->GetPackage()); // tile collision types live in the tile set
	}
	if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
	{
		Packages.Add(World->GetPackage());
	}
	const bool bSaved = FOnyxAssetUtils::SavePackages(Packages);
	Notify(bSaved ? INVTEXT("Saved the map and the level") : INVTEXT("Could not save (is the level untitled? Save it once with Ctrl+S)."), bSaved);
}

void SOnyxPainterTool::HandleObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	if (Object && Object == Painter.GetTileMap() && Painter.GetMapSize() != MapSizeSeen)
	{
		NewSize = MapSizeSeen = Painter.GetMapSize();
	}
	if (Event.GetEventType() == ETransactionObjectEventType::UndoRedo && Object == Painter.GetTileMap())
	{
		Painter.SetLayer(Painter.GetLayer());
		RefreshLayers();
		// The map's cells came back, but the collision built from them isn't part of the undo.
		FOnyxMapPainter::RebuildLevelCollision();
	}
}

// --- Display -------------------------------------------------------------------------

FText SOnyxPainterTool::GetLayerTitle(int32 LayerItem) const
{
	if (LayerItem == EntitiesItem)
	{
		return INVTEXT("Entities");
	}
	if (LayerItem == DecorItem)
	{
		return INVTEXT("Decor (front)");
	}
	if (LayerItem == BackgroundItem)
	{
		return INVTEXT("Background (behind tiles)");
	}
	if (IsImageItem(LayerItem))
	{
		const FOnyxParallaxLayer* Layer = GetImageLayer(ImageIndexOf(LayerItem));
		return Layer ? FText::FromName(Layer->Name) : FText::GetEmpty();
	}
	return Painter.GetLayerName(LayerItem);
}

FText SOnyxPainterTool::GetPaletteTitle() const
{
	if (IsImageLayer())
	{
		const FOnyxParallaxLayer* Layer = GetImageLayer(ActiveImageLayer);
		return FText::Format(INVTEXT("Image Layer  ·  {0}"), Layer ? FText::FromName(Layer->Name) : FText::GetEmpty());
	}
	if (IsObjectLayer())
	{
		return FText::Format(INVTEXT("Objects  ·  {0} layer"), FText::FromName(ActiveObjectLayer));
	}
	return Stamp.TileSet.IsValid() ? FText::Format(INVTEXT("Tile Set  ·  {0}"), FText::FromString(Stamp.TileSet->GetName())) : INVTEXT("Tile Set");
}

FText SOnyxPainterTool::GetCanvasInfo() const
{
	if (!Painter.GetTileMap())
	{
		return INVTEXT("No map");
	}
	const FText Layer = IsObjectLayer() ? FText::FromName(ActiveObjectLayer) : Painter.GetLayerName(Painter.GetLayer());
	const FText Zoom = FText::AsPercent(Canvas.IsValid() ? Canvas->GetZoom() : 1.f);
	if (Canvas.IsValid() && Canvas->IsHovering() && Painter.IsValidCell(Canvas->GetHoverCell()))
	{
		return FText::Format(INVTEXT("{0}  ·  cell {1}, {2}  ·  {3}"), Layer, Canvas->GetHoverCell().X, Canvas->GetHoverCell().Y, Zoom);
	}
	return FText::Format(INVTEXT("{0}  ·  {1}"), Layer, Zoom);
}

FText SOnyxPainterTool::GetStatus() const
{
	const UPaperTileMap* Map = Painter.GetTileMap();
	if (!Map)
	{
		return INVTEXT("Pick a tile set and press New Map, or choose a tile map in the toolbar.");
	}
	const FText Brush = IsObjectLayer()
		? (SelectedBrush.IsValid() ? SelectedBrush->Label : FText::GetEmpty())
		: FText::Format(INVTEXT("{0} x {1} tiles"), Stamp.Size.X, Stamp.Size.Y);
	return FText::Format(INVTEXT("{0}  ·  {1} x {2} tiles of {3} px  ·  brush: {4}{5}"), FText::FromString(Map->GetName()),
		Map->MapWidth, Map->MapHeight, Map->TileWidth, Brush, Map->GetPackage()->IsDirty() ? INVTEXT("  ·  unsaved changes") : FText::GetEmpty());
}

TOptional<FIntRect> SOnyxPainterTool::GetPreviewRect() const
{
	if (!RectStart.IsSet())
	{
		return {};
	}
	return FIntRect(RectStart->ComponentMin(RectEnd), RectStart->ComponentMax(RectEnd));
}

const FSlateBrush* SOnyxPainterTool::GetBrushThumbnail(const TSharedPtr<FOnyxPainterBrush>& Brush)
{
	const FOnyxBrushLibrary& Library = FOnyxBrushLibrary::Get();
	UTexture2D* Texture = nullptr;
	FIntRect Rect;
	if (!Library.GetPreviewSprite(*Brush, Texture, Rect, FPlatformTime::Seconds()))
	{
		return Library.GetIcon(*Brush);
	}
	// The sprite it shows, animated (flipbooks, characters' Idle clip), cropped to the figure: a character's frame is mostly
	// empty space, and the whole frame shrunk to the box leaves a speck. One crop for all its frames (worked out once), so
	// the figure keeps its size while it plays.
	const FString CropKey = Brush->Id.ToString() + TEXT("|") + Texture->GetPathName();
	const FIntRect* Crop = ThumbnailCrops.Find(CropKey);
	if (!Crop)
	{
		TArray<FIntRect> Frames;
		for (int32 Sample = 0; Sample < 120; ++Sample)
		{
			UTexture2D* FrameTexture = nullptr;
			FIntRect Frame;
			if (Library.GetPreviewSprite(*Brush, FrameTexture, Frame, Sample / 30.0) && FrameTexture == Texture)
			{
				Frames.AddUnique(Frame);
			}
		}
		Frames.AddUnique(Rect);
		const TOptional<FIntRect> Visible = FOnyxImageAnalysis::VisibleBounds(Texture, Frames);
		Crop = &ThumbnailCrops.Add(CropKey, Visible.IsSet() ? *Visible : FIntRect(FIntPoint::ZeroValue, Rect.Size()));
	}
	const FIntRect Shown(Rect.Min + Crop->Min.ComponentMax(FIntPoint::ZeroValue), Rect.Min + Crop->Max.ComponentMin(Rect.Size()));
	if (Shown.Width() <= 0 || Shown.Height() <= 0)
	{
		return Library.GetIcon(*Brush);
	}
	TSharedRef<FSlateBrush>& Thumbnail = Thumbnails.FindOrAdd(Brush->Id, MakeShared<FSlateBrush>());
	// Measured against the texture as the GPU has it, like the rectangle itself (a padded texture is bigger than the picture).
	const FVector2f TextureSize(FMath::Max(Texture->GetSurfaceWidth(), 1.f), FMath::Max(Texture->GetSurfaceHeight(), 1.f));
	constexpr float Box = 36.f;
	const float Scale = Box / FMath::Max(Shown.Width(), Shown.Height());
	Thumbnail->SetResourceObject(Texture);
	Thumbnail->DrawAs = ESlateBrushDrawType::Image;
	Thumbnail->ImageSize = FVector2D(Shown.Width() * Scale, Shown.Height() * Scale);
	Thumbnail->SetUVRegion(FBox2f(FVector2f(Shown.Min.X / TextureSize.X, Shown.Min.Y / TextureSize.Y),
		FVector2f(Shown.Max.X / TextureSize.X, Shown.Max.Y / TextureSize.Y)));
	return &Thumbnail.Get();
}
