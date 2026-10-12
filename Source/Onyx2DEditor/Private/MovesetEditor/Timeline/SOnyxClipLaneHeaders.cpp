// Copyright Onyx2D. All Rights Reserved.

#include "MovesetEditor/Timeline/SOnyxClipLaneHeaders.h"
#include "Animation/OnyxClip.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MovesetEditor/Timeline/OnyxClipTimelineModel.h"
#include "MovesetEditor/Timeline/OnyxClipTimelineStyle.h"
#include "PaperFlipbook.h"
#include "Style/OnyxEditorStyle.h"
#include "Styling/AppStyle.h"
#include "ToolsWindow/OnyxToolWidgets.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"

void SOnyxClipLaneHeaders::Construct(const FArguments& InArgs, const TSharedRef<FOnyxClipTimelineModel>& InModel)
{
	Model = InModel;
	EditedHandle = Model->OnEdited.AddSP(this, &SOnyxClipLaneHeaders::Rebuild);
	ChildSlot
	[
		SAssignNew(Rows, SVerticalBox)
	];
	Rebuild();
}

SOnyxClipLaneHeaders::~SOnyxClipLaneHeaders()
{
	if (Model.IsValid())
	{
		Model->OnEdited.Remove(EditedHandle);
	}
}

void SOnyxClipLaneHeaders::OpenMenu(const FPointerEvent& MouseEvent, const TSharedRef<SWidget>& Menu)
{
	const FWidgetPath Path = MouseEvent.GetEventPath() ? *MouseEvent.GetEventPath() : FWidgetPath();
	FSlateApplication::Get().PushMenu(AsShared(), Path, Menu, MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

void SOnyxClipLaneHeaders::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// A different clip (or lanes changed by undo): rebuild.
	const UOnyxClip* Clip = Model->GetClip();
	if (BuiltClip.Get() != Clip || BuiltLaneCount != (Clip ? Clip->Lanes.Num() : 0))
	{
		Rebuild();
	}
}

void SOnyxClipLaneHeaders::Rebuild()
{
	Rows->ClearChildren();
	NameBlocks.Reset();
	const UOnyxClip* Clip = Model->GetClip();
	BuiltClip = Clip;
	BuiltLaneCount = Clip ? Clip->Lanes.Num() : 0;

	// Frames.
	Rows->AddSlot().AutoHeight()
	[
		SNew(SBox).HeightOverride(OnyxTimeline::FramesHeight)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(OnyxTimeline::Header()).Padding(FMargin(12.f, 0.f)).VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SImage).Image(FOnyxEditorStyle::Get().GetBrush("Onyx2D.Timeline.Frames.Small"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(INVTEXT("Frames")).Font(OnyxTools::Font(9, true)).ColorAndOpacity(OnyxTimeline::TextColor())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(OnyxTools::Font(8)).ColorAndOpacity(OnyxTimeline::MutedText())
					.Text_Lambda([this]()
					{
						const UPaperFlipbook* Flipbook = Model->GetFlipbook();
						return Flipbook ? FText::Format(INVTEXT("{0} · {1} fps"), Flipbook->GetNumKeyFrames(), FText::AsNumber(FMath::RoundToInt(Flipbook->GetFramesPerSecond()))) : FText::GetEmpty();
					})
				]
			]
		]
	];

	for (int32 Lane = 0; Clip && Lane < Clip->Lanes.Num(); ++Lane)
	{
		Rows->AddSlot().AutoHeight()[MakeLaneRow(Lane)];
	}

	// + Lane.
	TWeakPtr<FOnyxClipTimelineModel> WeakModel = Model;
	Rows->AddSlot().AutoHeight()
	[
		SNew(SBox).HeightOverride(OnyxTimeline::AddLaneHeight)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(OnyxTimeline::Header() * 0.9f).Padding(FMargin(6.f, 0.f)).VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(INVTEXT("Add a lane (up to 8). Lanes only organise events; every lane plays."))
				.IsEnabled_Lambda([WeakModel]() { TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); return M && M->GetClip() && M->GetClip()->Lanes.Num() < UOnyxClip::MaxLanes; })
				.OnClicked_Lambda([WeakModel]()
				{
					if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); M && M->GetClip())
					{
						M->InsertLane(M->GetClip()->Lanes.Num());
					}
					return FReply::Handled();
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(SImage).Image(FAppStyle::GetBrush("Icons.Plus")).ColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(0x7A, 0xA7, 0xFF))))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(INVTEXT("Lane")).Font(OnyxTools::Font(9)).ColorAndOpacity(FLinearColor::FromSRGBColor(FColor(0x7A, 0xA7, 0xFF)))
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SOnyxClipLaneHeaders::MakeLaneRow(int32 Lane)
{
	TWeakPtr<FOnyxClipTimelineModel> WeakModel = Model;
	auto LaneColor = [WeakModel, Lane]()
	{
		TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin();
		const UOnyxClip* Clip = M ? M->GetClip() : nullptr;
		return Clip && Clip->Lanes.IsValidIndex(Lane) ? Clip->Lanes[Lane].Color : FLinearColor::Gray;
	};
	TSharedPtr<SInlineEditableTextBlock> NameBlock;
	TSharedRef<SBorder> Row = SNew(SBorder)
		.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent& MouseEvent)
		{
			return MouseEvent.GetEffectingButton() == EKeys::RightMouseButton ? FReply::Handled() : FReply::Unhandled();
		})
		.OnMouseButtonUp_Lambda([this, Lane](const FGeometry&, const FPointerEvent& MouseEvent)
		{
			if (MouseEvent.GetEffectingButton() != EKeys::RightMouseButton)
			{
				return FReply::Unhandled();
			}
			OpenMenu(MouseEvent, MakeLaneMenu(Lane));
			return FReply::Handled();
		})
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor_Lambda([LaneColor, Lane]() { return FSlateColor(OnyxTimeline::LaneRow(LaneColor(), Lane) * 1.18f); })
		.Padding(FMargin(8.f, 0.f, 10.f, 0.f))
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			// Colour swatch: click to pick another colour.
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(3.f))
				.ToolTipText(INVTEXT("Lane colour"))
				.OnClicked_Lambda([this, Lane]()
				{
					FSlateApplication::Get().PushMenu(AsShared(), FWidgetPath(), MakeColorMenu(Lane), FSlateApplication::Get().GetCursorPos(),
						FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
					return FReply::Handled();
				})
				[
					SNew(SBox).WidthOverride(11.f).HeightOverride(11.f)
					[
						SNew(SImage).Image(OnyxTimeline::Rounded(3.f)).ColorAndOpacity_Lambda([LaneColor]() { return FSlateColor(LaneColor()); })
					]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SAssignNew(NameBlock, SInlineEditableTextBlock)
				.Font(OnyxTools::Font(9))
				.ColorAndOpacity(OnyxTimeline::TextColor())
				.ToolTipText(INVTEXT("Double-click to rename"))
				.Text_Lambda([WeakModel, Lane]()
				{
					TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin();
					const UOnyxClip* Clip = M ? M->GetClip() : nullptr;
					return Clip && Clip->Lanes.IsValidIndex(Lane) ? FText::FromName(Clip->Lanes[Lane].Name) : FText::GetEmpty();
				})
				.OnTextCommitted_Lambda([WeakModel, Lane](const FText& Text, ETextCommit::Type)
				{
					const FString Name = Text.ToString().TrimStartAndEnd();
					if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); M && !Name.IsEmpty())
					{
						M->RenameLane(Lane, FName(*Name));
					}
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(OnyxTools::Font(8)).ColorAndOpacity(OnyxTimeline::MutedText())
				.Text_Lambda([WeakModel, Lane]()
				{
					TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin();
					const int32 Count = M ? M->GetEventCountOnLane(Lane) : 0;
					return Count > 0 ? FText::AsNumber(Count) : FText::GetEmpty();
				})
			]
		];
	NameBlocks.Add(NameBlock);
	return SNew(SBox).HeightOverride(OnyxTimeline::LaneHeight)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(1.f)[SNew(SImage).Image(FAppStyle::GetBrush("WhiteBrush")).ColorAndOpacity(OnyxTimeline::Divider())]]
		+ SVerticalBox::Slot().FillHeight(1.f)[Row]
	];
}

TSharedRef<SWidget> SOnyxClipLaneHeaders::MakeColorMenu(int32 Lane)
{
	TWeakPtr<FOnyxClipTimelineModel> WeakModel = Model;
	FMenuBuilder Menu(true, nullptr);
	Menu.BeginSection(NAME_None, INVTEXT("Lane Colour"));
	for (const OnyxTimeline::FNamedColor& Named : OnyxTimeline::LaneColors())
	{
		const FLinearColor Color = Named.Color;
		Menu.AddMenuEntry(FUIAction(FExecuteAction::CreateLambda([WeakModel, Lane, Color]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->SetLaneColor(Lane, Color); } })),
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SBox).WidthOverride(12.f).HeightOverride(12.f)[SNew(SImage).Image(OnyxTimeline::Rounded(3.f)).ColorAndOpacity(Color)]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Named.Name)
			]);
	}
	Menu.EndSection();
	return Menu.MakeWidget();
}

TSharedRef<SWidget> SOnyxClipLaneHeaders::MakeLaneMenu(int32 Lane)
{
	TWeakPtr<FOnyxClipTimelineModel> WeakModel = Model;
	const UOnyxClip* Clip = Model->GetClip();
	const int32 Count = Clip ? Clip->Lanes.Num() : 0;
	FMenuBuilder Menu(true, nullptr);
	Menu.BeginSection(NAME_None, Clip && Clip->Lanes.IsValidIndex(Lane) ? FText::FromName(Clip->Lanes[Lane].Name) : INVTEXT("Lane"));
	Menu.AddMenuEntry(INVTEXT("Rename"), INVTEXT("Or double-click the name."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
		FUIAction(FExecuteAction::CreateLambda([this, Lane]()
		{
			if (NameBlocks.IsValidIndex(Lane) && NameBlocks[Lane].IsValid())
			{
				NameBlocks[Lane]->EnterEditingMode();
			}
		})));
	Menu.AddSubMenu(INVTEXT("Colour"), FText::GetEmpty(), FNewMenuDelegate::CreateLambda([this, Lane](FMenuBuilder& Sub)
	{
		TWeakPtr<FOnyxClipTimelineModel> InnerModel = Model;
		for (const OnyxTimeline::FNamedColor& Named : OnyxTimeline::LaneColors())
		{
			const FLinearColor Color = Named.Color;
			Sub.AddMenuEntry(FUIAction(FExecuteAction::CreateLambda([InnerModel, Lane, Color]() { if (TSharedPtr<FOnyxClipTimelineModel> M = InnerModel.Pin()) { M->SetLaneColor(Lane, Color); } })),
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SBox).WidthOverride(12.f).HeightOverride(12.f)[SNew(SImage).Image(OnyxTimeline::Rounded(3.f)).ColorAndOpacity(Color)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(Named.Name)]);
		}
	}));
	Menu.AddMenuEntry(INVTEXT("Move Up"), FText::GetEmpty(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.ChevronUp"),
		FUIAction(FExecuteAction::CreateLambda([WeakModel, Lane]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->MoveLane(Lane, Lane - 1); } }),
			FCanExecuteAction::CreateLambda([Lane]() { return Lane > 0; })));
	Menu.AddMenuEntry(INVTEXT("Move Down"), FText::GetEmpty(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.ChevronDown"),
		FUIAction(FExecuteAction::CreateLambda([WeakModel, Lane]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->MoveLane(Lane, Lane + 1); } }),
			FCanExecuteAction::CreateLambda([Lane, Count]() { return Lane < Count - 1; })));
	Menu.AddMenuEntry(INVTEXT("Add Lane Below"), FText::GetEmpty(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
		FUIAction(FExecuteAction::CreateLambda([WeakModel, Lane]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->InsertLane(Lane + 1); } }),
			FCanExecuteAction::CreateLambda([Count]() { return Count < UOnyxClip::MaxLanes; })));
	Menu.AddMenuEntry(INVTEXT("Delete Lane"), INVTEXT("Removes the lane and its events."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
		FUIAction(FExecuteAction::CreateLambda([WeakModel, Lane]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->RemoveLane(Lane); } }),
			FCanExecuteAction::CreateLambda([Count]() { return Count > 1; })));
	Menu.EndSection();
	return Menu.MakeWidget();
}
