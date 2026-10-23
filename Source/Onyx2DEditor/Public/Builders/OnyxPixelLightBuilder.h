// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Lighting/OnyxPixelLightSet.h"

class UOnyxMoveset;
class UPaperFlipbook;
class UPaperSprite;

/**
 * Makes Pixel Lights: light that comes from chosen pixels of the art. For every frame of the flipbooks, the pixels the recipe
 * names (by colour, by brightness, or painted white on a glow mask beside the art) are taken, spread softly by its Spread and laid in one light sheet with where each sits
 * around its sprite's pivot (a Pixel Light Set, played by an Onyx Pixel Light component). The same pixels can make the
 * sprites' glow masks, so Glowing Pixels lights up only them.
 */
class ONYX2DEDITOR_API FOnyxPixelLightBuilder
{
public:
	/** A Pixel Light Set (and its sheet texture) for these flipbooks, made anew as Folder/Name. */
	static UOnyxPixelLightSet* Build(TConstArrayView<UPaperFlipbook*> Flipbooks, const FOnyxPixelLightRecipe& Recipe, const FString& Folder, const FString& Name,
		FText& OutError);

	/** The same for every clip of a moveset. */
	static UOnyxPixelLightSet* BuildForMoveset(const UOnyxMoveset* Moveset, const FOnyxPixelLightRecipe& Recipe, const FString& Folder, const FString& Name,
		FText& OutError);

	/** The same, saved where the Moveset Editor keeps a moveset's Pixel Light, so it opens on this recipe. */
	static UOnyxPixelLightSet* BuildForMoveset(const UOnyxMoveset* Moveset, const FOnyxPixelLightRecipe& Recipe, FText& OutError);

	/** Where the Moveset Editor keeps a moveset's Pixel Light: beside it, named "PL_<moveset without MS_>" (MS_Fox: PL_Fox). */
	static void GetMovesetSetPath(const UOnyxMoveset* Moveset, FString& OutFolder, FString& OutName);

	/** Makes a set again from its own recipe and the flipbooks it was made from. */
	static bool Rebuild(UOnyxPixelLightSet* Set, FText& OutError);

	/**
	 * Glow masks: for each picture the flipbooks' frames come from, a mask of the pixels the recipe names, given to the frames
	 * as their glow mask (Additional Textures slot 1). Glowing Pixels with a mask (Glow From Mask) then lights only those.
	 * With a painted mask (bFromPaintedMask) the one beside each picture is given as it is. Returns how many sprites got one.
	 */
	static int32 ApplyGlowMasks(TConstArrayView<UPaperFlipbook*> Flipbooks, const FOnyxPixelLightRecipe& Recipe, FText& OutError);

	/** The colours in a frame's pixels, the most used first: what to choose from. */
	static TArray<TPair<FColor, int32>> CountColours(const UPaperSprite* Sprite);

	/** The glow mask beside a picture ("<texture>_GlowMask" in its folder), painted or made here before; null when there is none. */
	static UTexture2D* FindGlowMask(const UTexture2D* Picture);

	/** Every flipbook of a moveset's clips. */
	static TArray<UPaperFlipbook*> FlipbooksOf(const UOnyxMoveset* Moveset);
};
