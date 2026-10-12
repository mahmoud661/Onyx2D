// Copyright Onyx2D. All Rights Reserved.

#include "Widgets/SOnyxFlipbookPreview.h"
#include "Engine/Texture2D.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "ToolsWindow/OnyxToolWidgets.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SOnyxFlipbookPreview::Construct(const FArguments& InArgs)
{
	Brush.DrawAs = ESlateBrushDrawType::NoDrawType;

	ChildSlot
	[
		SNew(SBox)
		.HeightOverride(InArgs._Height > 0.f ? FOptionalSize(InArgs._Height) : FOptionalSize())
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.025f, 0.028f, 0.035f))
			.Padding(12.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SScaleBox)
					.Stretch(EStretch::ScaleToFit)
					[
						SAssignNew(Image, SImage).Image(&Brush)
					]
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(InArgs._EmptyText)
					.Font(OnyxTools::Font(10))
					.ColorAndOpacity(OnyxTools::Muted())
					.Visibility_Lambda([this]() { return Frames.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				]
			]
		]
	];

	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SOnyxFlipbookPreview::Advance));
}

void SOnyxFlipbookPreview::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FOnyxPreviewFrame& Frame : Frames)
	{
		Collector.AddReferencedObject(Frame.Texture);
	}
}

void SOnyxFlipbookPreview::SetFrames(const TArray<FOnyxPreviewFrame>& InFrames, float InFramesPerSecond)
{
	Frames = InFrames;
	FramesPerSecond = FMath::Max(InFramesPerSecond, 0.1f);
	Accumulator = 0.f;
	CurrentFrame = 0;
	UpdateBrush();
}

void SOnyxFlipbookPreview::SetFlipbook(const UPaperFlipbook* Flipbook, float PlayRate)
{
	TArray<FOnyxPreviewFrame> NewFrames;
	if (Flipbook)
	{
		for (int32 KeyIndex = 0; KeyIndex < Flipbook->GetNumKeyFrames(); ++KeyIndex)
		{
			const FPaperFlipbookKeyFrame& Key = Flipbook->GetKeyFrameChecked(KeyIndex);
			if (!Key.Sprite || !Key.Sprite->GetSourceTexture())
			{
				continue;
			}
			FOnyxPreviewFrame Frame;
			Frame.Texture = Key.Sprite->GetSourceTexture();
			const FIntPoint Min(FMath::RoundToInt(Key.Sprite->GetSourceUV().X), FMath::RoundToInt(Key.Sprite->GetSourceUV().Y));
			const FIntPoint Size(FMath::RoundToInt(Key.Sprite->GetSourceSize().X), FMath::RoundToInt(Key.Sprite->GetSourceSize().Y));
			Frame.Rect = FIntRect(Min, Min + Size);
			for (int32 Run = 0; Run < FMath::Max(Key.FrameRun, 1); ++Run)
			{
				NewFrames.Add(Frame);
			}
		}
	}
	SetFrames(NewFrames, Flipbook ? Flipbook->GetFramesPerSecond() * PlayRate : 10.f);
}

void SOnyxFlipbookPreview::Clear()
{
	SetFrames({}, 10.f);
}

void SOnyxFlipbookPreview::SetFrame(int32 Index)
{
	if (Frames.Num() > 0)
	{
		CurrentFrame = FMath::Clamp(Index, 0, Frames.Num() - 1);
		UpdateBrush();
	}
}

EActiveTimerReturnType SOnyxFlipbookPreview::Advance(double CurrentTime, float DeltaTime)
{
	if (bPlaying && Frames.Num() > 1)
	{
		Accumulator += DeltaTime;
		const float FrameTime = 1.f / FramesPerSecond;
		bool bChanged = false;
		while (Accumulator >= FrameTime)
		{
			Accumulator -= FrameTime;
			CurrentFrame = (CurrentFrame + 1) % Frames.Num();
			bChanged = true;
		}
		if (bChanged)
		{
			UpdateBrush();
		}
	}
	return EActiveTimerReturnType::Continue;
}

void SOnyxFlipbookPreview::UpdateBrush()
{
	if (!Frames.IsValidIndex(CurrentFrame) || !Frames[CurrentFrame].Texture)
	{
		Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
		Brush.SetResourceObject(nullptr);
		return;
	}

	const FOnyxPreviewFrame& Frame = Frames[CurrentFrame];
	const FIntPoint TextureSize = Frame.Texture->GetImportedSize();
	if (TextureSize.X <= 0 || TextureSize.Y <= 0)
	{
		return;
	}
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetResourceObject(Frame.Texture);
	Brush.ImageSize = FVector2D(Frame.Rect.Size());
	Brush.SetUVRegion(FBox2f(
		FVector2f(float(Frame.Rect.Min.X) / TextureSize.X, float(Frame.Rect.Min.Y) / TextureSize.Y),
		FVector2f(float(Frame.Rect.Max.X) / TextureSize.X, float(Frame.Rect.Max.Y) / TextureSize.Y)));
	if (Image.IsValid())
	{
		Image->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	}
}
