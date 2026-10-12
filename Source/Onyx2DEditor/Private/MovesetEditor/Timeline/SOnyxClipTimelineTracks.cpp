// Copyright Onyx2D. All Rights Reserved.

#include "MovesetEditor/Timeline/SOnyxClipTimelineTracks.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxClipEvent.h"
#include "Animation/OnyxClipWindow.h"
#include "Animation/OnyxMoveset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/PackageName.h"
#include "MovesetEditor/Timeline/OnyxClipTimelineModel.h"
#include "MovesetEditor/Timeline/OnyxClipTimelineStyle.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Style/OnyxEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ToolsWindow/OnyxToolWidgets.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBox.h"

namespace
{
	constexpr float DragThreshold = 4.f;
	constexpr float TagHeight = 18.f;
	constexpr float CapsuleInset = 6.f;
	constexpr float EdgeGrab = 7.f;

	/** Event (or window) classes for the Add menus: Onyx's own, then the project's (Blueprint ones are loaded first). */
	void GatherClipEventClasses(bool bWindows, TArray<UClass*>& OutBuiltIn, TArray<UClass*>& OutProject)
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		FARFilter Filter;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.PackagePaths.Add(TEXT("/Game"));
		Filter.bRecursivePaths = true;
		TArray<FAssetData> Blueprints;
		Registry.GetAssets(Filter, Blueprints);
		for (const FAssetData& Asset : Blueprints)
		{
			FString NativeParent;
			if (Asset.GetTagValue(FBlueprintTags::NativeParentClassPath, NativeParent))
			{
				const UClass* Parent = FindObject<UClass>(nullptr, *FPackageName::ExportTextPathToObjectPath(NativeParent));
				if (Parent && Parent->IsChildOf(UOnyxClipEvent::StaticClass()))
				{
					Asset.GetAsset();
				}
			}
		}
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (!Class->IsChildOf(UOnyxClipEvent::StaticClass()) || Class->IsChildOf(UOnyxClipWindow::StaticClass()) != bWindows
				|| Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
				|| Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_"))
				|| !Class->GetDefaultObject<UOnyxClipEvent>()->ShowInAddMenu(nullptr))
			{
				continue;
			}
			(Class->GetOutermost()->GetName() == TEXT("/Script/Onyx2DRuntime") ? OutBuiltIn : OutProject).Add(Class);
		}
		auto ByName = [](const UClass& A, const UClass& B) { return A.GetDisplayNameText().CompareTo(B.GetDisplayNameText()) < 0; };
		OutBuiltIn.Sort(ByName);
		OutProject.Sort(ByName);
	}

	FText TimeLabel(const FOnyxClipTimelineModel& Model, float Seconds)
	{
		return FText::FromString(FString::Printf(TEXT("%d ms · frame %d"), FMath::RoundToInt(Seconds * 1000.f), FMath::FloorToInt(Seconds * Model.GetFps() + 0.001f) + 1));
	}
}

void SOnyxClipTimelineTracks::Construct(const FArguments& InArgs, const TSharedRef<FOnyxClipTimelineModel>& InModel)
{
	Model = InModel;
	SetClipping(EWidgetClipping::ClipToBounds);
}

void SOnyxClipTimelineTracks::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(FrameTextures);
}

int32 SOnyxClipTimelineTracks::GetLaneCount() const
{
	const UOnyxClip* Clip = Model->GetClip();
	return Clip ? Clip->Lanes.Num() : 0;
}

float SOnyxClipTimelineTracks::LaneTop(int32 Lane) const
{
	return OnyxTimeline::FramesHeight + Lane * OnyxTimeline::LaneHeight;
}

int32 SOnyxClipTimelineTracks::LaneAt(float Y) const
{
	return FMath::Clamp(FMath::FloorToInt((Y - OnyxTimeline::FramesHeight) / OnyxTimeline::LaneHeight), 0, FMath::Max(GetLaneCount() - 1, 0));
}

FVector2D SOnyxClipTimelineTracks::ComputeDesiredSize(float) const
{
	return FVector2D(400.f, OnyxTimeline::FramesHeight + GetLaneCount() * OnyxTimeline::LaneHeight + OnyxTimeline::AddLaneHeight);
}

float SOnyxClipTimelineTracks::GetTagWidth(const UOnyxClipEvent* Event) const
{
	const float TextWidth = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Event->GetLabel(), OnyxTools::Font(8)).X;
	return FMath::Min(TextWidth, 150.f) + 30.f;
}

FBox2f SOnyxClipTimelineTracks::GetEventBounds(const UOnyxClipEvent* Event) const
{
	const float Top = LaneTop(Event->Lane);
	const float X = Model->TimeToX(Event->Time);
	if (const UOnyxClipWindow* Window = Cast<UOnyxClipWindow>(Event))
	{
		const float X1 = FMath::Max(Model->TimeToX(Window->GetEndTime()), X + 10.f);
		return FBox2f(FVector2f(X, Top + CapsuleInset), FVector2f(X1, Top + OnyxTimeline::LaneHeight - CapsuleInset));
	}
	return FBox2f(FVector2f(X - 4.f, Top + 4.f), FVector2f(X + GetTagWidth(Event), Top + OnyxTimeline::LaneHeight));
}

int32 SOnyxClipTimelineTracks::KeyFrameAtX(float X, bool* bOnEdge) const
{
	const UPaperFlipbook* Flipbook = Model->GetFlipbook();
	if (bOnEdge)
	{
		*bOnEdge = false;
	}
	if (!Flipbook)
	{
		return INDEX_NONE;
	}
	int32 Units = 0;
	for (int32 Index = 0; Index < Flipbook->GetNumKeyFrames(); ++Index)
	{
		const int32 Run = FMath::Max(Flipbook->GetKeyFrameChecked(Index).FrameRun, 1);
		const float X0 = Model->UnitToX(Units);
		const float X1 = Model->UnitToX(Units + Run);
		if (X >= X0 && X < X1 + EdgeGrab * 0.5f)
		{
			if (bOnEdge)
			{
				*bOnEdge = X > X1 - EdgeGrab;
			}
			return Index;
		}
		Units += Run;
	}
	return INDEX_NONE;
}

SOnyxClipTimelineTracks::FHit SOnyxClipTimelineTracks::HitTest(const FVector2f& Local) const
{
	FHit Hit;
	const UOnyxClip* Clip = Model->GetClip();
	if (!Clip)
	{
		return Hit;
	}
	if (Local.Y < OnyxTimeline::FramesHeight)
	{
		Hit.Row = ERow::Frames;
		Hit.KeyFrame = KeyFrameAtX(Local.X, &Hit.bFrameEdge);
		return Hit;
	}
	const int32 Lane = FMath::FloorToInt((Local.Y - OnyxTimeline::FramesHeight) / OnyxTimeline::LaneHeight);
	if (Lane >= Clip->Lanes.Num())
	{
		Hit.Row = ERow::AddLane;
		return Hit;
	}
	Hit.Row = ERow::Lane;
	Hit.Lane = Lane;
	// Top-most first: flags over windows, the selection over the rest.
	UOnyxClipEvent* Best = nullptr;
	int32 BestRank = -1;
	for (UOnyxClipEvent* Event : Clip->Events)
	{
		if (!Event || Event->Lane != Lane)
		{
			continue;
		}
		const FBox2f Bounds = GetEventBounds(Event);
		const bool bWindow = Event->IsWindow();
		bool bInside = Local.X >= Bounds.Min.X - 2.f && Local.X <= Bounds.Max.X + 2.f;
		if (!bWindow)
		{
			// The tag, or the stem anywhere down the lane.
			const float StemX = Model->TimeToX(Event->Time);
			const bool bOnTag = bInside && Local.Y <= LaneTop(Lane) + 5.f + TagHeight + 2.f;
			bInside = bOnTag || FMath::Abs(Local.X - StemX) <= 5.f;
		}
		const int32 Rank = (bWindow ? 0 : 2) + (Model->IsSelected(Event) ? 1 : 0);
		if (bInside && Rank >= BestRank)
		{
			Best = Event;
			BestRank = Rank;
			Hit.bWindowEnd = bWindow && Local.X > Bounds.Max.X - EdgeGrab;
		}
	}
	Hit.Event = Best;
	return Hit;
}

// Painting -------------------------------------------------------------------------------------------------------------

int32 SOnyxClipTimelineTracks::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* White = FAppStyle::GetBrush("WhiteBrush");
	const FVector2f Size = Geometry.GetLocalSize();
	const FSlateFontInfo Small = OnyxTools::Font(8);
	auto Box = [&](int32 Layer, FVector2f Min, FVector2f BoxSize, const FLinearColor& Color, const FSlateBrush* Brush = nullptr)
	{
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(Min)), Brush ? Brush : White, ESlateDrawEffect::None, Color);
	};
	auto Text = [&](int32 Layer, FVector2f At, const FString& String, const FLinearColor& Color, const FSlateFontInfo& Font)
	{
		FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(FVector2f(400.f, 16.f), FSlateLayoutTransform(At)), String, Font, ESlateDrawEffect::None, Color);
	};
	auto PushClip = [&](FVector2f Min, FVector2f ClipSize)
	{
		Out.PushClip(FSlateClippingZone(Geometry.ToPaintGeometry(ClipSize, FSlateLayoutTransform(Min))));
	};

	Model->SetViewWidth(Size.X);
	const UOnyxClip* Clip = Model->GetClip();
	const UPaperFlipbook* Flipbook = Model->GetFlipbook();
	Box(LayerId, FVector2f::ZeroVector, Size, OnyxTimeline::Panel());
	if (!Clip || !Flipbook)
	{
		Text(LayerId + 1, FVector2f(16.f, 22.f), TEXT("Pick a clip on the left, or make one with New Clip."), OnyxTimeline::MutedText(), OnyxTools::Font(9));
		return LayerId + 2;
	}

	const int32 Lanes = Clip->Lanes.Num();
	const int32 Units = Model->GetUnits();
	const float EndX = Model->UnitToX(Units);
	const float LanesBottom = LaneTop(Lanes);

	// Rows.
	Box(LayerId, FVector2f::ZeroVector, FVector2f(Size.X, OnyxTimeline::FramesHeight), OnyxTimeline::FramesRow());
	for (int32 Lane = 0; Lane < Lanes; ++Lane)
	{
		Box(LayerId, FVector2f(0.f, LaneTop(Lane)), FVector2f(Size.X, OnyxTimeline::LaneHeight), OnyxTimeline::LaneRow(Clip->Lanes[Lane].Color, Lane));
		Box(LayerId, FVector2f(0.f, LaneTop(Lane)), FVector2f(Size.X, 1.f), OnyxTimeline::Divider());
	}
	Box(LayerId, FVector2f(0.f, LanesBottom), FVector2f(Size.X, Size.Y - LanesBottom), OnyxTimeline::Panel() * 0.85f);
	Box(LayerId, FVector2f(0.f, LanesBottom), FVector2f(Size.X, 1.f), OnyxTimeline::Divider());

	// A faint line per frame through the lanes.
	const int32 First = FMath::Max(FMath::FloorToInt(Model->GetScrollUnits()), 0);
	const int32 Last = FMath::Min(FMath::CeilToInt(Model->XToUnit(Size.X)), Units);
	for (int32 Unit = First; Unit <= Last; ++Unit)
	{
		Box(LayerId + 1, FVector2f(Model->UnitToX(Unit), OnyxTimeline::FramesHeight), FVector2f(1.f, LanesBottom - OnyxTimeline::FramesHeight), FLinearColor(1.f, 1.f, 1.f, 0.035f));
	}

	// Frames: a rounded cell per key frame with its picture; the one under the playhead is lit.
	FrameBrushes.SetNum(Flipbook->GetNumKeyFrames());
	FrameTextures.SetNum(Flipbook->GetNumKeyFrames());
	const int32 PlayingFrame = Model->GetKeyFrameAt(Model->GetTime());
	int32 FrameUnits = 0;
	for (int32 Index = 0; Index < Flipbook->GetNumKeyFrames(); ++Index)
	{
		const FPaperFlipbookKeyFrame& Key = Flipbook->GetKeyFrameChecked(Index);
		const int32 Run = FMath::Max(Key.FrameRun, 1);
		const float X0 = Model->UnitToX(FrameUnits);
		FrameUnits += Run;
		const FVector2f Min(X0 + 2.f, 5.f);
		const FVector2f CellSize(FMath::Max(Run * Model->GetPixelsPerUnit() - 4.f, 4.f), OnyxTimeline::FramesHeight - 10.f);
		if (Min.X > Size.X || Min.X + CellSize.X < 0.f)
		{
			continue;
		}
		const bool bSelected = Index == Model->GetSelectedFrame();
		const bool bDragged = Drag == EDrag::FrameMove && Index == PressHit.KeyFrame;
		Box(LayerId + 1, Min - FVector2f(1.f, 1.f), CellSize + FVector2f(2.f, 2.f), bSelected ? OnyxTimeline::Accent() : OnyxTimeline::FrameCellLine(), OnyxTimeline::Rounded(7.f));
		Box(LayerId + 1, Min, CellSize, (Index == PlayingFrame ? OnyxTimeline::FrameCell() * 1.25f : OnyxTimeline::FrameCell()).CopyWithNewOpacity(bDragged ? 0.5f : 1.f), OnyxTimeline::Rounded(6.f));
		if (UPaperSprite* Sprite = Key.Sprite; Sprite && Sprite->GetSourceTexture() && CellSize.X > 10.f)
		{
			UTexture2D* Texture = Sprite->GetSourceTexture();
			const FVector2f TextureSize(FMath::Max(Texture->GetSurfaceWidth(), 1.f), FMath::Max(Texture->GetSurfaceHeight(), 1.f));
			FSlateBrush& Brush = FrameBrushes[Index];
			Brush.SetResourceObject(Texture);
			Brush.SetUVRegion(FBox2f(FVector2f(Sprite->GetSourceUV()) / TextureSize, (FVector2f(Sprite->GetSourceUV()) + FVector2f(Sprite->GetSourceSize())) / TextureSize));
			Brush.DrawAs = ESlateBrushDrawType::Image;
			FrameTextures[Index] = Texture;
			const FVector2f SpriteSize(Sprite->GetSourceSize());
			const FVector2f Room(FMath::Min(CellSize.X, Model->GetPixelsPerUnit() * 1.5f) - 8.f, CellSize.Y - 14.f);
			const float Fit = FMath::Max(FMath::Min(Room.X / FMath::Max(SpriteSize.X, 1.f), Room.Y / FMath::Max(SpriteSize.Y, 1.f)), 0.01f);
			const FVector2f Draw = SpriteSize * Fit;
			Box(LayerId + 2, FVector2f(Min.X + (CellSize.X - Draw.X) * 0.5f, Min.Y + 2.f + (Room.Y - Draw.Y) * 0.5f), Draw, FLinearColor::White.CopyWithNewOpacity(bDragged ? 0.5f : 1.f), &Brush);
		}
		if (CellSize.X > 14.f)
		{
			Text(LayerId + 3, FVector2f(Min.X + 5.f, Min.Y + CellSize.Y - 15.f), FString::FromInt(Index + 1), bSelected ? OnyxTimeline::TextColor() : OnyxTimeline::MutedText(), Small);
		}
		if (Run > 1 && CellSize.X > 40.f)
		{
			const FString Hold = FString::Printf(TEXT("x%d"), Run);
			Box(LayerId + 3, FVector2f(Min.X + CellSize.X - 26.f, Min.Y + CellSize.Y - 16.f), FVector2f(22.f, 13.f), FLinearColor(1.f, 1.f, 1.f, 0.1f), OnyxTimeline::Rounded(6.f));
			Text(LayerId + 4, FVector2f(Min.X + CellSize.X - 22.f, Min.Y + CellSize.Y - 16.f), Hold, OnyxTimeline::TextColor(), Small);
		}
	}
	if (Drag == EDrag::FrameMove && FrameDropSlot != INDEX_NONE)
	{
		const float DropX = Model->UnitToX(Model->GetKeyFrameStart(FrameDropSlot + (FrameDropSlot > PressHit.KeyFrame ? 1 : 0)));
		Box(LayerId + 5, FVector2f(DropX - 1.5f, 3.f), FVector2f(3.f, OnyxTimeline::FramesHeight - 6.f), OnyxTimeline::Accent(), OnyxTimeline::Rounded(1.5f));
	}

	// After the end of the clip: dimmed.
	if (EndX < Size.X)
	{
		Box(LayerId + 3, FVector2f(FMath::Max(EndX, 0.f), 0.f), FVector2f(Size.X - FMath::Max(EndX, 0.f), LanesBottom), FLinearColor(0.f, 0.f, 0.f, 0.28f));
	}

	// Windows wash the frames they cover (stronger when selected).
	for (const UOnyxClipEvent* Event : Clip->Events)
	{
		if (const UOnyxClipWindow* Window = Cast<UOnyxClipWindow>(Event))
		{
			const bool bSelected = Model->IsSelected(Window);
			const float X0 = Model->TimeToX(Window->Time);
			const float X1 = Model->TimeToX(Window->GetEndTime());
			const FLinearColor Color = OnyxTimeline::EventFill(Window->TimelineColor);
			Box(LayerId + 4, FVector2f(X0, 0.f), FVector2f(FMath::Max(X1 - X0, 1.f), OnyxTimeline::FramesHeight), Color.CopyWithNewOpacity(bSelected ? 0.2f : 0.06f));
			if (bSelected)
			{
				Box(LayerId + 4, FVector2f(X0, 0.f), FVector2f(1.f, OnyxTimeline::FramesHeight), Color.CopyWithNewOpacity(0.8f));
				Box(LayerId + 4, FVector2f(X1 - 1.f, 0.f), FVector2f(1.f, OnyxTimeline::FramesHeight), Color.CopyWithNewOpacity(0.8f));
			}
		}
	}

	// Events: windows (capsules) first, then flags; the selection on top.
	TArray<const UOnyxClipEvent*> Ordered;
	for (const UOnyxClipEvent* Event : Clip->Events)
	{
		if (Event && Clip->Lanes.IsValidIndex(Event->Lane))
		{
			Ordered.Add(Event);
		}
	}
	Ordered.StableSort([this](const UOnyxClipEvent& A, const UOnyxClipEvent& B)
	{
		const int32 RankA = (A.IsWindow() ? 0 : 2) + (Model->IsSelected(&A) ? 1 : 0);
		const int32 RankB = (B.IsWindow() ? 0 : 2) + (Model->IsSelected(&B) ? 1 : 0);
		return RankA < RankB;
	});
	int32 Layer = LayerId + 6;
	for (const UOnyxClipEvent* Event : Ordered)
	{
		const bool bSelected = Model->IsSelected(Event);
		const bool bHovered = Event == Hovered.Get();
		const FLinearColor Fill = OnyxTimeline::EventFill(Event->TimelineColor) * (bHovered && !bSelected ? 1.12f : 1.f);
		const FLinearColor Ink = OnyxTimeline::EventInk(Event->TimelineColor);
		const float Top = LaneTop(Event->Lane);
		const FString Label = Event->GetLabel().ToString();
		if (const UOnyxClipWindow* Window = Cast<UOnyxClipWindow>(Event))
		{
			const FBox2f Bounds = GetEventBounds(Window);
			const FVector2f CapsuleSize = Bounds.GetSize();
			if (bSelected)
			{
				Box(Layer, Bounds.Min - FVector2f(2.f, 2.f), CapsuleSize + FVector2f(4.f, 4.f), FLinearColor::White, OnyxTimeline::Rounded(12.f));
			}
			Box(Layer + 1, Bounds.Min, CapsuleSize, Fill.CopyWithNewOpacity(1.f), OnyxTimeline::Rounded(10.f));
			PushClip(Bounds.Min, FVector2f(FMath::Max(CapsuleSize.X - 10.f, 0.f), CapsuleSize.Y));
			Box(Layer + 2, FVector2f(Bounds.Min.X + 7.f, Bounds.Min.Y + 4.f), FVector2f(12.f, 12.f), Ink, OnyxTimeline::EventIcon(Event));
			Text(Layer + 2, FVector2f(Bounds.Min.X + 23.f, Bounds.Min.Y + 3.f), Label, Ink, Small);
			Out.PopClip();
			// Grab handle at the end: drag it to change the length.
			Box(Layer + 2, FVector2f(Bounds.Max.X - 8.f, Bounds.Min.Y + 4.f), FVector2f(3.f, CapsuleSize.Y - 8.f), Ink.CopyWithNewOpacity(0.45f), OnyxTimeline::Rounded(1.5f));
			Layer += 3;
			continue;
		}
		const float X = Model->TimeToX(Event->Time);
		const float TagWidth = GetTagWidth(Event);
		if (bSelected)
		{
			Box(Layer, FVector2f(X - 3.f, Top + 3.f), FVector2f(TagWidth + 4.f, TagHeight + 4.f), FLinearColor::White, OnyxTimeline::FlagTag());
			Box(Layer, FVector2f(X - 2.f, Top + 4.f), FVector2f(4.f, OnyxTimeline::LaneHeight - 4.f), FLinearColor::White);
		}
		Box(Layer + 1, FVector2f(X - 1.f, Top + 5.f), FVector2f(2.f, OnyxTimeline::LaneHeight - 5.f), Fill.CopyWithNewOpacity(1.f));
		Box(Layer + 1, FVector2f(X - 1.f, Top + 5.f), FVector2f(TagWidth, TagHeight), Fill.CopyWithNewOpacity(1.f), OnyxTimeline::FlagTag());
		PushClip(FVector2f(X - 1.f, Top + 5.f), FVector2f(TagWidth - 4.f, TagHeight));
		Box(Layer + 2, FVector2f(X + 5.f, Top + 8.f), FVector2f(12.f, 12.f), Ink, OnyxTimeline::EventIcon(Event));
		Text(Layer + 2, FVector2f(X + 20.f, Top + 7.f), Label, Ink, Small);
		Out.PopClip();
		Layer += 3;
	}

	if (Clip->Events.IsEmpty() && Lanes > 0)
	{
		Text(LayerId + 5, FVector2f(FMath::Max(Model->UnitToX(0.f), 0.f) + 10.f, LaneTop(0) + 9.f),
			TEXT("Right-click a lane to add an event or a window. Double-click repeats the last one."), OnyxTimeline::MutedText(), Small);
	}

	// While dragging: a guide line through everything and a readout of where it lands.
	const UOnyxClipEvent* Focused = Model->GetFocusedEvent();
	if ((Drag == EDrag::Move || Drag == EDrag::Resize) && Focused)
	{
		const UOnyxClipWindow* Window = Cast<UOnyxClipWindow>(Focused);
		const float GuideTime = Drag == EDrag::Resize && Window ? Window->GetEndTime() : Focused->Time;
		const float GuideX = Model->TimeToX(GuideTime);
		Box(Layer, FVector2f(GuideX, 0.f), FVector2f(1.f, LanesBottom), FLinearColor(1.f, 1.f, 1.f, 0.5f));
		const FString Readout = TimeLabel(*Model, GuideTime).ToString();
		const float ReadoutWidth = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Readout, Small).X + 14.f;
		const FVector2f At(FMath::Clamp(GuideX + 8.f, 0.f, FMath::Max(Size.X - ReadoutWidth, 0.f)), FMath::Max(LaneTop(Focused->Lane) - 20.f, 2.f));
		Box(Layer + 1, At, FVector2f(ReadoutWidth, 17.f), FLinearColor(0.f, 0.f, 0.f, 0.85f), OnyxTimeline::Rounded(8.f));
		Text(Layer + 2, At + FVector2f(7.f, 1.f), Readout, FLinearColor::White, Small);
		Layer += 3;
	}

	// Box selection.
	if (Drag == EDrag::Marquee)
	{
		const FVector2f Min(FMath::Min(PressPosition.X, MousePosition.X), FMath::Min(PressPosition.Y, MousePosition.Y));
		const FVector2f Max(FMath::Max(PressPosition.X, MousePosition.X), FMath::Max(PressPosition.Y, MousePosition.Y));
		Box(Layer, Min, Max - Min, OnyxTimeline::Accent().CopyWithNewOpacity(0.15f));
		const TArray<FVector2f> Outline = { Min, FVector2f(Max.X, Min.Y), Max, FVector2f(Min.X, Max.Y), Min };
		FSlateDrawElement::MakeLines(Out, Layer + 1, Geometry.ToPaintGeometry(), Outline, ESlateDrawEffect::None, OnyxTimeline::Accent(), true, 1.f);
		Layer += 2;
	}

	// Playhead.
	const float PlayX = Model->TimeToX(Model->GetTime());
	if (PlayX >= -2.f && PlayX <= Size.X + 2.f)
	{
		Box(Layer, FVector2f(PlayX - 1.f, 0.f), FVector2f(2.f, LanesBottom), OnyxTimeline::Playhead());
	}
	return Layer + 1;
}

// Mouse ----------------------------------------------------------------------------------------------------------------

FReply SOnyxClipTimelineTracks::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2f Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	PressPosition = MousePosition = Local;
	if (!Model->GetClip())
	{
		return FReply::Unhandled();
	}
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bRightPressed = true;
		return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		Drag = EDrag::Pan;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	PressHit = HitTest(Local);
	if (UOnyxClipEvent* Event = PressHit.Event.Get())
	{
		if (MouseEvent.IsControlDown())
		{
			Model->SelectEvent(Event, EOnyxSelect::Toggle);
		}
		else if (MouseEvent.IsShiftDown())
		{
			Model->SelectEvent(Event, EOnyxSelect::Add);
		}
		else if (!Model->IsSelected(Event))
		{
			Model->SelectEvent(Event, EOnyxSelect::Replace);
		}
		else
		{
			Model->SelectEvent(Event, EOnyxSelect::Add); // keeps the group, makes it the lead
		}
		Drag = PressHit.bWindowEnd ? EDrag::PendingResize : EDrag::PendingMove;
	}
	else if (PressHit.Row == ERow::Frames && PressHit.KeyFrame != INDEX_NONE)
	{
		Model->SelectFrame(PressHit.KeyFrame);
		Model->SetTime(Model->GetKeyFrameStart(PressHit.KeyFrame) / Model->GetFps());
		Drag = PressHit.bFrameEdge ? EDrag::PendingHold : EDrag::PendingFrameMove;
	}
	else if (PressHit.Row == ERow::Lane || PressHit.Row == ERow::Frames)
	{
		Drag = EDrag::PendingMarquee;
	}
	return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SOnyxClipTimelineTracks::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2f Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FVector2f Previous = MousePosition;
	MousePosition = Local;

	// Start a drag once the mouse has really moved.
	if (FVector2f::Distance(Local, PressPosition) > DragThreshold)
	{
		switch (Drag)
		{
		case EDrag::PendingMove:
			Model->BeginMoveSelection();
			Drag = EDrag::Move;
			break;
		case EDrag::PendingResize:
			Model->BeginResizeWindow(Cast<UOnyxClipWindow>(PressHit.Event.Get()));
			Drag = EDrag::Resize;
			break;
		case EDrag::PendingHold:
			if (const UPaperFlipbook* Flipbook = Model->GetFlipbook(); Flipbook && Flipbook->IsValidKeyFrameIndex(PressHit.KeyFrame))
			{
				HoldStart = Model->GetKeyFrameStart(PressHit.KeyFrame);
				Model->BeginFrameHoldDrag(PressHit.KeyFrame);
				Drag = EDrag::Hold;
			}
			break;
		case EDrag::PendingFrameMove:
			Drag = EDrag::FrameMove;
			break;
		case EDrag::PendingMarquee:
			Drag = EDrag::Marquee;
			break;
		default:
			break;
		}
	}

	const bool bFlipSnap = MouseEvent.IsAltDown();
	switch (Drag)
	{
	case EDrag::Move:
		Model->UpdateMoveSelection(Model->XToTime(Local.X) - Model->XToTime(PressPosition.X), PressHit.Lane == INDEX_NONE ? 0 : LaneAt(Local.Y) - PressHit.Lane, bFlipSnap);
		return FReply::Handled();
	case EDrag::Resize:
		Model->UpdateResizeWindow(Model->XToTime(Local.X), bFlipSnap);
		return FReply::Handled();
	case EDrag::Hold:
		Model->UpdateFrameHoldDrag(FMath::Max(FMath::RoundToInt(Model->XToUnit(Local.X) - HoldStart), 1));
		return FReply::Handled();
	case EDrag::FrameMove:
		FrameDropSlot = KeyFrameAtX(FMath::Max(Local.X, 0.f));
		if (FrameDropSlot == INDEX_NONE && Model->GetFlipbook())
		{
			FrameDropSlot = Model->GetFlipbook()->GetNumKeyFrames() - 1;
		}
		return FReply::Handled();
	case EDrag::Marquee:
	{
		const FBox2f Area(FVector2f(FMath::Min(PressPosition.X, Local.X), FMath::Min(PressPosition.Y, Local.Y)),
			FVector2f(FMath::Max(PressPosition.X, Local.X), FMath::Max(PressPosition.Y, Local.Y)));
		TArray<UOnyxClipEvent*> Inside;
		for (UOnyxClipEvent* Event : Model->GetClip()->Events)
		{
			if (Event && Area.Intersect(GetEventBounds(Event)))
			{
				Inside.Add(Event);
			}
		}
		Model->SelectEvents(Inside, MouseEvent.IsShiftDown() ? EOnyxSelect::Add : EOnyxSelect::Replace);
		return FReply::Handled();
	}
	case EDrag::Pan:
		Model->ScrollTo(Model->GetScrollUnits() - (Local.X - Previous.X) / Model->GetPixelsPerUnit());
		return FReply::Handled();
	default:
		break;
	}

	// Hover: highlight and describe the event under the mouse.
	const FHit Hit = HitTest(Local);
	if (Hit.Event != Hovered)
	{
		Hovered = Hit.Event;
		if (const UOnyxClipEvent* Event = Hovered.Get())
		{
			SetToolTipText(FText::Format(INVTEXT("{0} ({1})\n{2}"), Event->GetLabel(), Event->GetClass()->GetDisplayNameText(), TimeLabel(*Model, Event->Time)));
		}
		else
		{
			SetToolTipText(FText::GetEmpty());
		}
	}
	return FReply::Unhandled();
}

FReply SOnyxClipTimelineTracks::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2f Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bRightPressed)
	{
		bRightPressed = false;
		const FHit Hit = HitTest(PressPosition);
		if (UOnyxClipEvent* Event = Hit.Event.Get())
		{
			if (!Model->IsSelected(Event))
			{
				Model->SelectEvent(Event, EOnyxSelect::Replace);
			}
			OpenMenu(MouseEvent, MakeEventMenu(Event));
		}
		else if (Hit.Row == ERow::Frames && Hit.KeyFrame != INDEX_NONE)
		{
			Model->SelectFrame(Hit.KeyFrame);
			OpenMenu(MouseEvent, MakeFrameMenu(Hit.KeyFrame));
		}
		else if (Hit.Row == ERow::Lane)
		{
			OpenMenu(MouseEvent, MakeLaneMenu(FMath::Max(Model->XToTime(PressPosition.X), 0.f), Hit.Lane));
		}
		return FReply::Handled();
	}
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton && MouseEvent.GetEffectingButton() != EKeys::MiddleMouseButton)
	{
		return FReply::Unhandled();
	}

	switch (Drag)
	{
	case EDrag::Move:
	case EDrag::Resize:
	case EDrag::Hold:
		Model->EndDrag();
		break;
	case EDrag::FrameMove:
		if (FrameDropSlot != INDEX_NONE && FrameDropSlot != PressHit.KeyFrame)
		{
			Model->MoveFrame(PressHit.KeyFrame, FrameDropSlot);
		}
		break;
	case EDrag::PendingMarquee:
		// A plain click on empty space: clear the selection and move the playhead there.
		if (!MouseEvent.IsShiftDown() && !MouseEvent.IsControlDown())
		{
			Model->ClearSelection();
		}
		Model->SetTime(Model->Snap(FMath::Max(Model->XToTime(Local.X), 0.f), MouseEvent.IsAltDown()));
		break;
	case EDrag::PendingMove:
		// Clicking one event of a group (without moving) selects just that one.
		if (!MouseEvent.IsShiftDown() && !MouseEvent.IsControlDown() && Model->GetSelectedEvents().Num() > 1)
		{
			Model->SelectEvent(PressHit.Event.Get(), EOnyxSelect::Replace);
		}
		break;
	default:
		break;
	}
	Drag = EDrag::None;
	FrameDropSlot = INDEX_NONE;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SOnyxClipTimelineTracks::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2f Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FHit Hit = HitTest(Local);
	if (Hit.Row == ERow::AddLane)
	{
		Model->InsertLane(GetLaneCount());
		return FReply::Handled();
	}
	if (Hit.Row == ERow::Lane && !Hit.Event.IsValid())
	{
		const float Time = FMath::Max(Model->XToTime(Local.X), 0.f);
		if (UClass* Last = Model->GetLastAddedClass())
		{
			Model->AddEvent(Last, Time, Hit.Lane);
		}
		else
		{
			OpenMenu(MouseEvent, MakeLaneMenu(Time, Hit.Lane));
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SOnyxClipTimelineTracks::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SLeafWidget::OnMouseLeave(MouseEvent);
	Hovered.Reset();
}

FReply SOnyxClipTimelineTracks::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const float X = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X;
	if (MouseEvent.IsControlDown())
	{
		Model->Zoom(MouseEvent.GetWheelDelta() > 0.f ? 1.15f : 1.f / 1.15f, X);
		return FReply::Handled();
	}
	if (MouseEvent.IsShiftDown())
	{
		Model->ScrollTo(Model->GetScrollUnits() - MouseEvent.GetWheelDelta() * 2.f);
		return FReply::Handled();
	}
	return FReply::Unhandled(); // the lanes scroll up and down
}

FCursorReply SOnyxClipTimelineTracks::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	switch (Drag)
	{
	case EDrag::Move:
	case EDrag::FrameMove:
	case EDrag::Pan:
		return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
	case EDrag::Resize:
	case EDrag::Hold:
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	default:
		break;
	}
	const FHit Hit = HitTest(MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition()));
	if (Hit.bWindowEnd || (Hit.Row == ERow::Frames && Hit.bFrameEdge))
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	}
	if (Hit.Event.IsValid())
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHand);
	}
	return FCursorReply::Unhandled();
}

FReply SOnyxClipTimelineTracks::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const bool bCtrl = InKeyEvent.IsControlDown();
	const bool bHasEvents = !Model->GetSelectedEvents().IsEmpty();
	const int32 Frame = Model->GetSelectedFrame();
	const UOnyxClipEvent* Focused = Model->GetFocusedEvent();
	if (Key == EKeys::Delete || Key == EKeys::BackSpace)
	{
		bHasEvents ? Model->DeleteSelection() : Model->DeleteFrame(Frame);
	}
	else if (bCtrl && Key == EKeys::C)
	{
		Model->CopySelection();
	}
	else if (bCtrl && Key == EKeys::V)
	{
		Model->Paste(Model->GetTime(), Focused ? Focused->Lane : 0);
	}
	else if (bCtrl && Key == EKeys::D)
	{
		bHasEvents ? Model->DuplicateSelection() : Model->DuplicateFrame(Frame);
	}
	else if (bCtrl && Key == EKeys::A)
	{
		Model->SelectAllEvents();
	}
	else if (Key == EKeys::SpaceBar)
	{
		Model->TogglePlay();
	}
	else if (Key == EKeys::Left || Key == EKeys::Right)
	{
		Model->StepFrames(Key == EKeys::Left ? -1 : 1);
	}
	else if (Key == EKeys::Home)
	{
		Model->JumpToStart();
	}
	else if (Key == EKeys::End)
	{
		Model->JumpToEnd();
	}
	else if (Key == EKeys::Escape)
	{
		Model->ClearSelection();
	}
	else
	{
		return FReply::Unhandled();
	}
	return FReply::Handled();
}

// Menus ----------------------------------------------------------------------------------------------------------------

void SOnyxClipTimelineTracks::OpenMenu(const FPointerEvent& MouseEvent, const TSharedRef<SWidget>& Menu)
{
	// The event path tells Slate which window owns the menu.
	const FWidgetPath Path = MouseEvent.GetEventPath() ? *MouseEvent.GetEventPath() : FWidgetPath();
	FSlateApplication::Get().PushMenu(AsShared(), Path, Menu, MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

void SOnyxClipTimelineTracks::NewMessage(float Time, int32 Lane)
{
	const UOnyxMoveset* Moveset = Model->GetMoveset();
	const FName Suggested = Moveset ? Moveset->MakeMessageName(TEXT("Message")) : FName(TEXT("Message"));
	TWeakPtr<FOnyxClipTimelineModel> WeakModel = Model;
	OnyxTimeline::PromptForName(AsShared(), INVTEXT("New Message"), FText::FromName(Suggested), [WeakModel, Time, Lane](FName Name)
	{
		if (TSharedPtr<FOnyxClipTimelineModel> Pinned = WeakModel.Pin())
		{
			Pinned->AddMessage(Name, Time, Lane);
		}
	});
}

void SOnyxClipTimelineTracks::FillAddMenu(FMenuBuilder& Menu, bool bWindows, float Time, int32 Lane)
{
	TWeakPtr<FOnyxClipTimelineModel> WeakModel = Model;
	if (!bWindows)
	{
		Menu.BeginSection(NAME_None, INVTEXT("Messages"));
		const UOnyxMoveset* Moveset = Model->GetMoveset();
		for (const FName& Message : Moveset ? Moveset->Messages : TArray<FName>())
		{
			Menu.AddMenuEntry(FText::FromName(Message), INVTEXT("Runs the character's function of that name and fires On Clip Message."),
				FSlateIcon(FOnyxEditorStyle::GetStyleSetName(), "Onyx2D.Event.Message.Small"),
				FUIAction(FExecuteAction::CreateLambda([WeakModel, Message, Time, Lane]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->AddMessage(Message, Time, Lane); } })));
		}
		Menu.AddMenuEntry(INVTEXT("New Message..."), INVTEXT("Name a new message for this moveset and send it here."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
			FUIAction(FExecuteAction::CreateSP(this, &SOnyxClipTimelineTracks::NewMessage, Time, Lane)));
		Menu.EndSection();
	}
	TArray<UClass*> BuiltIn;
	TArray<UClass*> Project;
	GatherClipEventClasses(bWindows, BuiltIn, Project);
	auto AddClasses = [&](const TArray<UClass*>& Classes, const FText& Title)
	{
		if (Classes.IsEmpty())
		{
			return;
		}
		Menu.BeginSection(NAME_None, Title);
		for (UClass* Class : Classes)
		{
			TWeakObjectPtr<UClass> WeakClass = Class;
			Menu.AddMenuEntry(Class->GetDisplayNameText(), Class->GetToolTipText(), FSlateIcon(FOnyxEditorStyle::GetStyleSetName(), OnyxTimeline::EventIconName(Class)),
				FUIAction(FExecuteAction::CreateLambda([WeakModel, WeakClass, Time, Lane]()
				{
					TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin();
					if (M && WeakClass.IsValid())
					{
						M->AddEvent(WeakClass.Get(), Time, Lane);
					}
				})));
		}
		Menu.EndSection();
	};
	AddClasses(BuiltIn, bWindows ? INVTEXT("Windows") : INVTEXT("Events"));
	AddClasses(Project, INVTEXT("Your Own"));
}

TSharedRef<SWidget> SOnyxClipTimelineTracks::MakeLaneMenu(float Time, int32 Lane)
{
	const UOnyxClip* Clip = Model->GetClip();
	const FText LaneName = Clip && Clip->Lanes.IsValidIndex(Lane) ? FText::FromName(Clip->Lanes[Lane].Name) : FText::GetEmpty();
	const float Snapped = Model->Snap(Time, false);
	TWeakPtr<FOnyxClipTimelineModel> WeakModel = Model;

	FMenuBuilder Menu(true, nullptr);
	Menu.BeginSection(NAME_None, FText::Format(INVTEXT("{0} · {1}"), LaneName, TimeLabel(*Model, Snapped)));
	Menu.AddSubMenu(INVTEXT("Add Event"), INVTEXT("Something that happens at this moment: a message, a sound, an effect..."),
		FNewMenuDelegate::CreateSP(this, &SOnyxClipTimelineTracks::FillAddMenu, false, Snapped, Lane), false,
		FSlateIcon(FOnyxEditorStyle::GetStyleSetName(), "Onyx2D.Event.Custom.Small"));
	Menu.AddSubMenu(INVTEXT("Add Window"), INVTEXT("Something that stays open for a while: a hitbox, invulnerability, a looping sound..."),
		FNewMenuDelegate::CreateSP(this, &SOnyxClipTimelineTracks::FillAddMenu, true, Snapped, Lane), false,
		FSlateIcon(FOnyxEditorStyle::GetStyleSetName(), "Onyx2D.Window.Custom.Small"));
	Menu.AddMenuEntry(INVTEXT("Paste"), INVTEXT("Paste the copied events here."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Paste"),
		FUIAction(FExecuteAction::CreateLambda([WeakModel, Snapped, Lane]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->Paste(Snapped, Lane); } }),
			FCanExecuteAction::CreateLambda([WeakModel]() { TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); return M && M->CanPaste(); })));
	Menu.EndSection();
	Menu.BeginSection(NAME_None, INVTEXT("Lane"));
	Menu.AddMenuEntry(INVTEXT("Add Lane Below"), FText::GetEmpty(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
		FUIAction(FExecuteAction::CreateLambda([WeakModel, Lane]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->InsertLane(Lane + 1); } }),
			FCanExecuteAction::CreateLambda([WeakModel]() { TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); return M && M->GetClip() && M->GetClip()->Lanes.Num() < UOnyxClip::MaxLanes; })));
	Menu.AddMenuEntry(INVTEXT("Delete Lane"), INVTEXT("Removes the lane and its events."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
		FUIAction(FExecuteAction::CreateLambda([WeakModel, Lane]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->RemoveLane(Lane); } }),
			FCanExecuteAction::CreateLambda([WeakModel]() { TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); return M && M->GetClip() && M->GetClip()->Lanes.Num() > 1; })));
	Menu.EndSection();
	return Menu.MakeWidget();
}

TSharedRef<SWidget> SOnyxClipTimelineTracks::MakeEventMenu(UOnyxClipEvent* Event)
{
	TWeakPtr<FOnyxClipTimelineModel> WeakModel = Model;
	const int32 Count = Model->GetSelectedEvents().Num();
	FMenuBuilder Menu(true, nullptr);
	Menu.BeginSection(NAME_None, Count > 1 ? FText::Format(INVTEXT("{0} events"), Count)
		: FText::Format(INVTEXT("{0} · {1}"), Event->GetLabel(), Event->GetClass()->GetDisplayNameText()));
	Menu.AddMenuEntry(INVTEXT("Copy"), INVTEXT("Ctrl+C"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
		FUIAction(FExecuteAction::CreateLambda([WeakModel]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->CopySelection(); } })));
	Menu.AddMenuEntry(INVTEXT("Duplicate"), INVTEXT("A copy one frame later (Ctrl+D)."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Duplicate"),
		FUIAction(FExecuteAction::CreateLambda([WeakModel]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->DuplicateSelection(); } })));
	Menu.AddMenuEntry(INVTEXT("Delete"), INVTEXT("Delete"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Delete"),
		FUIAction(FExecuteAction::CreateLambda([WeakModel]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->DeleteSelection(); } })));
	if (Count == 1)
	{
		// Exact placement, to the millisecond.
		TWeakObjectPtr<UOnyxClipEvent> WeakEvent = Event;
		Menu.AddWidget(SNew(SBox).WidthOverride(110.f)
			[
				SNew(SNumericEntryBox<int32>)
				.MinValue(0).AllowSpin(true).MinSliderValue(0).MaxSliderValue_Lambda([WeakModel]() { TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); return TOptional<int32>(M ? FMath::RoundToInt(M->GetLength() * 1000.f) : 1000); })
				.Value_Lambda([WeakEvent]() { return WeakEvent.IsValid() ? TOptional<int32>(FMath::RoundToInt(WeakEvent->Time * 1000.f)) : TOptional<int32>(); })
				.OnValueCommitted_Lambda([WeakModel, WeakEvent](int32 Milliseconds, ETextCommit::Type)
				{
					if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); M && WeakEvent.IsValid())
					{
						M->SetEventTime(WeakEvent.Get(), Milliseconds / 1000.f);
					}
				})
			], INVTEXT("Time (ms)"));
		if (UOnyxClipWindow* Window = Cast<UOnyxClipWindow>(Event))
		{
			TWeakObjectPtr<UOnyxClipWindow> WeakWindow = Window;
			Menu.AddWidget(SNew(SBox).WidthOverride(110.f)
				[
					SNew(SNumericEntryBox<int32>)
					.MinValue(10)
					.Value_Lambda([WeakWindow]() { return WeakWindow.IsValid() ? TOptional<int32>(FMath::RoundToInt(WeakWindow->Length * 1000.f)) : TOptional<int32>(); })
					.OnValueCommitted_Lambda([WeakModel, WeakWindow](int32 Milliseconds, ETextCommit::Type)
					{
						if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin(); M && WeakWindow.IsValid())
						{
							M->SetWindowLength(WeakWindow.Get(), Milliseconds / 1000.f);
						}
					})
				], INVTEXT("Length (ms)"));
		}
	}
	Menu.AddSubMenu(INVTEXT("Move to Lane"), FText::GetEmpty(), FNewMenuDelegate::CreateLambda([WeakModel](FMenuBuilder& Sub)
	{
		TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin();
		const UOnyxClip* Clip = M ? M->GetClip() : nullptr;
		for (int32 Lane = 0; Clip && Lane < Clip->Lanes.Num(); ++Lane)
		{
			Sub.AddMenuEntry(FText::FromName(Clip->Lanes[Lane].Name), FText::GetEmpty(), FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([WeakModel, Lane]() { if (TSharedPtr<FOnyxClipTimelineModel> Pinned = WeakModel.Pin()) { Pinned->MoveEventsToLane(Lane); } })));
		}
	}));
	Menu.EndSection();
	if (UBlueprint* Blueprint = Cast<UBlueprint>(Event->GetClass()->ClassGeneratedBy))
	{
		Menu.BeginSection(NAME_None, INVTEXT("Blueprint"));
		TWeakObjectPtr<UBlueprint> WeakBlueprint = Blueprint;
		Menu.AddMenuEntry(INVTEXT("Open Blueprint"), INVTEXT("Edit what this kind of event does."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
			FUIAction(FExecuteAction::CreateLambda([WeakBlueprint]()
			{
				if (GEditor && WeakBlueprint.IsValid())
				{
					GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(WeakBlueprint.Get());
				}
			})));
		Menu.EndSection();
	}
	return Menu.MakeWidget();
}

TSharedRef<SWidget> SOnyxClipTimelineTracks::MakeFrameMenu(int32 KeyFrame)
{
	TWeakPtr<FOnyxClipTimelineModel> WeakModel = Model;
	FMenuBuilder Menu(true, nullptr);
	Menu.BeginSection(NAME_None, FText::Format(INVTEXT("Frame {0}"), KeyFrame + 1));
	Menu.AddMenuEntry(INVTEXT("Duplicate"), INVTEXT("Insert a copy right after it (Ctrl+D)."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Duplicate"),
		FUIAction(FExecuteAction::CreateLambda([WeakModel, KeyFrame]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->DuplicateFrame(KeyFrame); } })));
	Menu.AddMenuEntry(INVTEXT("Delete"), INVTEXT("Remove the frame (Delete)."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Delete"),
		FUIAction(FExecuteAction::CreateLambda([WeakModel, KeyFrame]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->DeleteFrame(KeyFrame); } })));
	Menu.EndSection();
	Menu.BeginSection(NAME_None, INVTEXT("Hold for"));
	for (int32 Hold = 1; Hold <= 8; ++Hold)
	{
		Menu.AddMenuEntry(FText::Format(INVTEXT("{0} {0}|plural(one=frame,other=frames)"), Hold), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([WeakModel, KeyFrame, Hold]() { if (TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin()) { M->SetFrameHold(KeyFrame, Hold); } }),
				FCanExecuteAction(), FIsActionChecked::CreateLambda([WeakModel, KeyFrame, Hold]()
				{
					TSharedPtr<FOnyxClipTimelineModel> M = WeakModel.Pin();
					const UPaperFlipbook* Flipbook = M ? M->GetFlipbook() : nullptr;
					return Flipbook && Flipbook->IsValidKeyFrameIndex(KeyFrame) && Flipbook->GetKeyFrameChecked(KeyFrame).FrameRun == Hold;
				})), NAME_None, EUserInterfaceActionType::RadioButton);
	}
	Menu.EndSection();
	return Menu.MakeWidget();
}
