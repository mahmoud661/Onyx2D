// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Builders/OnyxArtImporter.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class ITableRow;
class STableViewBase;

/**
 * Drop art here and Onyx2D handles the rest: PNG files or whole folders dragged from the file explorer (or picked with
 * Add Files / Add Folder) are sorted into tile sets, characters, sprites and backgrounds, shown for a quick check, and
 * built in one click.
 */
class SOnyxImportTool : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxImportTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Adds files / folders to the list (what a drop does). */
	void AddPaths(const TArray<FString>& Paths);

	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

private:
	using FItemPtr = TSharedPtr<FOnyxArtItem>;

	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeSettings();
	TSharedRef<SWidget> MakeEmptyHint();
	TSharedRef<ITableRow> MakeRow(FItemPtr Item, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<SWidget> MakeKindPicker(FItemPtr Item);
	TSharedRef<SWidget> MakeItemOptions(FItemPtr Item);

	void AddFiles();
	void AddFolder();
	void BuildAll();
	void Clear();
	void Refresh();
	FText GetStatus() const;

	TArray<FItemPtr> Items;
	TSharedPtr<SListView<FItemPtr>> List;
	TArray<TSharedPtr<EOnyxArtKind>> Kinds;
	FOnyxArtImportSettings Settings;
	FString LastDirectory;
	bool bDragOver = false;
};
