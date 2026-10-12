// Copyright Onyx2D. All Rights Reserved.

#include "OnyxSettingsMenu.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/OnyxGameUserSettings.h"
#include "Input/OnyxInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/KismetSystemLibrary.h"
#include "OnyxButton.h"
#include "OnyxUISettings.h"
#include "OnyxUIStyle.h"

using namespace OnyxUI;

namespace
{
	constexpr float RowHeight = 54.f;
	constexpr float MeterWidth = 180.f;
	constexpr float ValueWidth = 250.f;
	constexpr float ReadOnlyValueWidth = 520.f;

	void SettingsFill(UOverlaySlot* OverlaySlot)
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
	}

	TArray<FText> QualityChoices()
	{
		return { INVTEXT("Low"), INVTEXT("Medium"), INVTEXT("High"), INVTEXT("Epic"), INVTEXT("Cinematic") };
	}

	TArray<FText> PercentChoices(int32 From, int32 To, int32 Step)
	{
		TArray<FText> Choices;
		for (int32 Value = From; Value <= To; Value += Step)
		{
			Choices.Add(FText::FromString(FString::Printf(TEXT("%d%%"), Value)));
		}
		return Choices;
	}

	FText WithEditorNote(const FText& Text)
	{
		return GIsEditor ? FText::Format(INVTEXT("{0} (Applies in the standalone game, not in the editor.)"), Text) : Text;
	}

	void ApplyResolution(UOnyxGameUserSettings* Settings)
	{
		if (!GIsEditor)
		{
			Settings->ApplyResolutionSettings(false);
		}
	}

	FString ShortKeyName(const FKey& Key)
	{
		static const TMap<FName, FString> PadNames = {
			{ EKeys::Gamepad_FaceButton_Bottom.GetFName(), TEXT("Pad A") },
			{ EKeys::Gamepad_FaceButton_Right.GetFName(), TEXT("Pad B") },
			{ EKeys::Gamepad_FaceButton_Left.GetFName(), TEXT("Pad X") },
			{ EKeys::Gamepad_FaceButton_Top.GetFName(), TEXT("Pad Y") },
			{ EKeys::Gamepad_LeftShoulder.GetFName(), TEXT("LB") },
			{ EKeys::Gamepad_RightShoulder.GetFName(), TEXT("RB") },
			{ EKeys::Gamepad_LeftTrigger.GetFName(), TEXT("LT") },
			{ EKeys::Gamepad_RightTrigger.GetFName(), TEXT("RT") },
			{ EKeys::Gamepad_Special_Right.GetFName(), TEXT("Start") },
			{ EKeys::Gamepad_Special_Left.GetFName(), TEXT("Back") },
			{ EKeys::Gamepad_LeftX.GetFName(), TEXT("Left Stick") },
			{ EKeys::Gamepad_LeftY.GetFName(), TEXT("Left Stick") },
			{ EKeys::Gamepad_DPad_Up.GetFName(), TEXT("D-Pad Up") },
			{ EKeys::Gamepad_DPad_Down.GetFName(), TEXT("D-Pad Down") },
			{ EKeys::Gamepad_DPad_Left.GetFName(), TEXT("D-Pad Left") },
			{ EKeys::Gamepad_DPad_Right.GetFName(), TEXT("D-Pad Right") },
		};
		if (const FString* Found = PadNames.Find(Key.GetFName()))
		{
			return *Found;
		}
		return Key.GetDisplayName(false).ToString();
	}
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void UOnyxSettingsMenu::BuildUI()
{
	const FOnyxUITheme& Style = Theme();

	UOverlay* Root = Make<UOverlay>();
	WidgetTree->RootWidget = Root;

	Blur = Make<UBackgroundBlur>();
	Blur->SetBlurStrength(0.f);
	SettingsFill(Root->AddChildToOverlay(Blur));

	UImage* Dim = MakeImage(Black(0.5f), FVector2D(32.f, 32.f));
	Dim->SetVisibility(ESlateVisibility::Visible); // swallow clicks meant for screens below
	SettingsFill(Root->AddChildToOverlay(Dim));
	UImage* Vignette = MakeTexImage(GetVignetteTexture(), Black(0.6f));
	Vignette->SetVisibility(ESlateVisibility::HitTestInvisible);
	SettingsFill(Root->AddChildToOverlay(Vignette));

	USizeBox* PanelSize = Make<USizeBox>();
	PanelSize->SetWidthOverride(1140.f);
	PanelSize->SetHeightOverride(740.f);
	PanelWidget = PanelSize;
	if (UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelSize))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
	}

	UBorder* Panel = Make<UBorder>();
	Panel->SetBrush(OutlineBrush(Style.Panel, WithAlpha(Style.Accent, 0.22f), 1.f, 6.f));
	Panel->SetPadding(FMargin(56.f, 44.f, 56.f, 34.f));
	PanelSize->AddChild(Panel);

	UVerticalBox* Column = Make<UVerticalBox>();
	Panel->SetContent(Column);
	MenuContent = Column;

	UHorizontalBox* Header = Make<UHorizontalBox>();
	Column->AddChild(Header);
	UVerticalBox* TitleBox = Make<UVerticalBox>();
	TitleBox->AddChild(MakeText(INVTEXT("OPTIONS"), Font(EFont::Light, 13, 700), WithAlpha(Style.Accent, 0.85f)));
	TitleBox->AddChild(MakeText(INVTEXT("Settings"), Font(EFont::Display, 54, 60), Style.TextBright));
	Header->AddChild(TitleBox);
	Cast<UHorizontalBoxSlot>(Header->AddChild(MakeSpacer(FVector2D(1.f, 1.f))))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	if (UHorizontalBoxSlot* HintSlot = Cast<UHorizontalBoxSlot>(Header->AddChild(MakeKeyHint(INVTEXT("Q / E"), INVTEXT("Switch tab")))))
	{
		HintSlot->SetVerticalAlignment(VAlign_Bottom);
		HintSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}
	Column->AddChild(MakeSpacer(FVector2D(1.f, 26.f)));

	UHorizontalBox* Tabs = Make<UHorizontalBox>();
	Column->AddChild(Tabs);
	const FText TabNames[TabCount] = { INVTEXT("DISPLAY"), INVTEXT("GRAPHICS"), INVTEXT("AUDIO"), INVTEXT("CONTROLS") };
	for (int32 Tab = 0; Tab < TabCount; ++Tab)
	{
		UOnyxButton* TabButton = MakeButton(false);
		TabButton->bFocusOnHover = false;
		UVerticalBox* TabColumn = Make<UVerticalBox>();
		UTextBlock* TabLabel = MakeText(TabNames[Tab], Font(EFont::Bold, 14, 320), Style.TextDim);
		Cast<UVerticalBoxSlot>(TabColumn->AddChild(TabLabel))->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		UImage* Underline = MakeImage(Style.Accent, FVector2D(10.f, 2.f));
		Cast<UVerticalBoxSlot>(TabColumn->AddChild(Underline))->SetHorizontalAlignment(HAlign_Fill);
		TabButton->AddChild(TabColumn);
		TabButton->OnClickedFn = [this, Tab]()
		{
			if (Tab != ActiveTab && !bDialogOpen)
			{
				PlayUISound(false);
				SelectTab(Tab);
			}
		};
		Cast<UHorizontalBoxSlot>(Tabs->AddChild(TabButton))->SetPadding(FMargin(0.f, 0.f, 40.f, 0.f));
		TabButtons.Add(TabButton);
		TabLabels.Add(TabLabel);
		TabUnderlines.Add(Underline);
		TabFocus.Add(0.f);
	}

	Cast<UVerticalBoxSlot>(Column->AddChild(MakeImage(WithAlpha(Style.TextDim, 0.22f), FVector2D(10.f, 1.f))))->SetHorizontalAlignment(HAlign_Fill);
	Column->AddChild(MakeSpacer(FVector2D(1.f, 12.f)));

	Scroll = Make<UScrollBox>();
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	Scroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);
	Scroll->SetAnimateWheelScrolling(true);
	Cast<UVerticalBoxSlot>(Column->AddChild(Scroll))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	RowsBox = Make<UVerticalBox>();
	Scroll->AddChild(RowsBox);

	Column->AddChild(MakeSpacer(FVector2D(1.f, 10.f)));
	Cast<UVerticalBoxSlot>(Column->AddChild(MakeImage(WithAlpha(Style.TextDim, 0.22f), FVector2D(10.f, 1.f))))->SetHorizontalAlignment(HAlign_Fill);
	Column->AddChild(MakeSpacer(FVector2D(1.f, 18.f)));

	UHorizontalBox* Footer = Make<UHorizontalBox>();
	Column->AddChild(Footer);
	DescriptionText = MakeText(FText::GetEmpty(), Font(EFont::Body, 15), Style.TextDim);
	DescriptionText->SetAutoWrapText(true);
	if (UHorizontalBoxSlot* DescSlot = Cast<UHorizontalBoxSlot>(Footer->AddChild(DescriptionText)))
	{
		DescSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		DescSlot->SetVerticalAlignment(VAlign_Center);
		DescSlot->SetPadding(FMargin(0.f, 0.f, 32.f, 0.f));
	}
	ResetButton = MakeFramedButton(INVTEXT("RESET DEFAULTS"), true, false, [this]() { RequestReset(); });
	BackButton = MakeFramedButton(INVTEXT("BACK"), false, false, [this]() { Close(); });
	Cast<UHorizontalBoxSlot>(Footer->AddChild(ResetButton.Button))->SetVerticalAlignment(VAlign_Center);
	Footer->AddChild(MakeSpacer(FVector2D(12.f, 1.f)));
	Cast<UHorizontalBoxSlot>(Footer->AddChild(BackButton.Button))->SetVerticalAlignment(VAlign_Center);

	SettingsFill(Root->AddChildToOverlay(BuildDialogLayer()));
	SelectTab(Display);
}

void UOnyxSettingsMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const FOnyxUITheme& Style = Theme();

	if (bPendingRebuild)
	{
		bPendingRebuild = false;
		SelectTab(ActiveTab, LastFocusedRow);
	}

	const float In = SmoothStep01(Age / 0.28f);
	Blur->SetBlurStrength(6.f * In);
	PanelWidget->SetRenderOpacity(In);
	PanelWidget->SetRenderScale(FVector2D(0.97f + 0.03f * In));

	for (int32 Tab = 0; Tab < TabButtons.Num(); ++Tab)
	{
		const float Target = Tab == ActiveTab ? 1.f : (TabButtons[Tab]->IsHovered() ? 0.45f : 0.f);
		TabFocus[Tab] = FMath::FInterpTo(TabFocus[Tab], Target, InDeltaTime, 14.f);
		TabLabels[Tab]->SetColorAndOpacity(FSlateColor(FMath::Lerp(Style.TextDim, Style.TextBright, TabFocus[Tab])));
		TabUnderlines[Tab]->SetRenderOpacity(Tab == ActiveTab ? TabFocus[Tab] : 0.f);
	}

	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		FOnyxOptionRow& Row = Rows[Index];
		const FOnyxOption& Option = Options[Index];
		const float Target = Row.Button->HasKeyboardFocus() ? 1.f : (Row.Button->IsHovered() ? 0.4f : 0.f);
		Row.Focus = FMath::FInterpTo(Row.Focus, Target, InDeltaTime, 14.f);
		Row.Pulse = FMath::FInterpTo(Row.Pulse, 0.f, InDeltaTime, 10.f);
		Row.Background->SetBrushColor(WithAlpha(Style.Accent, 0.075f * Row.Focus));
		Row.AccentBar->SetRenderOpacity(Row.Focus);
		Row.Label->SetColorAndOpacity(FSlateColor(FMath::Lerp(Style.Text, Style.TextBright, Row.Focus)));
		Row.Value->SetRenderScale(FVector2D(1.f + 0.07f * Row.Pulse));
		if (Row.PrevArrow && Row.NextArrow)
		{
			const FLinearColor ArrowColor = FMath::Lerp(WithAlpha(Style.TextDim, 0.55f), Style.Accent, Row.Focus);
			const bool bAtStart = !Option.bWrap && Option.Index <= 0;
			const bool bAtEnd = !Option.bWrap && Option.Index >= Option.Choices.Num() - 1;
			Row.PrevArrow->SetColorAndOpacity(FSlateColor(bAtStart ? WithAlpha(Style.TextDim, 0.2f) : ArrowColor));
			Row.NextArrow->SetColorAndOpacity(FSlateColor(bAtEnd ? WithAlpha(Style.TextDim, 0.2f) : ArrowColor));
		}
	}

	TickFramedButton(ResetButton, InDeltaTime);
	TickFramedButton(BackButton, InDeltaTime);

	const int32 Focused = GetFocusedRow();
	if (Focused != DescribedRow && Focused != INDEX_NONE)
	{
		DescribedRow = Focused;
		DescriptionText->SetText(Options[Focused].Description);
	}
	if (!bClosing && !bDialogOpen && Rows.Num() > 0 && CanReclaimFocus())
	{
		if (Focused == INDEX_NONE)
		{
			FocusRow(LastFocusedRow);
		}
		else if (Focused != LastFocusedRow)
		{
			LastFocusedRow = Focused;
			PlayUISound(false);
		}
	}
}

FReply UOnyxSettingsMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bDialogOpen && !bClosing)
	{
		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
		{
			ChangeOption(GetFocusedRow(), -1);
			return FReply::Handled();
		}
		if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
		{
			ChangeOption(GetFocusedRow(), 1);
			return FReply::Handled();
		}
		if (Key == EKeys::Q || Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::E || Key == EKeys::Gamepad_RightShoulder)
		{
			const int32 Step = (Key == EKeys::Q || Key == EKeys::Gamepad_LeftShoulder) ? -1 : 1;
			PlayUISound(false);
			SelectTab((ActiveTab + Step + TabCount) % TabCount);
			return FReply::Handled();
		}
		if (Key == EKeys::R || Key == EKeys::Gamepad_FaceButton_Top)
		{
			RequestReset();
			return FReply::Handled();
		}
		if (Key == EKeys::W || Key == EKeys::S)
		{
			FocusRow(FMath::Clamp(LastFocusedRow + (Key == EKeys::W ? -1 : 1), 0, Rows.Num() - 1));
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UOnyxSettingsMenu::Close()
{
	if (bClosing)
	{
		return;
	}
	bClosing = true;
	if (UOnyxGameUserSettings* Settings = UOnyxGameUserSettings::GetOnyxSettings())
	{
		Settings->SaveSettings();
	}
	PlayUISound(false);
	RemoveFromParent();
	if (OnClosed)
	{
		TFunction<void()> Callback = OnClosed;
		Callback();
	}
}

// ---------------------------------------------------------------------------
// Rows
// ---------------------------------------------------------------------------

void UOnyxSettingsMenu::SelectTab(int32 Tab, int32 FocusRowIndex)
{
	ActiveTab = Tab;
	Options.Reset();
	BuildOptions(Tab, Options);

	RowsBox->ClearChildren();
	Rows.Reset();
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		AddRow(Index);
	}
	Scroll->ScrollToStart();
	DescribedRow = INDEX_NONE;
	LastFocusedRow = FMath::Clamp(FocusRowIndex, 0, FMath::Max(Rows.Num() - 1, 0));
	if (GetCachedWidget().IsValid())
	{
		FocusRow(LastFocusedRow);
	}
}

void UOnyxSettingsMenu::AddRow(int32 RowIndex)
{
	const FOnyxUITheme& Style = Theme();
	const FOnyxOption& Option = Options[RowIndex];
	FOnyxOptionRow Row;

	Row.Button = MakeButton(true);
	USizeBox* Height = Make<USizeBox>();
	Height->SetHeightOverride(RowHeight);
	Row.Background = Make<UBorder>();
	Row.Background->SetBrush(SolidBrush(WithAlpha(Style.Accent, 0.f)));
	Row.Background->SetPadding(FMargin(0.f));
	Height->AddChild(Row.Background);

	UHorizontalBox* Line = Make<UHorizontalBox>();
	Row.Background->SetContent(Line);
	Row.AccentBar = MakeImage(Style.Accent, FVector2D(3.f, RowHeight));
	Cast<UHorizontalBoxSlot>(Line->AddChild(Row.AccentBar))->SetVerticalAlignment(VAlign_Fill);
	Line->AddChild(MakeSpacer(FVector2D(22.f, 1.f)));

	Row.Label = MakeText(Option.Label, Font(EFont::Body, 18), Style.Text);
	if (UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(Line->AddChild(Row.Label)))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	auto MakeArrow = [this, RowIndex, Line, &Style](const TCHAR* Glyph, int32 Delta)
	{
		UOnyxButton* Arrow = MakeButton(false);
		Arrow->bFocusOnHover = false;
		UTextBlock* ArrowText = MakeText(FText::FromString(Glyph), Font(EFont::Body, 30), Style.TextDim);
		if (UButtonSlot* ArrowSlot = Cast<UButtonSlot>(Arrow->AddChild(ArrowText)))
		{
			ArrowSlot->SetPadding(FMargin(14.f, 0.f, 14.f, 4.f));
		}
		Arrow->OnClickedFn = [this, RowIndex, Delta]() { ChangeOption(RowIndex, Delta); };
		Cast<UHorizontalBoxSlot>(Line->AddChild(Arrow))->SetVerticalAlignment(VAlign_Center);
		return ArrowText;
	};

	if (!Option.bReadOnly)
	{
		Row.PrevArrow = MakeArrow(TEXT("‹"), -1);
	}

	USizeBox* ValueBox = Make<USizeBox>();
	ValueBox->SetWidthOverride(Option.bReadOnly ? ReadOnlyValueWidth : ValueWidth);
	UVerticalBox* ValueColumn = Make<UVerticalBox>();
	Cast<USizeBoxSlot>(ValueBox->AddChild(ValueColumn))->SetVerticalAlignment(VAlign_Center);
	Row.Value = MakeText(FText::GetEmpty(), Font(Option.bReadOnly ? EFont::Body : EFont::Bold, Option.bReadOnly ? 15 : 16, 40),
		Option.bReadOnly ? Style.Accent : Style.TextBright);
	Row.Value->SetJustification(Option.bReadOnly ? ETextJustify::Right : ETextJustify::Center);
	Row.Value->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Cast<UVerticalBoxSlot>(ValueColumn->AddChild(Row.Value))->SetHorizontalAlignment(HAlign_Fill);

	if (Option.bMeter)
	{
		ValueColumn->AddChild(MakeSpacer(FVector2D(1.f, 7.f)));
		USizeBox* MeterBox = Make<USizeBox>();
		MeterBox->SetWidthOverride(MeterWidth);
		MeterBox->SetHeightOverride(2.f);
		UOverlay* Meter = Make<UOverlay>();
		MeterBox->AddChild(Meter);
		SettingsFill(Meter->AddChildToOverlay(MakeImage(WithAlpha(Style.TextDim, 0.3f), FVector2D(8.f, 2.f))));
		Row.MeterFill = Make<USizeBox>();
		Row.MeterFill->AddChild(MakeImage(Style.Accent, FVector2D(8.f, 2.f)));
		if (UOverlaySlot* FillSlot = Meter->AddChildToOverlay(Row.MeterFill))
		{
			FillSlot->SetHorizontalAlignment(HAlign_Left);
			FillSlot->SetVerticalAlignment(VAlign_Fill);
		}
		Cast<UVerticalBoxSlot>(ValueColumn->AddChild(MeterBox))->SetHorizontalAlignment(HAlign_Center);
	}
	Cast<UHorizontalBoxSlot>(Line->AddChild(ValueBox))->SetVerticalAlignment(VAlign_Center);

	if (!Option.bReadOnly)
	{
		Row.NextArrow = MakeArrow(TEXT("›"), 1);
	}
	Line->AddChild(MakeSpacer(FVector2D(Option.bReadOnly ? 26.f : 14.f, 1.f)));

	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Row.Button->AddChild(Height)))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
		ContentSlot->SetPadding(FMargin(0.f));
	}
	Row.Button->OnClickedFn = [this, RowIndex]() { ChangeOption(RowIndex, 1); };
	if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(RowsBox->AddChild(Row.Button)))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
		RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	Rows.Add(Row);
	RefreshRow(RowIndex);
}

void UOnyxSettingsMenu::RefreshRow(int32 RowIndex)
{
	if (!Rows.IsValidIndex(RowIndex) || !Options.IsValidIndex(RowIndex))
	{
		return;
	}
	const FOnyxOption& Option = Options[RowIndex];
	FOnyxOptionRow& Row = Rows[RowIndex];
	Row.Value->SetText(Option.Choices.IsValidIndex(Option.Index) ? Option.Choices[Option.Index] : FText::GetEmpty());
	if (Row.MeterFill)
	{
		const int32 Count = Option.Choices.Num();
		const float Fraction = Count > 1 ? float(Option.Index) / float(Count - 1) : 1.f;
		Row.MeterFill->SetWidthOverride(FMath::Max(MeterWidth * Fraction, 0.5f));
	}
}

void UOnyxSettingsMenu::ChangeOption(int32 RowIndex, int32 Delta)
{
	if (!Options.IsValidIndex(RowIndex) || bDialogOpen)
	{
		return;
	}
	FOnyxOption& Option = Options[RowIndex];
	const int32 Count = Option.SelectableCount > 0 ? FMath::Min(Option.SelectableCount, Option.Choices.Num()) : Option.Choices.Num();
	if (Option.bReadOnly || Count < 2)
	{
		return;
	}

	int32 NewIndex;
	if (Option.Index >= Count)
	{
		NewIndex = Delta > 0 ? 0 : Count - 1; // leaving a non-selectable value such as "Custom"
	}
	else if (Option.bWrap)
	{
		NewIndex = (Option.Index + Delta + Count) % Count;
	}
	else
	{
		NewIndex = FMath::Clamp(Option.Index + Delta, 0, Count - 1);
	}
	if (NewIndex == Option.Index)
	{
		return;
	}

	FocusRow(RowIndex);
	Option.Index = NewIndex;
	Rows[RowIndex].Pulse = 1.f;
	RefreshRow(RowIndex);
	PlayUISound(false);
	if (Option.Apply)
	{
		Option.Apply(NewIndex);
	}

	for (int32 Other = 0; Other < Options.Num(); ++Other)
	{
		if (Options[Other].Read)
		{
			const int32 Current = Options[Other].Read();
			if (Current != Options[Other].Index)
			{
				Options[Other].Index = Current;
				RefreshRow(Other);
			}
		}
	}
}

void UOnyxSettingsMenu::FocusRow(int32 RowIndex)
{
	if (Rows.IsValidIndex(RowIndex) && Rows[RowIndex].Button)
	{
		LastFocusedRow = RowIndex;
		Rows[RowIndex].Button->SetKeyboardFocus();
	}
}

int32 UOnyxSettingsMenu::GetFocusedRow() const
{
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		if (Rows[Index].Button && Rows[Index].Button->HasKeyboardFocus())
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UOnyxSettingsMenu::RequestReset()
{
	ShowConfirm(INVTEXT("Reset Settings?"), INVTEXT("Display, graphics and audio settings will return to their default values."), INVTEXT("RESET"), [this]()
	{
		if (UOnyxGameUserSettings* Settings = UOnyxGameUserSettings::GetOnyxSettings())
		{
			Settings->SetToDefaults();
			GIsEditor ? Settings->ApplyNonResolutionSettings() : Settings->ApplySettings(false);
		}
		bPendingRebuild = true;
	});
}

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

void UOnyxSettingsMenu::BuildOptions(int32 Tab, TArray<FOnyxOption>& Out)
{
	if (Tab == Controls)
	{
		BuildControlOptions(Out);
		return;
	}
	if (!UOnyxGameUserSettings::GetOnyxSettings())
	{
		FOnyxOption Missing;
		Missing.Label = INVTEXT("Settings unavailable");
		Missing.Description = INVTEXT("Set GameUserSettingsClassName=/Script/Onyx2DRuntime.OnyxGameUserSettings in DefaultEngine.ini.");
		Missing.bReadOnly = true;
		Out.Add(Missing);
		return;
	}
	switch (Tab)
	{
	case Display:  BuildDisplayOptions(Out); break;
	case Graphics: BuildGraphicsOptions(Out); break;
	case Audio:    BuildAudioOptions(Out); break;
	default: break;
	}
	for (FOnyxOption& Option : Out)
	{
		if (Option.Read)
		{
			Option.Index = Option.Read();
		}
	}
}

void UOnyxSettingsMenu::BuildDisplayOptions(TArray<FOnyxOption>& Out)
{
	UOnyxGameUserSettings* Settings = UOnyxGameUserSettings::GetOnyxSettings();

	FOnyxOption& WindowMode = Out.AddDefaulted_GetRef();
	WindowMode.Label = INVTEXT("Window Mode");
	WindowMode.Description = WithEditorNote(INVTEXT("Fullscreen gives the best performance. Borderless lets you switch windows instantly."));
	WindowMode.Choices = { INVTEXT("Fullscreen"), INVTEXT("Borderless"), INVTEXT("Windowed") };
	WindowMode.Read = [Settings]()
	{
		switch (Settings->GetFullscreenMode())
		{
		case EWindowMode::Fullscreen: return 0;
		case EWindowMode::WindowedFullscreen: return 1;
		default: return 2;
		}
	};
	WindowMode.Apply = [Settings](int32 Index)
	{
		static const EWindowMode::Type Modes[] = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };
		Settings->SetFullscreenMode(Modes[Index]);
		ApplyResolution(Settings);
	};

	Resolutions.Reset();
	TArray<FIntPoint> Supported;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);
	for (const FIntPoint& Resolution : Supported)
	{
		if (Resolution.X >= 1024 && Resolution.Y >= 720)
		{
			Resolutions.AddUnique(Resolution);
		}
	}
	if (Settings->GetScreenResolution().X > 0)
	{
		Resolutions.AddUnique(Settings->GetScreenResolution());
	}
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });

	FOnyxOption& Resolution = Out.AddDefaulted_GetRef();
	Resolution.Label = INVTEXT("Resolution");
	Resolution.Description = WithEditorNote(INVTEXT("Rendering resolution of the game window."));
	for (const FIntPoint& Size : Resolutions)
	{
		Resolution.Choices.Add(FText::FromString(FString::Printf(TEXT("%d × %d"), Size.X, Size.Y)));
	}
	Resolution.bWrap = false;
	Resolution.Read = [this, Settings]()
	{
		const int32 Found = Resolutions.IndexOfByKey(Settings->GetScreenResolution());
		return Found == INDEX_NONE ? 0 : Found;
	};
	Resolution.Apply = [this, Settings](int32 Index)
	{
		if (Resolutions.IsValidIndex(Index))
		{
			Settings->SetScreenResolution(Resolutions[Index]);
			ApplyResolution(Settings);
		}
	};

	FOnyxOption& VSync = Out.AddDefaulted_GetRef();
	VSync.Label = INVTEXT("V-Sync");
	VSync.Description = INVTEXT("Synchronises frames with your monitor to remove tearing. May add slight input delay.");
	VSync.Choices = { INVTEXT("Off"), INVTEXT("On") };
	VSync.Read = [Settings]() { return Settings->IsVSyncEnabled() ? 1 : 0; };
	VSync.Apply = [Settings](int32 Index) { Settings->SetVSyncEnabled(Index == 1); Settings->ApplyNonResolutionSettings(); };

	static const int32 Limits[] = { 30, 60, 90, 120, 144, 165, 240, 0 };
	FOnyxOption& FrameLimit = Out.AddDefaulted_GetRef();
	FrameLimit.Label = INVTEXT("Frame Rate Limit");
	FrameLimit.Description = INVTEXT("Caps the frame rate to reduce heat, fan noise and power use.");
	for (const int32 Limit : Limits)
	{
		FrameLimit.Choices.Add(Limit > 0 ? FText::FromString(FString::Printf(TEXT("%d FPS"), Limit)) : INVTEXT("Unlimited"));
	}
	FrameLimit.bWrap = false;
	FrameLimit.Read = [Settings]()
	{
		const float Current = Settings->GetFrameRateLimit();
		if (Current <= 0.f)
		{
			return int32(UE_ARRAY_COUNT(Limits)) - 1;
		}
		int32 Best = 0;
		for (int32 Index = 0; Index < int32(UE_ARRAY_COUNT(Limits)) - 1; ++Index)
		{
			if (FMath::Abs(Limits[Index] - Current) < FMath::Abs(Limits[Best] - Current))
			{
				Best = Index;
			}
		}
		return Best;
	};
	FrameLimit.Apply = [Settings](int32 Index) { Settings->SetFrameRateLimit(float(Limits[Index])); Settings->ApplyNonResolutionSettings(); };

	FOnyxOption& Brightness = Out.AddDefaulted_GetRef();
	Brightness.Label = INVTEXT("Brightness");
	Brightness.Description = INVTEXT("Adjusts display gamma. Raise it if dark areas are hard to read.");
	Brightness.Choices = PercentChoices(50, 150, 5);
	Brightness.bWrap = false;
	Brightness.bMeter = true;
	Brightness.Read = [Settings]() { return FMath::Clamp(FMath::RoundToInt((Settings->GetBrightness() * 100.f - 50.f) / 5.f), 0, 20); };
	Brightness.Apply = [Settings](int32 Index) { Settings->SetBrightness((50.f + 5.f * Index) / 100.f); Settings->ApplyCustomSettings(); };
}

void UOnyxSettingsMenu::BuildGraphicsOptions(TArray<FOnyxOption>& Out)
{
	UOnyxGameUserSettings* Settings = UOnyxGameUserSettings::GetOnyxSettings();

	FOnyxOption& Preset = Out.AddDefaulted_GetRef();
	Preset.Label = INVTEXT("Quality Preset");
	Preset.Description = INVTEXT("Sets every graphics option at once. Fine-tune them individually below.");
	Preset.Choices = QualityChoices();
	Preset.Choices.Add(INVTEXT("Custom"));
	Preset.SelectableCount = 5;
	Preset.Read = [Settings]()
	{
		const int32 Level = Settings->GetOverallScalabilityLevel();
		return Level < 0 ? 5 : FMath::Clamp(Level, 0, 4);
	};
	Preset.Apply = [Settings](int32 Index)
	{
		if (Index < 5)
		{
			Settings->SetOverallScalabilityLevel(Index);
			Settings->ApplyNonResolutionSettings();
		}
	};

	FOnyxOption& RenderScale = Out.AddDefaulted_GetRef();
	RenderScale.Label = INVTEXT("Render Scale");
	RenderScale.Description = INVTEXT("Renders at a lower resolution and upscales. Lower runs faster. Auto lets the engine decide.");
	RenderScale.Choices = { INVTEXT("Auto") };
	RenderScale.Choices.Append(PercentChoices(50, 100, 5));
	RenderScale.bWrap = false;
	RenderScale.Read = [Settings]()
	{
		float Normalized = 0.f, Current = 0.f, Min = 0.f, Max = 100.f;
		Settings->GetResolutionScaleInformationEx(Normalized, Current, Min, Max);
		return Current <= 0.f ? 0 : 1 + FMath::Clamp(FMath::RoundToInt((Current - 50.f) / 5.f), 0, 10);
	};
	RenderScale.Apply = [Settings](int32 Index)
	{
		Settings->SetResolutionScaleValueEx(Index == 0 ? 0.f : 50.f + 5.f * (Index - 1));
		Settings->ApplyNonResolutionSettings();
	};

	struct FQualitySetting
	{
		FText Label;
		FText Description;
		int32 (UGameUserSettings::*Getter)() const;
		void (UGameUserSettings::*Setter)(int32);
	};
	const FQualitySetting QualitySettings[] = {
		{ INVTEXT("Anti-Aliasing"), INVTEXT("Smooths jagged edges. Pixel art usually looks best on Low."), &UGameUserSettings::GetAntiAliasingQuality, &UGameUserSettings::SetAntiAliasingQuality },
		{ INVTEXT("Shadows"), INVTEXT("Shadow resolution and softness from 2D lights."), &UGameUserSettings::GetShadowQuality, &UGameUserSettings::SetShadowQuality },
		{ INVTEXT("Effects"), INVTEXT("Detail of particles and visual effects."), &UGameUserSettings::GetVisualEffectQuality, &UGameUserSettings::SetVisualEffectQuality },
		{ INVTEXT("Post Processing"), INVTEXT("Bloom, glow and color grading quality."), &UGameUserSettings::GetPostProcessingQuality, &UGameUserSettings::SetPostProcessingQuality },
		{ INVTEXT("Textures"), INVTEXT("Texture quality. Lower it if you run out of video memory."), &UGameUserSettings::GetTextureQuality, &UGameUserSettings::SetTextureQuality },
	};
	for (const FQualitySetting& Quality : QualitySettings)
	{
		FOnyxOption& Option = Out.AddDefaulted_GetRef();
		Option.Label = Quality.Label;
		Option.Description = Quality.Description;
		Option.Choices = QualityChoices();
		auto Getter = Quality.Getter;
		auto Setter = Quality.Setter;
		Option.Read = [Settings, Getter]() { return FMath::Clamp((Settings->*Getter)(), 0, 4); };
		Option.Apply = [Settings, Setter](int32 Index) { (Settings->*Setter)(Index); Settings->ApplyNonResolutionSettings(); };
	}
}

void UOnyxSettingsMenu::BuildAudioOptions(TArray<FOnyxOption>& Out)
{
	UOnyxGameUserSettings* Settings = UOnyxGameUserSettings::GetOnyxSettings();

	FOnyxOption& Volume = Out.AddDefaulted_GetRef();
	Volume.Label = INVTEXT("Master Volume");
	Volume.Description = INVTEXT("Overall volume of music, ambience and sound effects.");
	Volume.Choices = PercentChoices(0, 100, 5);
	Volume.bWrap = false;
	Volume.bMeter = true;
	Volume.Read = [Settings]() { return FMath::Clamp(FMath::RoundToInt(Settings->GetMasterVolume() * 20.f), 0, 20); };
	Volume.Apply = [Settings](int32 Index) { Settings->SetMasterVolume(Index * 0.05f); Settings->ApplyCustomSettings(); };

	// Music / effects / UI on top of Master.
	struct FCategory
	{
		FText Label;
		FText Description;
		float (UOnyxGameUserSettings::*Getter)() const;
		void (UOnyxGameUserSettings::*Setter)(float);
	};
	const FCategory Categories[] = {
		{ INVTEXT("Music"), INVTEXT("Level and zone music."), &UOnyxGameUserSettings::GetMusicVolume, &UOnyxGameUserSettings::SetMusicVolume },
		{ INVTEXT("Sound Effects"), INVTEXT("Footsteps, hits, pickups and ambience."), &UOnyxGameUserSettings::GetEffectsVolume, &UOnyxGameUserSettings::SetEffectsVolume },
		{ INVTEXT("Interface"), INVTEXT("Menu and HUD sounds."), &UOnyxGameUserSettings::GetUIVolume, &UOnyxGameUserSettings::SetUIVolume },
	};
	for (const FCategory& Category : Categories)
	{
		FOnyxOption& Option = Out.AddDefaulted_GetRef();
		Option.Label = Category.Label;
		Option.Description = Category.Description;
		Option.Choices = PercentChoices(0, 100, 5);
		Option.bWrap = false;
		Option.bMeter = true;
		auto Getter = Category.Getter;
		auto Setter = Category.Setter;
		Option.Read = [Settings, Getter]() { return FMath::Clamp(FMath::RoundToInt((Settings->*Getter)() * 20.f), 0, 20); };
		Option.Apply = [Settings, Setter](int32 Index) { (Settings->*Setter)(Index * 0.05f); Settings->ApplyCustomSettings(); };
	}

	FOnyxOption& Mute = Out.AddDefaulted_GetRef();
	Mute.Label = INVTEXT("Mute When Unfocused");
	Mute.Description = INVTEXT("Silences the game while another window is in front.");
	Mute.Choices = { INVTEXT("Off"), INVTEXT("On") };
	Mute.Read = [Settings]() { return Settings->GetMuteWhenUnfocused() ? 1 : 0; };
	Mute.Apply = [Settings](int32 Index) { Settings->SetMuteWhenUnfocused(Index == 1); Settings->ApplyCustomSettings(); };
}

void UOnyxSettingsMenu::BuildControlOptions(TArray<FOnyxOption>& Out) const
{
	const UOnyxInputSubsystem* InputSubsystem = UOnyxInputSubsystem::Get(GetOwningPlayer());
	const UOnyxInputConfig* Config = InputSubsystem ? InputSubsystem->GetInputConfig() : nullptr;
	if (!Config || !Config->MappingContext)
	{
		return;
	}

	struct FActionKeys
	{
		const UInputAction* Action = nullptr;
		TArray<FString> Keyboard;
		TArray<FString> Gamepad;
	};
	TArray<FActionKeys> Actions;
	for (const FEnhancedActionKeyMapping& Mapping : Config->MappingContext->GetMappings())
	{
		if (!Mapping.Action || !Mapping.Key.IsValid() || Mapping.Key.IsDeprecated())
		{
			continue;
		}
		FActionKeys* Entry = Actions.FindByPredicate([&Mapping](const FActionKeys& Item) { return Item.Action == Mapping.Action; });
		if (!Entry)
		{
			Entry = &Actions.AddDefaulted_GetRef();
			Entry->Action = Mapping.Action;
		}
		(Mapping.Key.IsGamepadKey() ? Entry->Gamepad : Entry->Keyboard).AddUnique(ShortKeyName(Mapping.Key));
	}

	for (const FActionKeys& Item : Actions)
	{
		FOnyxOption& Option = Out.AddDefaulted_GetRef();
		Option.Label = Item.Action->ActionDescription.IsEmpty() ? FText::FromString(Item.Action->GetName()) : Item.Action->ActionDescription;
		FString Value = FString::Join(Item.Keyboard, TEXT("  /  "));
		if (Item.Gamepad.Num() > 0)
		{
			Value += (Value.IsEmpty() ? TEXT("") : TEXT("     ·     ")) + FString::Join(Item.Gamepad, TEXT("  /  "));
		}
		Option.Choices = { FText::FromString(Value) };
		Option.Description = INVTEXT("Current key bindings for keyboard, mouse and gamepad.");
		Option.bReadOnly = true;
	}
}
