// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/** Reads a texture's source pixels to guess sprite-sheet layout and visible bounds. */
class FOnyxImageAnalysis
{
public:
	/** Alpha channel of the texture source (BGRA8 / G8 only). */
	static bool ReadAlpha(UTexture2D* Texture, TArray<uint8>& OutAlpha, FIntPoint& OutSize);

	/**
	 * Guesses the frame size of a grid sheet: the smallest cell size that divides the image
	 * and never cuts through visible pixels, widened when every other cell would be empty.
	 * Unset when no grid can be found (unreadable source, opaque background).
	 */
	static TOptional<FIntPoint> DetectFrameSize(UTexture2D* Texture);

	/** Same as above, on an alpha channel (one byte per pixel, rows top to bottom). */
	static TOptional<FIntPoint> DetectFrameSize(const TArray<uint8>& Alpha, FIntPoint Size);

	/** Fraction (0..1) of visible pixels inside a pixel rectangle; -1 when the source can't be read. */
	static float GetCoverage(UTexture2D* Texture, const FIntRect& Rect);

	/** Union of the visible pixels of every frame, relative to each frame's top-left corner. */
	static TOptional<FIntRect> VisibleBounds(UTexture2D* Texture, const TArray<FIntRect>& Frames);
};
