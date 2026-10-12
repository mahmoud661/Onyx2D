// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FOnyxClipTimelineModel;
class SInlineEditableTextBlock;
class SVerticalBox;

/**
 * The timeline's header column, row for row beside the lanes: Frames, then each lane (colour swatch, a name you can
 * double-click to rename, how many events it holds; right-click for colour, move, insert, delete) and the "+ Lane" row.
 */
class SOnyxClipLaneHeaders : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxClipLaneHeaders) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FOnyxClipTimelineModel>& InModel);
	virtual ~SOnyxClipLaneHeaders() override;

	/** Rebuilds the rows (lanes added, removed, renamed...). */
	void Rebuild();

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TSharedRef<SWidget> MakeLaneRow(int32 Lane);
	TSharedRef<SWidget> MakeLaneMenu(int32 Lane);
	TSharedRef<SWidget> MakeColorMenu(int32 Lane);
	void OpenMenu(const FPointerEvent& MouseEvent, const TSharedRef<SWidget>& Menu);

	TSharedPtr<FOnyxClipTimelineModel> Model;
	TSharedPtr<SVerticalBox> Rows;
	TArray<TSharedPtr<SInlineEditableTextBlock>> NameBlocks;
	FDelegateHandle EditedHandle;
	int32 BuiltLaneCount = INDEX_NONE;
	TWeakObjectPtr<const class UOnyxClip> BuiltClip;
};
