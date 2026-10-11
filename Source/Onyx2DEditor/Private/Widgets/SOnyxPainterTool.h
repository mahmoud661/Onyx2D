// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Assets/OnyxBrushLibrary.h"
#include "Assets/OnyxMapPainter.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOnyxMapCanvas.h"
#include "Widgets/Views/SListView.h"

class IDetailsView;
class ITableRow;
class STableViewBase;
class UPaperTileMap;
class UPaperTileSet;

enum class EOnyxPaintTool : uint8
{
	Brush,
	Rect,
	Fill,
	Erase,
	Pick,
	Select,
	/** Auto-tiling: paint rough shapes with a terrain, edges and corners pick themselves. */
	Terrain
};

/**
 * LDtk-style level painter.
 *  - Left: the palette. Tile layers show your tile set (click a tile, or drag to pick a block);
 *    object layers show gameplay pieces plus your own Blueprints / flipbooks / sprites / meshes.
 *  - Middle: the map canvas.
 *  - Right: layers (tile layers with collision, plus the Entities and Decor object layers) and the inspector.
 */
class SOnyxPainterTool : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxPainterTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SOnyxPainterTool() override;

	void SetTileMap(UPaperTileMap* TileMap);

private:
	// Layout
	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeMapPicker();
	TSharedRef<SWidget> MakePalettePanel();
	TSharedRef<SWidget> MakeTilePalette();
	TSharedRef<SWidget> MakeTileSheetView();
	TSharedRef<SWidget> MakeCollisionBar();
	TSharedRef<SWidget> MakeTerrainPanel();
	void RefreshTerrains();
	TSharedRef<SWidget> MakeObjectPalette();
	TSharedRef<SWidget> MakeLayersPanel();
	TSharedRef<SWidget> MakeCanvasArea();
	TSharedRef<SWidget> MakeInspector();
	TSharedRef<ITableRow> GenerateLayerRow(TSharedPtr<int32> Item, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<ITableRow> GenerateBrushRow(TSharedPtr<FOnyxPainterBrush> Item, const TSharedRef<STableViewBase>& Owner);
	TSharedPtr<SWidget> MakeLayerContextMenu();

	// State
	void RefreshLayers();
	void RefreshBrushes();
	void SelectLayer(int32 LayerItem);
	bool IsObjectLayer() const { return !ActiveObjectLayer.IsNone(); }
	void SetTileSet(UPaperTileSet* InTileSet);
	void SetTool(EOnyxPaintTool NewTool) { Tool = NewTool; RectStart.Reset(); }
	void SelectActor(AActor* Actor);
	void ToggleLayerVisibility(int32 LayerItem);
	bool IsLayerVisible(int32 LayerItem) const;
	AActor* FindObjectForSelection(const FIntPoint& Cell) const;

	// Editing
	void HandleStroke(EOnyxStrokePhase Phase, FIntPoint Cell, bool bSecondary);
	void ApplyAt(const FIntPoint& Cell, bool bErase);
	void ApplyRect(const FIntRect& Rect, bool bErase);
	FReply HandleKey(const FKeyEvent& Event);
	void AddSelectedAssetsAsBrushes();
	void CreateNewMap();
	void Save();
	void HandleObjectTransacted(UObject* Object, const class FTransactionObjectEvent& Event);

	// Display
	FText GetLayerTitle(int32 LayerItem) const;
	FText GetPaletteTitle() const;
	FText GetStatus() const;
	FText GetCanvasInfo() const;
	TOptional<FIntRect> GetPreviewRect() const;
	const FSlateBrush* GetBrushThumbnail(const TSharedPtr<FOnyxPainterBrush>& Brush);

	FOnyxMapPainter Painter;
	EOnyxPaintTool Tool = EOnyxPaintTool::Brush;
	/** None = a tile layer is active. */
	FName ActiveObjectLayer;
	bool bShowGrid = true;
	bool bShowCollision = false;
	TSet<int32> HiddenLayers;
	TSet<FName> HiddenObjectLayers;
	FOnyxStamp Stamp;
	/** Terrain painted by the Terrain tool (index in the tile set's terrains). */
	int32 ActiveTerrain = INDEX_NONE;
	TSharedPtr<class SWrapBox> TerrainButtons;
	float PaletteZoom = 0.f;
	TSharedPtr<FOnyxPainterBrush> SelectedBrush;
	FIntPoint StrokeOrigin = FIntPoint::ZeroValue;
	FIntPoint LastStrokeCell = FIntPoint::ZeroValue;
	TOptional<FIntPoint> RectStart;
	FIntPoint RectEnd = FIntPoint::ZeroValue;
	bool bRectErase = false;
	TWeakObjectPtr<AActor> SelectedActor;
	bool bDraggingActor = false;
	bool bWarnedNoActor = false;
	FIntPoint NewSize = FIntPoint(60, 24);

	/** Tile layer index, or -1 Entities / -2 Decor. */
	TArray<TSharedPtr<int32>> LayerItems;
	TArray<TSharedPtr<FOnyxPainterBrush>> BrushItems;
	TMap<FName, TSharedRef<FSlateBrush>> Thumbnails;
	TSharedPtr<SListView<TSharedPtr<int32>>> LayerList;
	TSharedPtr<SListView<TSharedPtr<FOnyxPainterBrush>>> BrushList;
	TSharedPtr<SOnyxMapCanvas> Canvas;
	TSharedPtr<IDetailsView> Inspector;
	FDelegateHandle TransactedHandle;
	FDelegateHandle LibraryHandle;
};
