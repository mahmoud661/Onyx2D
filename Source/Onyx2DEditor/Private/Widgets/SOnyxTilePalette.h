// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class UPaperTileSet;

DECLARE_DELEGATE_TwoParams(FOnOnyxTilesSelected, FIntPoint /*Min*/, FIntPoint /*Size*/);

/**
 * Tile set sheet: click a tile, or drag across several to pick a block (a stamp).
 * Zoom 0 fits the width; put it in a scroll box for big sheets.
 */
class SOnyxTilePalette : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxTilePalette) {}
		SLATE_ATTRIBUTE(UPaperTileSet*, TileSet)
		/** Selected block in tile-grid coordinates (Min inclusive, Max exclusive). */
		SLATE_ATTRIBUTE(FIntRect, Selection)
		SLATE_ATTRIBUTE(float, Zoom)
		/** Tint each tile by its collision type. */
		SLATE_ATTRIBUTE(bool, ShowCollision)
		SLATE_EVENT(FOnOnyxTilesSelected, OnTilesSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void OnMouseLeave(const FPointerEvent& Event) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override;

private:
	float GetScale(float Width) const;
	/** Tile grid coordinate under a screen position (INDEX_NONE components when outside). */
	FIntPoint TileAt(const FGeometry& Geometry, const FVector2D& ScreenPosition) const;
	FVector2f TileTopLeft(const FIntPoint& Tile, float Scale) const;
	void EmitSelection(const FIntPoint& A, const FIntPoint& B);

	TAttribute<UPaperTileSet*> TileSet;
	TAttribute<FIntRect> Selection;
	TAttribute<float> Zoom;
	TAttribute<bool> ShowCollision;
	FOnOnyxTilesSelected OnTilesSelected;

	mutable FSlateBrush SheetBrush;
	mutable float LastWidth = 240.f;
	FIntPoint HoverTile = FIntPoint(INDEX_NONE, INDEX_NONE);
	TOptional<FIntPoint> DragStart;
};
