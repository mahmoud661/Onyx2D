// Copyright Onyx2D. All Rights Reserved.

#include "Widgets/SOnyxTileTool.h"
#include "Assets/OnyxTileBuilder.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "IDetailsView.h"
#include "PaperTileMap.h"
#include "PaperTileSet.h"
#include "Styling/AppStyle.h"
#include "Style/OnyxEditorStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Tools/OnyxToolOptions.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/OnyxToolWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOnyxSpriteSheetView.h"
#include "Widgets/Text/STextBlock.h"

using namespace OnyxTools;

namespace
{
	int32 TilesAlong(int32 TextureSize, int32 TileSize, int32 Margin, int32 Spacing)
	{
		return TileSize > 0 ? FMath::Max((TextureSize - Margin * 2 + Spacing) / (TileSize + Spacing), 0) : 0;
	}
}

void SOnyxTileTool::Construct(const FArguments& InArgs)
{
	OnTileMapCreated = InArgs._OnTileMapCreated;
	Options.Reset(NewObject<UOnyxTileOptions>(GetTransientPackage(), NAME_None, RF_Transactional));
	Details = MakeDetailsView(Options.Get());
	Details->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent& Event)
	{
		LastResult = FText::GetEmpty();
		if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UOnyxTileOptions, TileSheet))
		{
			SetTileSheet(Options->TileSheet);
		}
	});

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			ToolbarStrip(MakeToolbar())
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			.PhysicalSplitterHandleSize(2.f)
			+ SSplitter::Slot().Value(0.38f)
			[
				Section(INVTEXT("Tiles"), Details.ToSharedRef())
			]
			+ SSplitter::Slot().Value(0.62f)
			[
				Section(INVTEXT("Tile Sheet"),
					SNew(SOnyxSpriteSheetView)
					.Texture_Lambda([this]() { return Options->TileSheet.Get(); })
					.CellSize_Lambda([this]() { return Options->TileSize; })
					.Margin_Lambda([this]() { return Options->Margin; })
					.Spacing_Lambda([this]() { return Options->Spacing; })
					.HighlightRects(this, &SOnyxTileTool::GetGroundTileRect)
					.MaxHeight(4096.f),
					SNew(STextBlock).Text(this, &SOnyxTileTool::GetSheetInfo).ColorAndOpacity(FSlateColor::UseSubduedForeground()))
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			StatusBar(TAttribute<FText>::CreateSP(this, &SOnyxTileTool::GetStatus))
		]
	];
}

TSharedRef<SWidget> SOnyxTileTool::MakeToolbar()
{
	FSlimHorizontalToolBarBuilder Toolbar(nullptr, FMultiBoxCustomization::None);
	Toolbar.SetLabelVisibility(EVisibility::Visible);
	const FCanExecuteAction HasSheet = FCanExecuteAction::CreateLambda([this]() { return Options->TileSheet != nullptr; });
	const FCanExecuteAction HasTileSet = FCanExecuteAction::CreateLambda([this]() { return Options->TileSet != nullptr || Options->TileSheet != nullptr; });

	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([this]() { CreateTileSet(); }), HasSheet),
		NAME_None, INVTEXT("Create Tile Set"), INVTEXT("Cut the sheet into a Paper2D tile set (with collision)."), FOnyxEditorStyle::Icon("Onyx2D.Tab.Tiles"));
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([this]()
	{
		if (!Options->TileSet && !CreateTileSet())
		{
			return;
		}
		CreateTileMap();
	}), HasTileSet),
		NAME_None, INVTEXT("Create Tile Map"), INVTEXT("Create a lit tile map with collision (and the tile set first if needed) and place it in the level."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"));
	Toolbar.AddSeparator();
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxTileTool::UseContentBrowserSelection)),
		NAME_None, INVTEXT("Use Selected"), INVTEXT("Use the texture or tile set selected in the Content Browser."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"));
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([this]() { OpenAsset(Options->TileSet); }),
		FCanExecuteAction::CreateLambda([this]() { return Options->TileSet != nullptr; })),
		NAME_None, INVTEXT("Edit Tile Set"), INVTEXT("Open the tile set editor (per-tile collision, metadata)."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"));
	return Toolbar.MakeWidget();
}

void SOnyxTileTool::SetTileSheet(UTexture2D* Texture)
{
	if (!Texture)
	{
		return;
	}
	Options->TileSheet = Texture;
	FString Base = Texture->GetName();
	Base.RemoveFromStart(TEXT("T_"));
	Options->TileSetName = TEXT("TS_") + Base;
	Options->TileMapName = TEXT("TM_") + Base;
	Details->ForceRefresh();
}

bool SOnyxTileTool::CreateTileSet()
{
	FText Error;
	UPaperTileSet* TileSet = FOnyxTileBuilder::CreateTileSet(Options->ToTileSetSpec(), Error);
	if (!TileSet)
	{
		LastResult = Error;
		Notify(Error, false);
		return false;
	}
	Options->TileSet = TileSet;
	Details->ForceRefresh();
	LastResult = FText::Format(INVTEXT("Created tile set {0} ({1} tiles)."), FText::FromString(TileSet->GetName()), TileSet->GetTileCount());
	Notify(LastResult, true);
	TArray<UObject*> Created = { TileSet };
	GEditor->SyncBrowserToObjects(Created, false);
	return true;
}

bool SOnyxTileTool::CreateTileMap()
{
	FText Error;
	UPaperTileMap* TileMap = FOnyxTileBuilder::CreateTileMap(Options->ToTileMapSpec(), Error);
	if (!TileMap)
	{
		LastResult = Error;
		Notify(Error, false);
		return false;
	}
	LastResult = FText::Format(INVTEXT("Created tile map {0} ({1} x {2}){3}. Paint it in the Level Painter."), FText::FromString(TileMap->GetName()),
		TileMap->MapWidth, TileMap->MapHeight, Options->bPlaceInLevel ? INVTEXT(" and placed it in the level") : FText::GetEmpty());
	Notify(LastResult, true);
	if (!OnTileMapCreated.ExecuteIfBound(TileMap))
	{
		OpenAsset(TileMap);
	}
	return true;
}

void SOnyxTileTool::UseContentBrowserSelection()
{
	TArray<FAssetData> Selection;
	FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().GetSelectedAssets(Selection);
	for (const FAssetData& Asset : Selection)
	{
		if (UTexture2D* Texture = Cast<UTexture2D>(Asset.GetAsset()))
		{
			SetTileSheet(Texture);
			return;
		}
		if (UPaperTileSet* TileSet = Cast<UPaperTileSet>(Asset.GetAsset()))
		{
			Options->TileSet = TileSet;
			Options->TileSheet = TileSet->GetTileSheetTexture();
			Options->TileSize = TileSet->GetTileSize();
			Details->ForceRefresh();
			return;
		}
	}
	Notify(INVTEXT("Select a texture or tile set in the Content Browser first."), false);
}

void SOnyxTileTool::OpenAsset(UObject* Asset) const
{
	if (Asset)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
	}
}

TArray<FIntRect> SOnyxTileTool::GetGroundTileRect() const
{
	const UTexture2D* Texture = Options->TileSheet;
	if (!Texture || !Options->bFillGround || Options->TileSize.X <= 0)
	{
		return {};
	}
	const int32 Columns = TilesAlong(Texture->GetImportedSize().X, Options->TileSize.X, Options->Margin, Options->Spacing);
	if (Columns <= 0)
	{
		return {};
	}
	const FIntPoint Cell(Options->GroundTile % Columns, Options->GroundTile / Columns);
	const FIntPoint Min(Options->Margin + Cell.X * (Options->TileSize.X + Options->Spacing), Options->Margin + Cell.Y * (Options->TileSize.Y + Options->Spacing));
	return { FIntRect(Min, Min + Options->TileSize) };
}

FText SOnyxTileTool::GetSheetInfo() const
{
	const UTexture2D* Texture = Options->TileSheet;
	if (!Texture)
	{
		return FText::GetEmpty();
	}
	const FIntPoint Size = Texture->GetImportedSize();
	const int32 Columns = TilesAlong(Size.X, Options->TileSize.X, Options->Margin, Options->Spacing);
	const int32 Rows = TilesAlong(Size.Y, Options->TileSize.Y, Options->Margin, Options->Spacing);
	return FText::Format(INVTEXT("{0} x {1} tiles  ·  {2} total  ·  ground tile outlined"), Columns, Rows, Columns * Rows);
}

FText SOnyxTileTool::GetStatus() const
{
	if (!LastResult.IsEmpty())
	{
		return LastResult;
	}
	if (!Options->TileSheet && !Options->TileSet)
	{
		return INVTEXT("Choose a tile sheet texture (or right-click a texture in the Content Browser > Create Onyx Tile Set).");
	}
	return FText::Format(INVTEXT("Output: {0}/{1} and {0}/{2}"), FText::FromString(Options->Folder.Path), FText::FromString(Options->TileSetName), FText::FromString(Options->TileMapName));
}
