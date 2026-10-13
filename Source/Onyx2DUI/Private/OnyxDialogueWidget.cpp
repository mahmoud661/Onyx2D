// Copyright Onyx2D. All Rights Reserved.

#include "OnyxDialogueWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Logic/OnyxDialogueSubsystem.h"
#include "OnyxUISettings.h"
#include "OnyxUIStyle.h"
#include "Rendering/SlateRenderer.h"

using namespace OnyxUI;

namespace
{
	constexpr float BoxWidth = 880.f;
	constexpr float BoxPadding = 28.f;
	constexpr float PortraitSize = 96.f;
	constexpr float PortraitGap = 22.f;
	constexpr int32 LineFontSize = 20;

	FSlateFontInfo LineFont() { return Font(EFont::Body, LineFontSize); }
}

bool UOnyxDialogueWidget::Initialize()
{
	const bool bResult = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildUI();
	}
	return bResult;
}

void UOnyxDialogueWidget::BuildUI()
{
	const FOnyxUITheme& Style = Theme();
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Root;

	// Message banner, near the top.
	UBorder* Banner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Banner->SetBrush(OutlineBrush(WithAlpha(Style.Panel, 0.88f), WithAlpha(Style.Accent, 0.45f), 1.5f, 6.f));
	Banner->SetPadding(FMargin(30.f, 12.f));
	MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	MessageText->SetFont(Font(EFont::Bold, 22, 20));
	MessageText->SetColorAndOpacity(Style.TextBright);
	MessageText->SetJustification(ETextJustify::Center);
	Banner->SetContent(MessageText);
	Banner->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Banner->SetRenderOpacity(0.f);
	MessagePanel = Banner;
	if (UOverlaySlot* BannerSlot = Root->AddChildToOverlay(Banner))
	{
		BannerSlot->SetHorizontalAlignment(HAlign_Center);
		BannerSlot->SetVerticalAlignment(VAlign_Top);
		BannerSlot->SetPadding(FMargin(0.f, 120.f, 0.f, 0.f));
	}

	// Conversation box, at the bottom: [portrait] [speaker / line], with a blinking mark when the line is done.
	USizeBox* BoxSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BoxSize->SetWidthOverride(BoxWidth);
	BoxSize->SetMinDesiredHeight(150.f);
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrush(OutlineBrush(WithAlpha(Style.Panel, 0.94f), WithAlpha(Style.Accent, 0.7f), 2.f, 8.f));
	Frame->SetPadding(FMargin(BoxPadding, 22.f, BoxPadding, 24.f));
	BoxSize->AddChild(Frame);

	UOverlay* FrameLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Frame->SetContent(FrameLayers);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	FrameLayers->AddChildToOverlay(Row);

	USizeBox* PortraitBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PortraitBox->SetWidthOverride(PortraitSize);
	PortraitBox->SetHeightOverride(PortraitSize);
	UBorder* PortraitBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PortraitBack->SetBrush(OutlineBrush(Black(0.35f), WithAlpha(Style.Accent, 0.4f), 1.5f, 6.f));
	PortraitBack->SetPadding(FMargin(6.f));
	Portrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	PortraitBack->SetContent(Portrait);
	PortraitBox->AddChild(PortraitBack);
	PortraitBox->SetVisibility(ESlateVisibility::Collapsed);
	PortraitFrame = PortraitBox;
	if (UHorizontalBoxSlot* PortraitSlot = Row->AddChildToHorizontalBox(PortraitBox))
	{
		PortraitSlot->SetVerticalAlignment(VAlign_Top);
		PortraitSlot->SetPadding(FMargin(0.f, 2.f, PortraitGap, 0.f));
	}

	UVerticalBox* Words = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UHorizontalBoxSlot* WordsSlot = Row->AddChildToHorizontalBox(Words))
	{
		WordsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	SpeakerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SpeakerText->SetFont(Font(EFont::Bold, 17, 60));
	SpeakerText->SetColorAndOpacity(Style.Accent);
	if (UVerticalBoxSlot* SpeakerSlot = Words->AddChildToVerticalBox(SpeakerText))
	{
		SpeakerSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}
	LineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LineText->SetFont(LineFont());
	LineText->SetColorAndOpacity(Style.TextBright);
	LineText->SetLineHeightPercentage(1.15f);
	Words->AddChildToVerticalBox(LineText);

	// A small diamond, bottom right, that blinks when the player can go on.
	UImage* Mark = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	FSlateBrush MarkBrush;
	MarkBrush.DrawAs = ESlateBrushDrawType::Image;
	MarkBrush.ImageSize = FVector2D(10.f, 10.f);
	Mark->SetBrush(MarkBrush);
	Mark->SetColorAndOpacity(Style.Accent);
	Mark->SetRenderTransformAngle(45.f);
	ContinueMark = Mark;
	if (UOverlaySlot* MarkSlot = FrameLayers->AddChildToOverlay(Mark))
	{
		MarkSlot->SetHorizontalAlignment(HAlign_Right);
		MarkSlot->SetVerticalAlignment(VAlign_Bottom);
		MarkSlot->SetPadding(FMargin(0.f, 0.f, -6.f, -10.f));
	}

	BoxSize->SetRenderOpacity(0.f);
	BoxSize->SetVisibility(ESlateVisibility::Collapsed);
	Box = BoxSize;
	if (UOverlaySlot* BoxSlot = Root->AddChildToOverlay(BoxSize))
	{
		BoxSlot->SetHorizontalAlignment(HAlign_Center);
		BoxSlot->SetVerticalAlignment(VAlign_Bottom);
		BoxSlot->SetPadding(FMargin(24.f, 0.f, 24.f, 48.f));
	}
}

FString UOnyxDialogueWidget::WrapLine(const FString& Line, bool bWithPortrait) const
{
	if (!FSlateApplication::IsInitialized())
	{
		return Line;
	}
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FSlateFontInfo FontInfo = LineFont();
	const float Width = BoxWidth - BoxPadding * 2.f - (bWithPortrait ? PortraitSize + PortraitGap : 0.f) - 4.f;

	// Greedy wrap that turns the breaking spaces into line breaks: same length, so letter N is still letter N.
	FString Result = Line;
	int32 LineStart = 0;
	int32 LastSpace = INDEX_NONE;
	for (int32 Index = 0; Index <= Result.Len(); ++Index)
	{
		const bool bEnd = Index == Result.Len();
		const TCHAR Char = bEnd ? TEXT(' ') : Result[Index];
		if (Char == TEXT('\n'))
		{
			LineStart = Index + 1;
			LastSpace = INDEX_NONE;
			continue;
		}
		if (Char != TEXT(' '))
		{
			continue;
		}
		const float Wide = Measure->Measure(Result.Mid(LineStart, Index - LineStart), FontInfo).X;
		if (Wide > Width && LastSpace != INDEX_NONE)
		{
			Result[LastSpace] = TEXT('\n');
			LineStart = LastSpace + 1;
		}
		LastSpace = bEnd ? INDEX_NONE : Index;
	}
	return Result;
}

void UOnyxDialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(this);
	if (!Dialogue)
	{
		return;
	}

	// Every widget is optional: a Widget Blueprint with its own layout may have only some of them.
	// Message.
	if (Dialogue->GetMessageSerial() != ShownMessageSerial)
	{
		ShownMessageSerial = Dialogue->GetMessageSerial();
		if (MessageText)
		{
			MessageText->SetText(Dialogue->GetMessage());
		}
		MessagePop = 1.12f;
	}
	MessagePop = FMath::FInterpTo(MessagePop, 1.f, InDeltaTime, 10.f);
	const float MessageOpacity = SmoothStep01(Dialogue->GetMessageOpacity());
	if (MessagePanel)
	{
		// Collapsed when faded out: render opacity doesn't fade a rounded box's outline, which would stay on screen.
		MessagePanel->SetVisibility(MessageOpacity > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		MessagePanel->SetRenderOpacity(MessageOpacity);
		MessagePanel->SetRenderScale(FVector2D(MessagePop));
	}

	// Conversation: the box rises and fades in while talking.
	const bool bTalking = Dialogue->IsTalking();
	BoxShown = FMath::FInterpConstantTo(BoxShown, bTalking ? 1.f : 0.f, InDeltaTime, 6.f);
	if (Box)
	{
		Box->SetVisibility(BoxShown > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Box->SetRenderOpacity(SmoothStep01(BoxShown));
		Box->SetRenderTranslation(FVector2D(0.f, (1.f - SmoothStep01(BoxShown)) * 24.f));
	}
	if (!bTalking)
	{
		return;
	}

	const FOnyxDialogueLine Line = Dialogue->GetLine();
	UObject* Face = Line.Portrait.Get();
	if (Face != ShownPortrait.Get())
	{
		ShownPortrait = Face;
		if (Face && Portrait)
		{
			Portrait->SetBrushResourceObject(Face);
			Portrait->SetDesiredSizeOverride(FVector2D(PortraitSize - 12.f));
		}
		if (PortraitFrame)
		{
			PortraitFrame->SetVisibility(Face ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	if (SpeakerText)
	{
		SpeakerText->SetText(FText::FromString(Line.Speaker.ToString().ToUpper()));
		SpeakerText->SetVisibility(Line.Speaker.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	const FString Full = Line.Text.ToString();
	if (Full != WrappedSource)
	{
		WrappedSource = Full;
		Wrapped = WrapLine(Full, Face != nullptr);
	}
	if (LineText)
	{
		const int32 Shown = Dialogue->GetShownText().ToString().Len();
		LineText->SetText(FText::FromString(Wrapped.Left(Shown)));
	}

	Blink += InDeltaTime;
	if (ContinueMark)
	{
		const bool bDone = Dialogue->IsLineComplete();
		ContinueMark->SetRenderOpacity(bDone ? 0.55f + 0.45f * FMath::Cos(Blink * 6.f) : 0.f);
		ContinueMark->SetRenderTranslation(FVector2D(0.f, bDone ? FMath::Sin(Blink * 6.f) * 2.f : 0.f));
	}
}
