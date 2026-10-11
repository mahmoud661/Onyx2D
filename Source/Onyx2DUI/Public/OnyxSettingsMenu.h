// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnyxScreen.h"
#include "OnyxSettingsMenu.generated.h"

class UBackgroundBlur;
class UScrollBox;
class USizeBox;

USTRUCT()
struct FOnyxOptionRow
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UOnyxButton> Button = nullptr;

	UPROPERTY()
	TObjectPtr<UBorder> Background = nullptr;

	UPROPERTY()
	TObjectPtr<UImage> AccentBar = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> Label = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> Value = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> PrevArrow = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> NextArrow = nullptr;

	UPROPERTY()
	TObjectPtr<USizeBox> MeterFill = nullptr;

	float Focus = 0.f;
	float Pulse = 0.f;
};

/** One setting: label + a list of choices cycled with left/right. Read keeps it in sync with the source. */
struct FOnyxOption
{
	FText Label;
	FText Description;
	TArray<FText> Choices;
	int32 Index = 0;
	int32 SelectableCount = -1;
	bool bWrap = true;
	bool bMeter = false;
	bool bReadOnly = false;
	TFunction<void(int32)> Apply;
	TFunction<int32()> Read;
};

/** Display / Graphics / Audio / Controls. Changes apply immediately and are saved on close. */
UCLASS()
class ONYX2DUI_API UOnyxSettingsMenu : public UOnyxScreen
{
	GENERATED_BODY()

public:
	TFunction<void()> OnClosed;

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void Close();

protected:
	virtual void BuildUI() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void OnBackPressed() override { Close(); }
	virtual void FocusDefault() override { FocusRow(LastFocusedRow); }

private:
	enum ETab : int32 { Display, Graphics, Audio, Controls, TabCount };

	void SelectTab(int32 Tab, int32 FocusRowIndex = 0);
	void BuildOptions(int32 Tab, TArray<FOnyxOption>& Out);
	void BuildDisplayOptions(TArray<FOnyxOption>& Out);
	void BuildGraphicsOptions(TArray<FOnyxOption>& Out);
	void BuildAudioOptions(TArray<FOnyxOption>& Out);
	void BuildControlOptions(TArray<FOnyxOption>& Out) const;
	void AddRow(int32 RowIndex);
	void RefreshRow(int32 RowIndex);
	void ChangeOption(int32 RowIndex, int32 Delta);
	void FocusRow(int32 RowIndex);
	int32 GetFocusedRow() const;
	void RequestReset();

	UPROPERTY(Transient)
	TArray<FOnyxOptionRow> Rows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UOnyxButton>> TabButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> TabLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> TabUnderlines;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RowsBox;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> Scroll;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> PanelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBackgroundBlur> Blur;

	UPROPERTY(Transient)
	FOnyxFramedButton ResetButton;

	UPROPERTY(Transient)
	FOnyxFramedButton BackButton;

	TArray<FOnyxOption> Options;
	TArray<float> TabFocus;
	TArray<FIntPoint> Resolutions;
	int32 ActiveTab = 0;
	int32 LastFocusedRow = 0;
	int32 DescribedRow = INDEX_NONE;
	bool bPendingRebuild = false;
	bool bClosing = false;
};
