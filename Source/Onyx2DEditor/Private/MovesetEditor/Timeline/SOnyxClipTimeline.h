// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FOnyxClipTimelineModel;
class SOnyxClipLaneHeaders;
class SOnyxClipTimelineTracks;
class SScrollBar;

/**
 * The Moveset Editor's timeline for one Clip:
 *  - top bar: start, step back, play / pause, step, end, loop; the frame and time; frames per second; sound preview,
 *    snapping and zoom to fit;
 *  - the ruler with the playhead;
 *  - the header column (Frames, the lanes, + Lane) beside the frames row and the event lanes;
 *  - a scroll bar for long clips (Ctrl + wheel zooms).
 * All state lives in the shared FOnyxClipTimelineModel, which the preview reads too.
 */
class SOnyxClipTimeline : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxClipTimeline) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FOnyxClipTimelineModel>& InModel);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TSharedRef<SWidget> MakeTopBar();
	TSharedRef<SWidget> MakeTransportButton(FName Icon, const FText& ToolTip, TFunction<void()> OnClick);

	TSharedPtr<FOnyxClipTimelineModel> Model;
	TSharedPtr<SScrollBar> HorizontalScroll;
	TSharedPtr<SOnyxClipTimelineTracks> Tracks;
};
