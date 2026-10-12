// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Widgets/SLeafWidget.h"

class FOnyxClipTimelineModel;
class UOnyxClipEvent;
class UTexture2D;

/**
 * The body of the clip timeline, under the ruler:
 *  - Frames row: a cell per frame, as wide as it is held, with its picture. Drag a cell's right edge to hold it longer,
 *    drag the cell to reorder, right-click for duplicate / delete / hold. Windows tint the frames they cover.
 *  - Lanes: events are flags (a stem and a tag with an icon and a label), windows are capsules (drag the right end to change
 *    the length). Click / Shift / Ctrl select, drag on empty space to box-select, drag to move in time and
 *    between lanes (snaps to frames; Alt = free). Right-click a lane to add an event or a window; double-click repeats the
 *    last one. Delete, Ctrl+C / Ctrl+V / Ctrl+D, Ctrl+A, Space plays, Left / Right step a frame.
 */
class SOnyxClipTimelineTracks : public SLeafWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SOnyxClipTimelineTracks) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FOnyxClipTimelineModel>& InModel);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SOnyxClipTimelineTracks"); }

private:
	enum class ERow : uint8 { None, Frames, Lane, AddLane };
	enum class EDrag : uint8 { None, PendingMove, PendingResize, PendingHold, PendingFrameMove, PendingMarquee, Move, Resize, Hold, FrameMove, Marquee, Pan };

	struct FHit
	{
		ERow Row = ERow::None;
		int32 Lane = INDEX_NONE;
		int32 KeyFrame = INDEX_NONE;
		bool bFrameEdge = false;
		TWeakObjectPtr<UOnyxClipEvent> Event;
		bool bWindowEnd = false;
	};

	FHit HitTest(const FVector2f& Local) const;
	int32 LaneAt(float Y) const;
	float LaneTop(int32 Lane) const;
	int32 GetLaneCount() const;
	/** The area an event covers (flag stem + tag, or window capsule), in local space. */
	FBox2f GetEventBounds(const UOnyxClipEvent* Event) const;
	float GetTagWidth(const UOnyxClipEvent* Event) const;
	/** Key frame whose cell is under X, or the slot a dragged frame would drop into. */
	int32 KeyFrameAtX(float X, bool* bOnEdge = nullptr) const;

	TSharedRef<SWidget> MakeLaneMenu(float Time, int32 Lane);
	TSharedRef<SWidget> MakeEventMenu(UOnyxClipEvent* Event);
	TSharedRef<SWidget> MakeFrameMenu(int32 KeyFrame);
	void FillAddMenu(class FMenuBuilder& Menu, bool bWindows, float Time, int32 Lane);
	void OpenMenu(const FPointerEvent& MouseEvent, const TSharedRef<SWidget>& Menu);
	void NewMessage(float Time, int32 Lane);

	TSharedPtr<FOnyxClipTimelineModel> Model;

	EDrag Drag = EDrag::None;
	FVector2f PressPosition = FVector2f::ZeroVector;
	FVector2f MousePosition = FVector2f::ZeroVector;
	FHit PressHit;
	int32 HoldStart = 0;
	int32 FrameDropSlot = INDEX_NONE;
	bool bRightPressed = false;
	TWeakObjectPtr<UOnyxClipEvent> Hovered;

	mutable TArray<FSlateBrush> FrameBrushes;
	mutable TArray<TObjectPtr<UTexture2D>> FrameTextures;
};
