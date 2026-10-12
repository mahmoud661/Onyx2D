// Copyright Onyx2D. All Rights Reserved.

#include "MovesetEditor/Timeline/SOnyxClipTimelineRuler.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MovesetEditor/Timeline/OnyxClipTimelineModel.h"
#include "MovesetEditor/Timeline/OnyxClipTimelineStyle.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/AppStyle.h"
#include "ToolsWindow/OnyxToolWidgets.h"

void SOnyxClipTimelineRuler::Construct(const FArguments& InArgs, const TSharedRef<FOnyxClipTimelineModel>& InModel)
{
	Model = InModel;
	SetClipping(EWidgetClipping::ClipToBounds);
}

FVector2D SOnyxClipTimelineRuler::ComputeDesiredSize(float) const
{
	return FVector2D(200.f, OnyxTimeline::RulerHeight);
}

int32 SOnyxClipTimelineRuler::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* White = FAppStyle::GetBrush("WhiteBrush");
	const FVector2f Size = Geometry.GetLocalSize();
	const FSlateFontInfo Font = OnyxTools::Font(8);
	auto Box = [&](int32 Layer, FVector2f Min, FVector2f BoxSize, const FLinearColor& Color, const FSlateBrush* Brush = nullptr)
	{
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(Min)), Brush ? Brush : White, ESlateDrawEffect::None, Color);
	};
	auto Text = [&](int32 Layer, FVector2f At, const FString& String, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(FVector2f(200.f, 16.f), FSlateLayoutTransform(At)), String, Font, ESlateDrawEffect::None, Color);
	};

	Model->SetViewWidth(Size.X);
	Box(LayerId, FVector2f::ZeroVector, Size, OnyxTimeline::RulerColor());
	Box(LayerId, FVector2f(0.f, Size.Y - 1.f), FVector2f(Size.X, 1.f), OnyxTimeline::Divider());
	if (!Model->GetClip() || !Model->GetFlipbook())
	{
		return LayerId + 1;
	}

	// A tick per frame; numbers as often as they fit.
	const float Ppu = Model->GetPixelsPerUnit();
	const int32 Every = Ppu >= 30.f ? 1 : Ppu >= 15.f ? 2 : Ppu >= 7.f ? 5 : 10;
	const int32 Units = Model->GetUnits();
	const int32 First = FMath::Max(FMath::FloorToInt(Model->GetScrollUnits()), 0);
	const int32 Last = FMath::CeilToInt(Model->XToUnit(Size.X));
	for (int32 Unit = First; Unit <= Last; ++Unit)
	{
		const float X = Model->UnitToX(Unit);
		const bool bLabel = Unit % Every == 0;
		const float Height = bLabel ? 10.f : 5.f;
		Box(LayerId + 1, FVector2f(X, Size.Y - Height - 1.f), FVector2f(1.f, Height), FLinearColor(1.f, 1.f, 1.f, bLabel ? 0.28f : 0.12f));
		if (bLabel && Unit < Units + 20)
		{
			const FString Label = Model->bShowSeconds ? FString::Printf(TEXT("%.2f"), Unit / Model->GetFps()) : FString::FromInt(Unit + 1);
			Text(LayerId + 1, FVector2f(X + 4.f, 4.f), Label, Unit < Units ? OnyxTimeline::MutedText() : OnyxTimeline::MutedText() * 0.6f);
		}
	}

	// After the end of the clip: dimmed.
	const float EndX = Model->UnitToX(Units);
	if (EndX < Size.X)
	{
		Box(LayerId + 2, FVector2f(FMath::Max(EndX, 0.f), 0.f), FVector2f(Size.X - FMath::Max(EndX, 0.f), Size.Y), FLinearColor(0.f, 0.f, 0.f, 0.3f));
	}

	// Playhead badge: the frame number (or the time) on an orange pill, with its line below.
	const float PlayX = Model->TimeToX(Model->GetTime());
	if (PlayX >= -20.f && PlayX <= Size.X + 20.f)
	{
		const FString Label = Model->bShowSeconds ? FString::Printf(TEXT("%.2f"), Model->GetTime()) : FString::FromInt(Model->GetUnitAt(Model->GetTime()) + 1);
		const float TextWidth = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label, Font).X;
		const float PillWidth = FMath::Max(TextWidth + 14.f, 24.f);
		Box(LayerId + 4, FVector2f(PlayX - 1.f, 20.f), FVector2f(2.f, Size.Y - 20.f), OnyxTimeline::Playhead());
		Box(LayerId + 4, FVector2f(PlayX - PillWidth * 0.5f, 4.f), FVector2f(PillWidth, 18.f), OnyxTimeline::Playhead(), OnyxTimeline::Rounded(9.f));
		Text(LayerId + 5, FVector2f(PlayX - TextWidth * 0.5f, 5.f), Label, OnyxTimeline::EventInk(OnyxTimeline::Playhead()));
	}
	return LayerId + 6;
}

void SOnyxClipTimelineRuler::ScrubTo(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const float X = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X;
	const float Units = FMath::Max(Model->XToUnit(X), 0.f);
	// Lands on the frame under the mouse when snapping (Alt flips it), else on the exact millisecond.
	const bool bToFrame = Model->bSnapToFrames != MouseEvent.IsAltDown();
	Model->SetTime(bToFrame ? FMath::FloorToFloat(Units) / Model->GetFps() : Model->Snap(Units / Model->GetFps(), MouseEvent.IsAltDown()));
}

FReply SOnyxClipTimelineRuler::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Model->GetClip())
	{
		bScrubbing = true;
		ScrubTo(MyGeometry, MouseEvent);
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SOnyxClipTimelineRuler::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bScrubbing)
	{
		bScrubbing = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		const FWidgetPath Path = MouseEvent.GetEventPath() ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(AsShared(), Path, MakeOptionsMenu(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SOnyxClipTimelineRuler::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bScrubbing)
	{
		ScrubTo(MyGeometry, MouseEvent);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SOnyxClipTimelineRuler::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const float X = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X;
	if (MouseEvent.IsControlDown())
	{
		Model->Zoom(MouseEvent.GetWheelDelta() > 0.f ? 1.15f : 1.f / 1.15f, X);
	}
	else
	{
		Model->ScrollTo(Model->GetScrollUnits() - MouseEvent.GetWheelDelta() * 2.f);
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SOnyxClipTimelineRuler::MakeOptionsMenu()
{
	TWeakPtr<FOnyxClipTimelineModel> WeakModel = Model;
	FMenuBuilder Menu(true, nullptr);
	Menu.BeginSection(NAME_None, INVTEXT("Timeline"));
	Menu.AddMenuEntry(INVTEXT("Snap to Frames"), INVTEXT("Events, windows and the playhead land on frame edges. Off: anywhere, to the millisecond. Alt flips it while dragging."), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([WeakModel]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->bSnapToFrames = !M->bSnapToFrames; } }),
			FCanExecuteAction(), FIsActionChecked::CreateLambda([WeakModel]() { TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); return M && M->bSnapToFrames; })),
		NAME_None, EUserInterfaceActionType::ToggleButton);
	Menu.AddMenuEntry(INVTEXT("Show Seconds"), INVTEXT("Label the ruler in seconds instead of frame numbers."), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([WeakModel]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->bShowSeconds = !M->bShowSeconds; } }),
			FCanExecuteAction(), FIsActionChecked::CreateLambda([WeakModel]() { TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); return M && M->bShowSeconds; })),
		NAME_None, EUserInterfaceActionType::ToggleButton);
	Menu.AddMenuEntry(INVTEXT("Zoom to Fit"), INVTEXT("Show the whole clip."), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([WeakModel]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->ZoomToFit(); } })));
	Menu.EndSection();
	return Menu.MakeWidget();
}
