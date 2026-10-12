// Copyright Onyx2D. All Rights Reserved.

#include "ToolsWindow/SOnyxPainterTool.h"
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
	constexpr int32 EntitiesItem = -1;
	constexpr int32 DecorItem = -2;
	constexpr int32 BackgroundItem = -3;

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

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			ToolbarStrip(MakeToolbar(), MakeMapPicker())
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			.PhysicalSplitterHandleSize(2.f)
			+ SSplitter::Slot().Value(0.25f)
			[
				MakePalettePanel()
			]
			+ SSplitter::Slot().Value(0.53f)
			[
				MakeCanvasArea()
			]
			+ SSplitter::Slot().Value(0.22f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				.PhysicalSplitterHandleSize(2.f)
				+ SSplitter::Slot().Value(0.4f)[MakeLayersPanel()]
				+ SSplitter::Slot().Value(0.6f)[MakeInspector()]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			StatusBar(TAttribute<FText>::CreateSP(this, &SOnyxPainterTool::GetStatus))
		]
	];

	TransactedHandle = FCoreUObjectDelegates::OnObjectTransacted.AddSP(this, &SOnyxPainterTool::HandleObjectTransacted);
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

TSharedRef<SWidget> SOnyxPainterTool::MakeToolbar()
{
	FSlimHorizontalToolBarBuilder Toolbar(nullptr, FMultiBoxCustomization::None);
	Toolbar.SetLabelVisibility(EVisibility::Visible);
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
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([this]() { Canvas->RequestFit(); }), HasMap),
		NAME_None, INVTEXT("Fit"), INVTEXT("Frame the whole map (Home)."), FOnyxEditorStyle::Icon("Onyx2D.Tool.Fit"));
	Toolbar.AddSeparator();
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxPainterTool::Save), HasMap),
		NAME_None, INVTEXT("Save"), INVTEXT("Save the tile map and the level."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"));
	return Toolbar.MakeWidget();
}

TSharedRef<SWidget> SOnyxPainterTool::MakeMapPicker()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(STextBlock).Text(INVTEXT("Tile Map"))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(240.f)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UPaperTileMap::StaticClass())
				.DisplayThumbnail(false)
				.AllowCreate(false)
				.ObjectPath_Lambda([this]() { return Painter.GetTileMap() ? Painter.GetTileMap()->GetPathName() : FString(); })
				.OnObjectChanged_Lambda([this](const FAssetData& Asset) { SetTileMap(Cast<UPaperTileMap>(Asset.GetAsset())); })
			]
		];
}

TSharedRef<SWidget> SOnyxPainterTool::MakePalettePanel()
{
	const TSharedRef<SWidget> ZoomButtons = SNew(SHorizontalBox)
		.Visibility_Lambda([this]() { return IsObjectLayer() ? EVisibility::Collapsed : EVisibility::Visible; })
		+ SHorizontalBox::Slot().AutoWidth()[IconButton("Icons.Minus", INVTEXT("Zoom out"), [this]() { PaletteZoom = FMath::Max((PaletteZoom > 0.f ? PaletteZoom : 2.f) - 1.f, 1.f); })]
		+ SHorizontalBox::Slot().AutoWidth()[IconButton("Onyx2D.Tool.Fit.Small", INVTEXT("Fit to the panel"), [this]() { PaletteZoom = 0.f; }, true)]
		+ SHorizontalBox::Slot().AutoWidth()[IconButton("Icons.Plus", INVTEXT("Zoom in"), [this]() { PaletteZoom = FMath::Min((PaletteZoom > 0.f ? PaletteZoom : 2.f) + 1.f, 12.f); })];

	// Tiles / Objects: what the left panel paints with (switches between the tile layer and the Entities layer).
	auto ModeToggle = [this](const FText& Label, const FText& Tooltip, bool bObjects)
	{
		return SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.ToolTipText(Tooltip)
			.IsChecked_Lambda([this, bObjects]() { return IsObjectLayer() == bObjects ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this, bObjects](ECheckBoxState)
			{
				if (IsObjectLayer() != bObjects)
				{
					SelectLayer(bObjects ? EntitiesItem : Painter.GetLayer());
					RefreshLayers();
				}
			})
			[
				SNew(SBox).Padding(FMargin(8.f, 1.f))[SNew(STextBlock).Text(Label)]
			];
	};
	const TSharedRef<SWidget> HeaderExtra = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 2.f, 0.f)
		[
			ModeToggle(INVTEXT("Tiles"), INVTEXT("Paint tiles on the selected tile layer."), false)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
		[
			ModeToggle(INVTEXT("Objects"), INVTEXT("Place gameplay objects (player start, foes, pickups, lights...) and your own Blueprints. Select tool (S) picks and drags placed objects on any layer."), true)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			ZoomButtons
		];

	return Section(TAttribute<FText>::CreateSP(this, &SOnyxPainterTool::GetPaletteTitle),
		SNew(SWidgetSwitcher)
		.WidgetIndex_Lambda([this]() { return IsObjectLayer() ? 1 : 0; })
		+ SWidgetSwitcher::Slot()[MakeTilePalette()]
		+ SWidgetSwitcher::Slot()[MakeObjectPalette()],
		HeaderExtra);
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
				SNew(SBox).WidthOverride(28.f).HeightOverride(28.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SImage).Image(GetBrushThumbnail(Item))
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
				.Visibility(Item->bCustom ? EVisibility::Visible : EVisibility::Collapsed)
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

	const TSharedRef<SWidget> AddLayer = SNew(SBox)
		.IsEnabled_Lambda([this]() { return Painter.GetTileMap() != nullptr; })
		[
			IconButton("Icons.Plus", INVTEXT("Add a tile layer in front of the others"), [this]() { Painter.AddLayer(); RefreshLayers(); })
		];
	return Section(INVTEXT("Layers"), LayerList.ToSharedRef(), AddLayer);
}

TSharedRef<ITableRow> SOnyxPainterTool::GenerateLayerRow(TSharedPtr<int32> Item, const TSharedRef<STableViewBase>& Owner)
{
	const int32 LayerItem = *Item;
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

TSharedRef<SWidget> SOnyxPainterTool::MakeCanvasArea()
{
	SAssignNew(Canvas, SOnyxMapCanvas)
		.Painter(&Painter)
		.ShowGrid_Lambda([this]() { return bShowGrid; })
		.ShowCollision_Lambda([this]() { return bShowCollision; })
		.HiddenObjectLayers_Lambda([this]() { return HiddenObjectLayers; })
		.HiddenLayers_Lambda([this]() { return HiddenLayers; })
		.PreviewStamp_Lambda([this]()
		{
			const bool bShow = !IsObjectLayer() && (Tool == EOnyxPaintTool::Brush || Tool == EOnyxPaintTool::Fill);
			return bShow ? Stamp : FOnyxStamp();
		})
		.PreviewBrush_Lambda([this]() { return IsObjectLayer() && Tool == EOnyxPaintTool::Brush ? SelectedBrush : nullptr; })
		.PreviewRect(this, &SOnyxPainterTool::GetPreviewRect)
		.SelectedActor_Lambda([this]() { return SelectedActor; })
		.OnStroke(this, &SOnyxPainterTool::HandleStroke)
		.OnKey(this, &SOnyxPainterTool::HandleKey);

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			Canvas.ToSharedRef()
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
					SNew(SSpinBox<int32>).MinValue(1).MaxValue(1024)
					.Value_Lambda([this]() { return NewSize.X; })
					.OnValueChanged_Lambda([this](int32 V) { NewSize.X = V; })
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SSpinBox<int32>).MinValue(1).MaxValue(1024)
					.Value_Lambda([this]() { return NewSize.Y; })
					.OnValueChanged_Lambda([this](int32 V) { NewSize.Y = V; })
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(INVTEXT("Resize"))
					.IsEnabled_Lambda([this]() { return Painter.GetTileMap() && NewSize != Painter.GetMapSize(); })
					.OnClicked_Lambda([this]() { Painter.Resize(NewSize); Canvas->RequestFit(); return FReply::Handled(); })
				]
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
				Hint(INVTEXT("Left drag: use tool · Right drag: erase\nMiddle drag or Space + drag: pan · Wheel: zoom\nB brush · R rectangle · F fill · E erase · I pick · S select\nG grid · Home fit · Del delete object\n\nSelect an object to edit it here."))
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
		NewSize = Painter.GetMapSize();
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
	}
	if (LayerList.IsValid())
	{
		LayerList->RequestListRefresh();
		const int32 Active = ActiveObjectLayer == FOnyxBrushLibrary::EntitiesLayer ? EntitiesItem
			: ActiveObjectLayer == FOnyxBrushLibrary::DecorLayer ? DecorItem
			: ActiveObjectLayer == FOnyxBrushLibrary::BackgroundLayer ? BackgroundItem
			: Painter.GetLayer();
		if (const TSharedPtr<int32>* Item = LayerItems.FindByPredicate([Active](const TSharedPtr<int32>& I) { return *I == Active; }))
		{
			LayerList->SetSelection(*Item, ESelectInfo::Direct);
		}
	}
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
			if (Brush->bCustom == bCustom)
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
	ActiveObjectLayer = ObjectLayerOf(LayerItem);
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
	if (LayerItem < 0)
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
	else if (SelectedBrush.IsValid())
	{
		Painter.PlaceObject(Cell, *SelectedBrush, ActiveObjectLayer);
	}
}

void SOnyxPainterTool::ApplyRect(const FIntRect& Rect, bool bErase)
{
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

	// Select: click an object, drag it to another cell.
	if (Tool == EOnyxPaintTool::Select && !bSecondary)
	{
		if (Phase == EOnyxStrokePhase::Begin)
		{
			AActor* Actor = FindObjectForSelection(Cell);
			SelectActor(Actor);
			bDraggingActor = Actor != nullptr;
			if (bDraggingActor)
			{
				Painter.BeginStroke(INVTEXT("Move Object"));
			}
		}
		else if (Phase == EOnyxStrokePhase::Move && bDraggingActor)
		{
			Painter.MoveObject(SelectedActor.Get(), Cell);
		}
		else if (Phase == EOnyxStrokePhase::End && bDraggingActor)
		{
			bDraggingActor = false;
			Painter.EndStroke();
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
	if (Event.GetEventType() == ETransactionObjectEventType::UndoRedo && Object == Painter.GetTileMap())
	{
		Painter.SetLayer(Painter.GetLayer());
		RefreshLayers();
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
	return Painter.GetLayerName(LayerItem);
}

FText SOnyxPainterTool::GetPaletteTitle() const
{
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
	if (!Library.GetPreviewSprite(*Brush, Texture, Rect))
	{
		return Library.GetIcon(*Brush);
	}
	// Sprite / flipbook brushes show their first frame.
	TSharedRef<FSlateBrush>& Thumbnail = Thumbnails.FindOrAdd(Brush->Id, MakeShared<FSlateBrush>());
	const FIntPoint TextureSize = Texture->GetImportedSize();
	const float Scale = 28.f / FMath::Max(Rect.Width(), Rect.Height());
	Thumbnail->SetResourceObject(Texture);
	Thumbnail->DrawAs = ESlateBrushDrawType::Image;
	Thumbnail->ImageSize = FVector2D(Rect.Width() * Scale, Rect.Height() * Scale);
	Thumbnail->SetUVRegion(FBox2f(FVector2f(float(Rect.Min.X) / TextureSize.X, float(Rect.Min.Y) / TextureSize.Y),
		FVector2f(float(Rect.Max.X) / TextureSize.X, float(Rect.Max.Y) / TextureSize.Y)));
	return &Thumbnail.Get();
}
