// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Widgets/SLeafWidget.h"

class UTexture2D;

/** Draws a texture with its frame/tile grid, row numbers and highlighted rows. */
class SOnyxSpriteSheetView : public SLeafWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SOnyxSpriteSheetView)
		: _CellSize(FIntPoint(32, 32)), _Margin(0), _Spacing(0), _MaxHeight(320.f)
	{}
		SLATE_ATTRIBUTE(UTexture2D*, Texture)
		SLATE_ATTRIBUTE(FIntPoint, CellSize)
		SLATE_ATTRIBUTE(int32, Margin)
		SLATE_ATTRIBUTE(int32, Spacing)
		SLATE_ATTRIBUTE(TArray<int32>, HighlightRows)
		/** Pixel rectangles outlined on top (e.g. the frames of the selected animation). */
		SLATE_ATTRIBUTE(TArray<FIntRect>, HighlightRects)
		SLATE_ARGUMENT(float, MaxHeight)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SOnyxSpriteSheetView"); }

private:
	TAttribute<UTexture2D*> Texture;
	TAttribute<FIntPoint> CellSize;
	TAttribute<int32> Margin;
	TAttribute<int32> Spacing;
	TAttribute<TArray<int32>> HighlightRows;
	TAttribute<TArray<FIntRect>> HighlightRects;
	float MaxHeight = 320.f;

	mutable FSlateBrush TextureBrush;
	mutable TObjectPtr<UTexture2D> BrushTexture = nullptr;
};
