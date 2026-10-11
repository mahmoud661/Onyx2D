// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Assets/OnyxMapPainter.h"
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
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Frames the whole map on the next paint. */
	void RequestFit() { bFitRequested = true; }
	FIntPoint GetHoverCell() const { return HoverCell; }
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
	void DrawBrush(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FOnyxPainterBrush& Brush,
		const FVector2f& CellMin, const FVector2f& CellSize, float Opacity) const;

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

	mutable FVector2D Pan = FVector2D(40.f, 40.f);
	mutable float Zoom = 2.f;
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
