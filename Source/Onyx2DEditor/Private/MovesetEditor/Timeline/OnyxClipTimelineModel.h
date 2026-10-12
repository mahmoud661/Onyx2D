// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"

class UOnyxClip;
class UOnyxClipEvent;
class UOnyxClipWindow;
class UOnyxMoveset;
class UPaperFlipbook;

/** How a click changes the event selection. */
enum class EOnyxSelect : uint8 { Replace, Add, Toggle };

/**
 * Everything the Moveset Editor's timeline shows and edits for one Clip: playback (time, play, loop), the horizontal view
 * (zoom and scroll, in frame units), the selection (a frame or events) and every edit, each one undoable. The timeline
 * widgets draw from it and the preview reads its time, so they always agree.
 */
class FOnyxClipTimelineModel : public TSharedFromThis<FOnyxClipTimelineModel>, public FGCObject
{
public:
	// Clip

	void SetClip(UOnyxClip* InClip);
	UOnyxClip* GetClip() const { return Clip; }
	UPaperFlipbook* GetFlipbook() const;
	UOnyxMoveset* GetMoveset() const;
	float GetFps() const;
	/** Length in frame units (a frame held for 3 counts 3). */
	int32 GetUnits() const;
	float GetLength() const;
	/** First frame unit of a key frame. */
	int32 GetKeyFrameStart(int32 KeyFrame) const;
	/** The key frame showing at Time. */
	int32 GetKeyFrameAt(float Time) const;
	/** Frame unit at Time (0-based). */
	int32 GetUnitAt(float Time) const;

	// Playback

	float GetTime() const { return Time; }
	void SetTime(float NewTime);
	bool IsPlaying() const { return bPlaying; }
	void TogglePlay();
	void Pause() { bPlaying = false; }
	bool IsLooping() const { return bLooping; }
	void SetLooping(bool bLoop) { bLooping = bLoop; }
	void StepFrames(int32 Delta);
	void JumpToStart() { SetTime(0.f); }
	void JumpToEnd();
	/** Advances playback (speed applied) and plays Sound events crossed when sound preview is on. */
	void Tick(float DeltaTime);
	bool bPreviewSounds = true;

	// View: frame units <-> pixels from the left of the lane area.

	float GetPixelsPerUnit() const { return PixelsPerUnit; }
	float GetScrollUnits() const { return ScrollUnits; }
	float UnitToX(float Units) const { return (Units - ScrollUnits) * PixelsPerUnit; }
	float XToUnit(float X) const { return X / PixelsPerUnit + ScrollUnits; }
	float TimeToX(float Seconds) const { return UnitToX(Seconds * GetFps()); }
	float XToTime(float X) const { return XToUnit(X) / GetFps(); }
	/** Zooms keeping the unit under AnchorX where it is. */
	void Zoom(float Factor, float AnchorX);
	void ScrollTo(float Units);
	/** Fits the whole clip in ViewWidth pixels. */
	void ZoomToFit();
	/** Width of the lane area, reported by the widgets as they paint. */
	void SetViewWidth(float Width);
	float GetViewWidth() const { return ViewWidth; }
	/** Units the view can scroll through (the clip plus room to drag past the end). */
	float GetScrollableUnits() const;

	// Snapping

	/** Off: events land anywhere, to the millisecond. On: they land on frame edges. Alt flips it while dragging. */
	bool bSnapToFrames = false;
	bool bShowSeconds = false;
	/** Time on a frame edge when snapping (bFlip inverts the setting), else rounded to the millisecond. */
	float Snap(float Seconds, bool bFlip) const;

	// Selection

	void SelectEvent(UOnyxClipEvent* Event, EOnyxSelect Mode);
	void SelectEvents(const TArray<UOnyxClipEvent*>& Events, EOnyxSelect Mode);
	void ClearSelection();
	void SelectAllEvents();
	bool IsSelected(const UOnyxClipEvent* Event) const;
	TArray<UOnyxClipEvent*> GetSelectedEvents() const;
	/** The event the Inspector shows (the last one clicked). */
	UOnyxClipEvent* GetFocusedEvent() const { return FocusedEvent.Get(); }
	void SelectFrame(int32 KeyFrame);
	int32 GetSelectedFrame() const { return SelectedFrame; }

	// Edits (each one is a transaction)

	UOnyxClipEvent* AddEvent(UClass* Class, float AtTime, int32 Lane);
	/** A Message event; Message None makes a new message name on the Moveset. */
	UOnyxClipEvent* AddMessage(FName Message, float AtTime, int32 Lane);
	UClass* GetLastAddedClass() const { return LastAddedClass.Get(); }
	void DeleteSelection();
	void DuplicateSelection();
	void CopySelection();
	void Paste(float AtTime, int32 AtLane);
	bool CanPaste() const;
	void MoveEventsToLane(int32 Lane);
	/** Exact placement (typed in milliseconds in the timeline menu). */
	void SetEventTime(UOnyxClipEvent* Event, float Seconds);
	void SetWindowLength(UOnyxClipWindow* Window, float Seconds);

	/** Drags: Begin once, Update as the mouse moves (offsets from where it started), End once. */
	void BeginMoveSelection();
	void UpdateMoveSelection(float DeltaSeconds, int32 DeltaLanes, bool bFlipSnap);
	void BeginResizeWindow(UOnyxClipWindow* Window);
	void UpdateResizeWindow(float EndSeconds, bool bFlipSnap);
	void EndDrag();
	bool IsDragging() const { return bDragging; }

	void InsertLane(int32 Index);
	void RemoveLane(int32 Index);
	void RenameLane(int32 Index, FName Name);
	void SetLaneColor(int32 Index, const FLinearColor& Color);
	void MoveLane(int32 From, int32 To);
	int32 GetEventCountOnLane(int32 Lane) const;

	void SetFrameHold(int32 KeyFrame, int32 Hold);
	void BeginFrameHoldDrag(int32 KeyFrame);
	void UpdateFrameHoldDrag(int32 Hold);
	void DuplicateFrame(int32 KeyFrame);
	void DeleteFrame(int32 KeyFrame);
	void MoveFrame(int32 From, int32 To);
	void SetFramesPerSecond(float Fps);

	/** Undo / redo or an outside edit changed the clip: drops stale selection, then broadcasts OnEdited. */
	void NotifyExternalChange();

	/** After any edit (timeline, preview and Inspector refresh). */
	FSimpleMulticastDelegate OnEdited;
	/** The selection changed. */
	FSimpleMulticastDelegate OnSelectionChanged;

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FOnyxClipTimelineModel"); }

private:
	void Begin(const FText& Description);
	void End();
	void ClampScroll();
	void BroadcastSelection();

	TObjectPtr<UOnyxClip> Clip;
	float Time = 0.f;
	bool bPlaying = true;
	bool bLooping = true;

	float PixelsPerUnit = 56.f;
	float ScrollUnits = 0.f;
	float ViewWidth = 600.f;
	/** Fit the clip once the lane area has its real width (a new clip was set before it was laid out). */
	bool bFitPending = true;

	TArray<TWeakObjectPtr<UOnyxClipEvent>> Selected;
	TWeakObjectPtr<UOnyxClipEvent> FocusedEvent;
	int32 SelectedFrame = INDEX_NONE;
	TWeakObjectPtr<UClass> LastAddedClass;

	bool bInTransaction = false;
	bool bDragging = false;
	/** Where the dragged events started (time, lane). */
	TArray<TPair<TWeakObjectPtr<UOnyxClipEvent>, FVector2f>> DragStart;
	TWeakObjectPtr<UOnyxClipWindow> ResizingWindow;
	int32 HoldFrame = INDEX_NONE;

	/** Copied events (transient copies), the earliest time and the first lane among them. */
	TArray<TObjectPtr<UOnyxClipEvent>> Clipboard;
	float ClipboardOrigin = 0.f;
	int32 ClipboardFirstLane = 0;
};
