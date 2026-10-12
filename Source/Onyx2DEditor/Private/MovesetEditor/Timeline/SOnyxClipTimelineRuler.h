// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class FOnyxClipTimelineModel;

/**
 * The timeline's ruler: frame numbers (or seconds), the end of the clip and the playhead badge. Click or drag to move the
 * playhead (it lands on frames; hold Alt for any time). Ctrl + wheel zooms, the wheel scrolls; right-click for options.
 */
class SOnyxClipTimelineRuler : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxClipTimelineRuler) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FOnyxClipTimelineModel>& InModel);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	void ScrubTo(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	TSharedRef<SWidget> MakeOptionsMenu();

	TSharedPtr<FOnyxClipTimelineModel> Model;
	bool bScrubbing = false;
};
