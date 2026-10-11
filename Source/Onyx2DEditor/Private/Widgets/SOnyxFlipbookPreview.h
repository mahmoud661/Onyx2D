// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"

class SImage;
class UPaperFlipbook;
class UTexture2D;

/** One frame to preview: a pixel rectangle of a texture. */
struct FOnyxPreviewFrame
{
	TObjectPtr<UTexture2D> Texture = nullptr;
	FIntRect Rect;
};

/** Animated, crisp (nearest filtered) preview of a flipbook or of raw sprite-sheet frames. */
class SOnyxFlipbookPreview : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SOnyxFlipbookPreview) : _Height(220.f) {}
		SLATE_ARGUMENT(float, Height)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetFrames(const TArray<FOnyxPreviewFrame>& InFrames, float InFramesPerSecond);
	void SetFlipbook(const UPaperFlipbook* Flipbook, float PlayRate = 1.f);
	void Clear();

	void SetPlaying(bool bInPlaying) { bPlaying = bInPlaying; }
	bool IsPlaying() const { return bPlaying; }
	void SetFrame(int32 Index);
	int32 GetFrame() const { return CurrentFrame; }
	int32 GetFrameCount() const { return Frames.Num(); }

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SOnyxFlipbookPreview"); }

private:
	EActiveTimerReturnType Advance(double CurrentTime, float DeltaTime);
	void UpdateBrush();

	TArray<FOnyxPreviewFrame> Frames;
	float FramesPerSecond = 10.f;
	float Accumulator = 0.f;
	int32 CurrentFrame = 0;
	bool bPlaying = true;
	FSlateBrush Brush;
	TSharedPtr<SImage> Image;
};
