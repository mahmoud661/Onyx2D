// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/**
 * Re-colours pixel art exactly, colour for colour: each listed colour becomes its partner, and any other colour takes the
 * partner of the listed colour nearest to it, so the shades a list forgets follow their family. Alpha is kept. Made for
 * character templates drawn in flat colours per body part, and for palette variants of the same art.
 */
struct ONYX2DEDITOR_API FOnyxPaletteSwap
{
	void Add(const FColor& From, const FColor& To);
	/** Adds a pair written as hex ("5FCDE4" -> "F2F4F8"). */
	void Add(const TCHAR* From, const TCHAR* To);
	bool IsEmpty() const { return Pairs.IsEmpty(); }
	FColor Map(const FColor& Color) const;
	void Apply(TArray<FColor>& Pixels) const;
	/** Re-colours a texture's source pixels (8-bit BGRA); returns how many pixels changed. */
	int32 Apply(UTexture2D* Texture) const;

private:
	TArray<TPair<FColor, FColor>> Pairs;
	mutable TMap<uint32, FColor> Cache;
};
