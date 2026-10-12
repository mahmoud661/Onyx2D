// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Level/OnyxMapPainter.h"
#include "PaperTileLayer.h"
#include "Widgets/SLeafWidget.h"

struct FOnyxPainterBrush;

enum class EOnyxStrokePhase : uint8
{
	Begin,
	Move,
	End
};

DECLARE_DELEGATE_ThreeParams(FOnOnyxCanvasStroke, EOnyxStrokePhase /*Phase*/, FIntPoint /*Cell*/, bool /*bSecondary*/);
DECLARE_DELEGATE_RetVal_OneParam(FReply, FOnOnyxCanvasKey, const FKeyEvent&);
/** The mouse cursor to show over a point of the map (in map pixels); unset = the default. */
DECLARE_DELEGATE_RetVal_OneParam(TOptional<EMouseCursor::Type>, FOnOnyxCanvasCursor, FVector2D /*MapPixels*/);

/**
 * LDtk-style map canvas: draws the tile layers and gameplay entities of a tile map,
 * pans (middle mouse or Space + drag), zooms around the cursor and reports strokes in cell space.
 * It never edits anything itself.
 */
class SOnyxMapCanvas : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxMapCanvas) {}
		SLATE_ARGUMENT(FOnyxMapPainter*, Painter)
		SLATE_ATTRIBUTE(bool, ShowGrid)
		/** Overlay the effective collision of every cell. */
		SLATE_ATTRIBUTE(bool, ShowCollision)
		SLATE_ATTRIBUTE(TSet<FName>, HiddenObjectLayers)
		SLATE_ATTRIBUTE(TSet<int32>, HiddenLayers)
		/** Tiles drawn under the cursor as a preview (invalid = none). */
		SLATE_ATTRIBUTE(FOnyxStamp, PreviewStamp)
		/** Object drawn under the cursor as a preview. */
		SLATE_ATTRIBUTE(TSharedPtr<FOnyxPainterBrush>, PreviewBrush)
		/** Area highlighted by the rectangle tool. */
		SLATE_ATTRIBUTE(TOptional<FIntRect>, PreviewRect)
		SLATE_ATTRIBUTE(TWeakObjectPtr<AActor>, SelectedActor)
		SLATE_EVENT(FOnOnyxCanvasStroke, OnStroke)
		SLATE_EVENT(FOnOnyxCanvasKey, OnKey)
		SLATE_EVENT(FOnOnyxCanvasCursor, OnQueryCursor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Frames the whole map on the next paint. */
	void RequestFit() { bFitRequested = true; }
	FIntPoint GetHoverCell() const { return HoverCell; }
	/** The cursor in map pixels from the map's top-left corner (not rounded to a cell). */
	FVector2D GetHoverMapPixels() const { return (HoverLocal - Pan) / FMath::Max(Zoom, 0.01f); }
	bool IsHovering() const { return bHasHover; }
	float GetZoom() const { return Zoom; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(400.f, 300.f); }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void OnMouseLeave(const FPointerEvent& Event) override;
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override;

private:
	float CellPixels() const;
	FVector2f CellToLocal(const FIntPoint& Cell) const;
	FIntPoint LocalToCell(const FVector2D& Local) const;
	void FitTo(const FVector2D& Area) const;
	void DrawTile(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FPaperTileInfo& Tile,
		const FVector2f& Position, float Size, const FLinearColor& Tint) const;
	/** Draws a brush on a cell: the sprite for sprite/flipbook brushes, a colored box + icon otherwise. */
	/** Draws an actor's sprites where they are in the level (animated); false if it shows none. Out = the area covered. */
	bool DrawActorSprites(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const AActor* Actor,
		const FVector2f& MapMin, FVector2f& OutMin, FVector2f& OutMax) const;
	void DrawBrush(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FOnyxPainterBrush& Brush,
		const FVector2f& CellMin, const FVector2f& CellSize, float Opacity) const;
	/** Draws an area object (water) over a rectangle: its colours, its top edge and, when selected, resize handles. Actor may be null (preview). */
	void DrawArea(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FOnyxPainterBrush& Brush, const AActor* Actor,
		const FVector2f& Min, const FVector2f& Max, float Opacity, bool bSelected) const;
	/** Where an area object is on the canvas (from where it really is in the level). */
	bool GetAreaRect(const FOnyxPainterBrush& Brush, const AActor* Actor, const FVector2f& MapMin, FVector2f& OutMin, FVector2f& OutMax) const;

	FOnyxMapPainter* Painter = nullptr;
	TAttribute<bool> ShowGrid;
	TAttribute<bool> ShowCollision;
	TAttribute<TSet<FName>> HiddenObjectLayers;
	TAttribute<TSet<int32>> HiddenLayers;
	TAttribute<FOnyxStamp> PreviewStamp;
	TAttribute<TSharedPtr<FOnyxPainterBrush>> PreviewBrush;
	TAttribute<TOptional<FIntRect>> PreviewRect;
	TAttribute<TWeakObjectPtr<AActor>> SelectedActor;
	FOnOnyxCanvasStroke OnStroke;
	FOnOnyxCanvasKey OnKey;
	FOnOnyxCanvasCursor OnQueryCursor;
	FVector2D HoverLocal = FVector2D::ZeroVector;

	mutable FVector2D Pan = FVector2D(40.f, 40.f);
	mutable float Zoom = 2.f;
	/** Brushes for drawing placed actors' sprites (one per sprite drawn this frame). */
	mutable TArray<FSlateBrush> ActorBrushes;
	mutable bool bFitRequested = true;
	mutable FSlateBrush TileBrush;
	mutable FSlateBrush SpriteBrush;

	FIntPoint HoverCell = FIntPoint(-1, -1);
	bool bHasHover = false;
	bool bPanning = false;
	bool bStroking = false;
	bool bStrokeSecondary = false;
	bool bSpaceHeld = false;
};
