// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class ITableRow;
class STableViewBase;
struct FOnyxValidationIssue;

/** Level setup, project setup, actor placement and the level check. */
class SOnyxLevelTool : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxLevelTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SOnyxLevelTool() override;

	void RunValidation();

private:
	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakePlaceGrid();
	TSharedRef<ITableRow> GenerateIssueRow(TSharedPtr<FOnyxValidationIssue> Issue, const TSharedRef<STableViewBase>& Owner);
	void ApplyFix(TSharedPtr<FOnyxValidationIssue> Issue);
	void FixAll();
	FText GetStatus() const;

	TArray<TSharedPtr<FOnyxValidationIssue>> Issues;
	TSharedPtr<SListView<TSharedPtr<FOnyxValidationIssue>>> IssueList;
	FDelegateHandle MapChangedHandle;
};
