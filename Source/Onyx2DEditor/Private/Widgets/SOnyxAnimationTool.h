// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScopedTransaction.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class ITableRow;
class SOnyxFlipbookPreview;
class STableViewBase;
class UBlueprint;
class UOnyxAnimatorComponent;
struct FAssetData;
struct FOnyxAnimation;
struct FOnyxFrameEvent;

/** Edits a Onyx character's animation table and frame events with a live preview. */
class SOnyxAnimationTool : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxAnimationTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetBlueprint(UBlueprint* InBlueprint);

	// Used by the table rows.
	UOnyxAnimatorComponent* GetAnimator() const;
	FOnyxAnimation* FindAnimation(FName Name) const;
	FOnyxFrameEvent* FindEvent(int32 Index) const;
	void BeginEdit(const FText& Description);
	void EndEdit();
	bool RenameAnimation(FName OldName, FName NewName);
	void RemoveEvent(int32 Index);
	void PreviewFrame(int32 Frame);
	void RefreshPreview();

private:
	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeCharacterPicker();
	TSharedRef<SWidget> MakePreviewPanel();
	TSharedRef<ITableRow> GenerateAnimationRow(TSharedPtr<FName> Item, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<ITableRow> GenerateEventRow(TSharedPtr<int32> Item, const TSharedRef<STableViewBase>& Owner);

	void RefreshLists();
	void HandleSelectionChanged(TSharedPtr<FName> Item, ESelectInfo::Type SelectInfo);
	void AddAnimation();
	void RemoveSelectedAnimation();
	void AddEvent();
	void Save();
	void OpenBlueprintEditor();
	FText GetStatus() const;

	TWeakObjectPtr<UBlueprint> Blueprint;
	FName Selected;
	TArray<TSharedPtr<FName>> AnimationItems;
	TArray<TSharedPtr<int32>> EventItems;
	TSharedPtr<SListView<TSharedPtr<FName>>> AnimationList;
	TSharedPtr<SListView<TSharedPtr<int32>>> EventList;
	TSharedPtr<SOnyxFlipbookPreview> Preview;
	TUniquePtr<FScopedTransaction> Transaction;
};
