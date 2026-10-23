// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FOnyxPaletteSwap;

/** A small canvas of sRGB pixels for pixel art drawn or reworked by code, transparent to begin with. */
struct ONYX2DEDITOR_API FOnyxPixelCanvas
{
	FIntPoint Size;
	TArray<FColor> Pixels;

	FOnyxPixelCanvas() : Size(0, 0) {}
	FOnyxPixelCanvas(int32 Width, int32 Height);

	bool IsInside(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < Size.X && Y < Size.Y; }
	void Set(int32 X, int32 Y, const FColor& Color);
	FColor Get(int32 X, int32 Y) const;
	/** A one-pixel outline in every empty pixel touching a painted one (sides only: rounder shapes). */
	void Outline(const FColor& Color);
	/** Frames side by side in one strip. */
	static FOnyxPixelCanvas Strip(const TArray<FOnyxPixelCanvas>& Frames);

	/** Reads a PNG (or any image the engine reads) from disk. */
	static bool Load(const FString& File, FOnyxPixelCanvas& Out);
	/** A part of the canvas (outside it: transparent). */
	FOnyxPixelCanvas Crop(const FIntRect& Rect) const;
	/** Copies Source onto this canvas with its top-left at At (its transparent pixels too). */
	void Paste(const FOnyxPixelCanvas& Source, FIntPoint At);
	/** Re-colours every pixel. */
	void Recolour(const FOnyxPaletteSwap& Swap);
	/** Rows of transparent pixels under the lowest visible one (the whole height when empty). */
	int32 EmptyRowsBelow() const;
	/**
	 * The body: the run of dense columns around the densest one, from its top to the lowest pixel under it. Blades, spears,
	 * trails and loose effects are thin, so they are left out. Empty when nothing is painted.
	 */
	FIntRect BodyBounds() const;
	/** Every visible pixel of Other laid over this one (both the same size): the silhouettes of many frames at once. */
	void Overlay(const FOnyxPixelCanvas& Other);
};
