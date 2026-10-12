// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FOnOnyxOpenTool, FName /*TabId*/);

/**
 * The start page: the steps from art to a playable level (each one a button), the example level, and how to extend
 * Onyx2D with your own Blueprints or C++ classes (where the code goes, one-click creation from every Onyx base class).
 */
class SOnyxHomeTool : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxHomeTool) {}
		SLATE_EVENT(FOnOnyxOpenTool, OnOpenTool)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeHeader();
	TSharedRef<SWidget> MakeSteps();
	TSharedRef<SWidget> MakeStep(int32 Number, const FText& Title, const FText& Description, const TSharedRef<SWidget>& Actions);
	TSharedRef<SWidget> MakeExtend();
	TSharedRef<SWidget> MakeBaseClassRow(UClass* Class, const FText& Label, const FText& Description);
	TSharedRef<SWidget> Button(const FText& Label, const FText& Tooltip, TFunction<void()> OnClicked, bool bPrimary = false);

	void OpenTool(FName TabId) const;
	/** Re-checks which steps the project and the open level have done. */
	EActiveTimerReturnType UpdateProgress(double CurrentTime, float DeltaTime);
	bool IsDone(int32 Step) const { return Done.IsValidIndex(Step) && Done[Step]; }
	/** First step not done yet (0 = all done). */
	int32 GetNextStep() const;
	void CreateBlueprint(UClass* ParentClass) const;
	void CreateCppClass(UClass* ParentClass) const;

	FOnOnyxOpenTool OnOpenToolDelegate;
	/** Per step (index = step number): done in this project / level. */
	TArray<bool> Done;
};
