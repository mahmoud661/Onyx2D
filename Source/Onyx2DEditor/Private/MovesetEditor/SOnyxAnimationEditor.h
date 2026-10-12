// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class IDetailsView;
class ITableRow;
class SOnyxFlipbookPreview;
class FOnyxClipTimelineModel;
class SOnyxClipTimeline;
class STableViewBase;
class UBlueprint;
class UOnyxClipEvent;
class UOnyxClip;
class UOnyxMoveset;
class UTexture2D;

/**
 * The Onyx2D Animation Editor	:
 *  - left: the Moveset's clips by role; missing standard roles (Idle, Run, Jump...) are one click away;
 *  - middle: big preview with transport controls, and the timeline (frames with holds, events at exact times);
 *  - right: the clip's settings, the selected frame and the selected event (message, sound, effect...).
 * New Clip cuts frames from a sprite sheet; New Character makes a playable character from the moveset.
 */
class SOnyxAnimationEditor : public SCompoundWidget, public FSelfRegisteringEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SOnyxAnimationEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Edit a Moveset, a Clip (its moveset if one uses it) or a character Blueprint's moveset. */
	void Edit(UObject* Asset);

	/** A new animation of the current set (a new set when there is none) cut from Sheet. */
	void NewAnimationFrom(UTexture2D* Sheet);

	// FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override { OnUndoRedo(); }
	virtual void PostRedo(bool bSuccess) override { OnUndoRedo(); }

private:
	struct FEntry
	{
		FName Name;
		TWeakObjectPtr<UOnyxClip> Sequence;
	};
	using FEntryPtr = TSharedPtr<FEntry>;

	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeSidePanel();
	TSharedRef<ITableRow> MakeRow(FEntryPtr Entry, const TSharedRef<STableViewBase>& Owner);
	TSharedPtr<SWidget> MakeRowMenu();

	void SetSet(UOnyxMoveset* NewSet);
	void Select(FName Name);
	void Refresh();
	void RefreshPreview();
	UOnyxClip* GetSequence() const;
	float GetLength() const;
	EActiveTimerReturnType Tick(double CurrentTime, float DeltaTime);
	void SelectNotify(UOnyxClipEvent* Notify);
	/** The timeline's selection changed: show the frame or event in Details. */
	void SyncSelectionFromTimeline();
	void OnUndoRedo();

	void NewSet();
	void NewAnimation(FName SuggestedName);
	void OpenSlicer(UTexture2D* Sheet, FName Name);
	void AddExistingSequence();
	void RenameSelected();
	void RemoveSelected();
	void NewCharacter();
	void Save();
	FString GetFolder() const;

	TWeakObjectPtr<UOnyxMoveset> Set;
	TWeakObjectPtr<UBlueprint> Character;
	/** A sequence opened on its own (not in the set being edited). */
	TWeakObjectPtr<UOnyxClip> LooseSequence;
	TArray<FEntryPtr> Entries;
	FName SelectedName;
	int32 SelectedFrame = INDEX_NONE;
	TWeakObjectPtr<UOnyxClipEvent> SelectedNotify;

	TSharedPtr<SListView<FEntryPtr>> List;
	TSharedPtr<SOnyxFlipbookPreview> Preview;
	TSharedPtr<FOnyxClipTimelineModel> TimelineModel;
	TSharedPtr<SOnyxClipTimeline> Timeline;
	TSharedPtr<IDetailsView> NotifyDetails;
};
