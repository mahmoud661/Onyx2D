// Copyright Onyx2D. All Rights Reserved.

#include "MovesetEditor/Timeline/SOnyxClipTimeline.h"
#include "Animation/OnyxClip.h"
#include "MovesetEditor/Timeline/OnyxClipTimelineModel.h"
#include "MovesetEditor/Timeline/OnyxClipTimelineStyle.h"
#include "MovesetEditor/Timeline/SOnyxClipLaneHeaders.h"
#include "MovesetEditor/Timeline/SOnyxClipTimelineRuler.h"
#include "MovesetEditor/Timeline/SOnyxClipTimelineTracks.h"
#include "PaperFlipbook.h"
#include "Style/OnyxEditorStyle.h"
#include "Styling/AppStyle.h"
#include "ToolsWindow/OnyxToolWidgets.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SOnyxClipTimeline::Construct(const FArguments& InArgs, const TSharedRef<FOnyxClipTimelineModel>& InModel)
{
	Model = InModel;
	TWeakPtr<FOnyxClipTimelineModel> WeakModel = Model;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(OnyxTimeline::Panel())
		.Padding(0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeTopBar()
			]
			// Ruler.
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(OnyxTimeline::HeaderWidth).HeightOverride(OnyxTimeline::RulerHeight)
					[
						SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(OnyxTimeline::RulerColor()).Padding(FMargin(12.f, 0.f)).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(OnyxTools::Font(8)).ColorAndOpacity(OnyxTimeline::MutedText())
							.Text_Lambda([WeakModel]()
							{
								TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin();
								return M && M->GetClip() ? FText::Format(INVTEXT("{0} ms at speed {1}"), FMath::RoundToInt(M->GetLength() * 1000.f),
									FText::AsNumber(M->GetClip()->PlayRate)) : INVTEXT("No clip");
							})
						]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SOnyxClipTimelineRuler, InModel)
				]
			]
			// Header column + lanes, scrolling up and down together.
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)
				+ SScrollBox::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(OnyxTimeline::HeaderWidth)
						[
							SNew(SOnyxClipLaneHeaders, InModel)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(1.f)[SNew(SImage).Image(FAppStyle::GetBrush("WhiteBrush")).ColorAndOpacity(OnyxTimeline::Divider())]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SAssignNew(Tracks, SOnyxClipTimelineTracks, InModel)
					]
				]
			]
			// Horizontal scroll for long clips.
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(OnyxTimeline::HeaderWidth + 1.f)
					[
						SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(OnyxTimeline::Header() * 0.9f).Padding(FMargin(12.f, 2.f))
						[
							SNew(STextBlock).Font(OnyxTools::Font(8)).ColorAndOpacity(OnyxTimeline::MutedText()).Text(INVTEXT("Ctrl + wheel to zoom"))
						]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SAssignNew(HorizontalScroll, SScrollBar)
					.Orientation(Orient_Horizontal)
					.Thickness(FVector2D(8.f, 8.f))
					.OnUserScrolled_Lambda([WeakModel](float Offset)
					{
						if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin())
						{
							M->ScrollTo(Offset * M->GetScrollableUnits());
						}
					})
				]
			]
		]
	];
}

void SOnyxClipTimeline::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (HorizontalScroll.IsValid())
	{
		const float Total = FMath::Max(Model->GetScrollableUnits(), 1.f);
		const float Visible = Model->GetViewWidth() / Model->GetPixelsPerUnit();
		HorizontalScroll->SetState(Model->GetScrollUnits() / Total, FMath::Min(Visible / Total, 1.f));
	}
}

TSharedRef<SWidget> SOnyxClipTimeline::MakeTransportButton(FName Icon, const FText& ToolTip, TFunction<void()> OnClick)
{
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(5.f, 4.f))
		.ToolTipText(ToolTip)
		.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
		[
			SNew(SImage).Image(FAppStyle::GetBrush(Icon)).ColorAndOpacity(FSlateColor::UseForeground())
		];
}

TSharedRef<SWidget> SOnyxClipTimeline::MakeTopBar()
{
	TWeakPtr<FOnyxClipTimelineModel> WeakModel = Model;
	auto WithModel = [WeakModel](TFunction<void(FOnyxClipTimelineModel&)> Action)
	{
		return [WeakModel, Action]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { Action(*M); } };
	};
	auto Toggle = [](FName Icon, const FText& ToolTip, TFunction<bool()> IsOn, TFunction<void()> Flip, const ISlateStyle* Style = nullptr)
	{
		return SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.Padding(FMargin(5.f, 3.f))
			.ToolTipText(ToolTip)
			.IsChecked_Lambda([IsOn]() { return IsOn() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Flip](ECheckBoxState) { Flip(); })
			[
				SNew(SImage).Image(Style ? Style->GetBrush(Icon) : FAppStyle::GetBrush(Icon)).ColorAndOpacity(FSlateColor::UseForeground())
			];
	};

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(OnyxTimeline::Bar())
		.Padding(FMargin(8.f, 3.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				MakeTransportButton("Animation.Backward_End", INVTEXT("First frame (Home)"), WithModel([](FOnyxClipTimelineModel& M) { M.JumpToStart(); }))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				MakeTransportButton("Animation.Backward_Step", INVTEXT("Previous frame (Left)"), WithModel([](FOnyxClipTimelineModel& M) { M.StepFrames(-1); }))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.ContentPadding(FMargin(8.f, 3.f))
				.ToolTipText(INVTEXT("Play / pause (Space)"))
				.OnClicked_Lambda([WeakModel]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->TogglePlay(); } return FReply::Handled(); })
				[
					SNew(SImage).ColorAndOpacity(FSlateColor::UseForeground())
					.Image_Lambda([WeakModel]() { TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); return FAppStyle::GetBrush(M && M->IsPlaying() ? "Animation.Pause" : "Animation.Forward"); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				MakeTransportButton("Animation.Forward_Step", INVTEXT("Next frame (Right)"), WithModel([](FOnyxClipTimelineModel& M) { M.StepFrames(1); }))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				MakeTransportButton("Animation.Forward_End", INVTEXT("Last frame (End)"), WithModel([](FOnyxClipTimelineModel& M) { M.JumpToEnd(); }))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f, 0.f, 0.f, 0.f)
			[
				Toggle("Animation.Loop.Enabled", INVTEXT("Loop the preview"),
					[WeakModel]() { TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); return M && M->IsLooping(); },
					WithModel([](FOnyxClipTimelineModel& M) { M.SetLooping(!M.IsLooping()); }))
			]
			// Frame and time.
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock).Font(OnyxTools::Font(10)).ColorAndOpacity(OnyxTimeline::TextColor())
				.Text_Lambda([WeakModel]()
				{
					TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin();
					if (!M || !M->GetFlipbook())
					{
						return FText::GetEmpty();
					}
					return FText::FromString(FString::Printf(TEXT("frame %d / %d  ·  %d ms"), M->GetUnitAt(M->GetTime()) + 1, M->GetUnits(), FMath::RoundToInt(M->GetTime() * 1000.f)));
				})
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNullWidget::NullWidget
			]
			// Frames per second.
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SBox).WidthOverride(56.f)
				[
					SNew(SNumericEntryBox<float>)
					.ToolTipText(INVTEXT("Frames per second (events stay on their frames)"))
					.MinValue(0.5f).MaxValue(120.f)
					.Value_Lambda([WeakModel]() { TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); return M && M->GetFlipbook() ? TOptional<float>(M->GetFps()) : TOptional<float>(); })
					.OnValueCommitted_Lambda([WeakModel](float Value, ETextCommit::Type) { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->SetFramesPerSecond(Value); } })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(STextBlock).Text(INVTEXT("fps")).Font(OnyxTools::Font(9)).ColorAndOpacity(OnyxTimeline::MutedText())
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 2.f, 0.f)
			[
				Toggle("Onyx2D.Window.LoopSound.Small", INVTEXT("Play Sound events while previewing"),
					[WeakModel]() { TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); return M && M->bPreviewSounds; },
					WithModel([](FOnyxClipTimelineModel& M) { M.bPreviewSounds = !M.bPreviewSounds; }), &FOnyxEditorStyle::Get())
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 2.f, 0.f)
			[
				Toggle("Icons.Snap", INVTEXT("Snap to frames. Off (default): place events anywhere, to the millisecond. Alt flips it while dragging."),
					[WeakModel]() { TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); return M && M->bSnapToFrames; },
					WithModel([](FOnyxClipTimelineModel& M) { M.bSnapToFrames = !M.bSnapToFrames; }))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(5.f, 4.f))
				.ToolTipText(INVTEXT("Zoom to fit the whole clip"))
				.OnClicked_Lambda([WeakModel]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->ZoomToFit(); } return FReply::Handled(); })
				[
					SNew(SImage).Image(FOnyxEditorStyle::Get().GetBrush("Onyx2D.Tool.Fit.Small")).ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		];
}
