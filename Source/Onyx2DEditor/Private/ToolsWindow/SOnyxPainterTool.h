// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Level/OnyxBrushLibrary.h"
#include "Level/OnyxMapPainter.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOnyxMapCanvas.h"
#include "Widgets/Views/SListView.h"

class AOnyxParallaxBackground;
class IDetailsView;
class SBox;
class FScopedTransaction;
struct FOnyxParallaxLayer;
class UTexture2D;
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
 *  - Right: layers (the Decor, Entities and Background object layers, the tile layers with collision, and behind them the
 *    image layers: pictures placed in the level, scrolling with parallax) and the inspector.
 * The palette and the right-hand panels can be hidden (toolbar toggles) to give the map the room.
 */
class SOnyxPainterTool : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxPainterTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SOnyxPainterTool() override;

	void SetTileMap(UPaperTileMap* TileMap);
	/** Paint with this tile set (New Map uses it too). */
	void SetTileSet(UPaperTileSet* InTileSet);

private:
	// Layout
	/** bLabels off: icons only (names in the tooltips), for a narrow panel. */
	TSharedRef<SWidget> MakeToolbar(bool bLabels);
	/** Show / hide the palette and the right-hand panels. */
	TSharedRef<SWidget> MakePanelToggles();
	/** The selected image layer's settings (left panel): picture, size, repeat, parallax, colour. */
	TSharedRef<SWidget> MakeImageLayerPanel();
	TSharedRef<ITableRow> GenerateImageLayerRow(int32 LayerIndex, const TSharedPtr<int32>& Item, const TSharedRef<STableViewBase>& Owner);
	/** + in the Layers list: a tile layer, an image layer or a colour layer. */
	TSharedRef<SWidget> MakeAddLayerMenu();
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

	// Image layers (the level's Onyx Parallax Background, made when the first one is added)
	AOnyxParallaxBackground* FindBackdrop() const;
	AOnyxParallaxBackground* CreateBackdrop();
	FOnyxParallaxLayer* GetImageLayer(int32 LayerIndex) const;
	/**
	 * An image layer showing Texture (none: a plain colour band), in front of the other image layers, its pixels as big as
	 * the map's, its middle at AtMapPixels (else the middle of the view) as the view shows it now. Returns its index.
	 */
	int32 AddImageLayer(UTexture2D* Texture, TOptional<FVector2D> AtMapPixels);
	/** One image layer per texture (each in front of the last) and selects the last. */
	void AddImageLayers(const TArray<UTexture2D*>& Textures, TOptional<FVector2D> AtMapPixels);
	void RemoveImageLayer(int32 LayerIndex);
	/** Changes the backdrop in one undoable step and rebuilds its layers. */
	void EditBackdrop(const FText& Title, TFunctionRef<void(AOnyxParallaxBackground&)> Edit);
	/** Changes the selected image layer in one undoable step. */
	void EditImageLayer(const FText& Title, TFunctionRef<void(FOnyxParallaxLayer&)> Edit);
	/** A change made over time (a drag, a slider): one undo step from Begin to End; Live applies each step of it. */
	void BeginImageLayerChange(const FText& Title);
	void ApplyImageLayerLive(TFunctionRef<void(FOnyxParallaxLayer&)> Edit);
	void EndImageLayerChange();
	/** The selected image layer's picture size: 1 = its pixels as big as the map's. */
	float GetImageLayerScale(const FOnyxParallaxLayer& Layer) const;
	/** The corner of the selected image layer's picture under a map point (1 top-left, 2 top-right, 3 bottom-right, 4 bottom-left; 0 none). */
	int32 GetImageCornerAt(const FVector2D& MapPixels) const;
	TArray<UTexture2D*> GetSelectedTextures() const;
	/** Keeps the list in step with edits made elsewhere (the Details panel, undo, another level opened). */
	EActiveTimerReturnType PollBackdrop(double CurrentTime, float DeltaTime);
	/** Repeating layers as wide as the level and then some: they cover the view wherever the camera goes. */
	void FitBackdropToLevel();
	/** World centimetres per map pixel (the map's art scale); 0 without a map in the level. */
	double GetCmPerMapPixel() const;

	// State
	void RefreshLayers();
	void RefreshBrushes();
	void SelectLayer(int32 LayerItem);
	bool IsObjectLayer() const { return !ActiveObjectLayer.IsNone(); }
	bool IsImageLayer() const { return ActiveImageLayer != INDEX_NONE; }
	void SetTool(EOnyxPaintTool NewTool) { Tool = NewTool; RectStart.Reset(); }
	void SelectActor(AActor* Actor);
	void ToggleLayerVisibility(int32 LayerItem);
	bool IsLayerVisible(int32 LayerItem) const;
	AActor* FindObjectForSelection(const FIntPoint& Cell) const;

	// Editing
	void HandleStroke(EOnyxStrokePhase Phase, FIntPoint Cell, bool bSecondary);
	void ApplyAt(const FIntPoint& Cell, bool bErase);
	void ApplyRect(const FIntRect& Rect, bool bErase);
	/** Adds the selected area brush's object (water) over a block of cells and selects it. */
	void PlaceArea(const FIntRect& Cells);
	/** Edges of the selected area object near a map point (EAreaEdge flags; 0 = none). */
	int32 GetAreaEdgesAt(const FVector2D& MapPixels) const;
	/** The block of cells the dragged area object should cover, with the mouse over Cell. */
	FIntRect GetDraggedArea(const FIntPoint& Cell) const;
	TOptional<EMouseCursor::Type> GetCursorFor(FVector2D MapPixels) const;
	/** Grows or shrinks the map from its edges (canvas border drag, Inspector buttons); warns about objects left outside. */
	void ResizeMapEdges(const FOnyxMapEdges& Edges);
	/** [-] Side [+]: removes or adds EdgeStep cells on one side of the map. */
	TSharedRef<SWidget> MakeEdgeButtons(const FText& Side, const FText& Where, int32 FOnyxMapEdges::* Edge, bool bColumns);
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
	/**
	 * Parallax preview: image layers slide as the view pans, as the game's camera would see them from its middle. Off (the
	 * default): they stay where they are placed, moving with the map like the tiles.
	 */
	bool bShowParallax = false;
	/** The selected image layer (index in the backdrop's layers); INDEX_NONE = a tile or object layer is selected. */
	int32 ActiveImageLayer = INDEX_NONE;
	/** Image layers hidden in the painter only. */
	TSet<int32> HiddenImageLayers;
	/** An image layer change in progress (drag, slider): one undo step. */
	TUniquePtr<FScopedTransaction> ImageChange;
	/** Dragging the selected image layer: by a corner (resize, 1-4) or anywhere (move, 0), from where, and what it was. */
	bool bDraggingImage = false;
	int32 ImageGrabCorner = 0;
	FVector2D ImageGrabStart = FVector2D::ZeroVector;
	FBox2D ImageStartRect = FBox2D(ForceInit);
	FVector2D ImageStartOffset = FVector2D::ZeroVector;
	float ImageStartHeight = 0.f;
	bool bShowPalette = true;
	bool bShowSidePanels = true;
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
	/** Dragging the selected area object: moving it, or resizing it by these edges (EAreaEdge flags). */
	bool bMovingArea = false;
	int32 GrabbedEdges = 0;
	FIntRect GrabStartArea;
	FIntPoint GrabStartCell = FIntPoint::ZeroValue;
	bool bWarnedNoActor = false;
	FIntPoint NewSize = FIntPoint(60, 24);
	/** The map size NewSize last followed: when the map changes size (here, in an undo, in another editor), the fields follow it. */
	FIntPoint MapSizeSeen = FIntPoint::ZeroValue;
	/** Cells the Inspector's edge buttons add or remove. */
	int32 EdgeStep = 4;

	/** Tile layer index, or -1 Entities / -2 Decor. */
	TArray<TSharedPtr<int32>> LayerItems;
	TArray<TSharedPtr<FOnyxPainterBrush>> BrushItems;
	TMap<FName, TSharedRef<FSlateBrush>> Thumbnails;
	TSharedPtr<SListView<TSharedPtr<int32>>> LayerList;
	TSharedPtr<SListView<TSharedPtr<FOnyxPainterBrush>>> BrushList;
	TSharedPtr<SOnyxMapCanvas> Canvas;
	TSharedPtr<IDetailsView> Inspector;
	/** Holds the toolbar: its width picks labels or icons only. */
	TSharedPtr<SBox> ToolbarArea;
	mutable TWeakObjectPtr<AOnyxParallaxBackground> BackdropActor;
	/** Per brush (and texture): the part of its frames anything is drawn in, so thumbnails show the figure, not its empty frame. */
	TMap<FString, FIntRect> ThumbnailCrops;
	FDelegateHandle TransactedHandle;
	FDelegateHandle LibraryHandle;
};
