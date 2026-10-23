// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Builders/OnyxPixelCanvas.h"
#include "Core/Onyx2DTypes.h"

/** What a pixel of part-coloured art (one flat colour per body part, like the character template) is. */
enum class EOnyxOutfitPart : uint8
{
	None,
	Head,
	HeadShade,
	Torso,
	TorsoShade,
	TorsoDark,
	RightArmUpper,
	RightArmLower,
	LeftArmUpper,
	LeftArmLower,
	RightLegDark,
	RightLegLight,
	LeftLegDark,
	LeftLegLight,
	Steel
};

enum class EOnyxHairStyle : uint8
{
	Short,
	Long,
	Bald
};

enum class EOnyxHat : uint8
{
	None,
	/** A wide straw brim. */
	Straw,
	/** A cap with a peak. */
	Cap
};

/**
 * How to dress part-coloured frames as a person: the head becomes hair and a face (eyes looking the frame's way, turned
 * with the head when it tumbles), the torso a shirt or an open jacket with a belt, each arm a sleeve ending in a hand,
 * each leg trousers ending in a boot, the sword steel with a grip, and a dark outline goes round the whole figure. Hands
 * and boots are the arm and leg pixels furthest from the body, so they stay right through rolls and falls; the art's own
 * two tones per part become the clothes' light and shade. Other pixels (a ground shadow, a slash) are left as they are.
 */
struct ONYX2DEDITOR_API FOnyxOutfit
{
	/** The source's part colours (the character template's by default) and what each one is. */
	struct FPartColor { FColor Color; EOnyxOutfitPart Part; };
	TArray<FPartColor> PartColors;

	FColor Skin = FColor(240, 196, 160), SkinShade = FColor(208, 152, 120);
	FColor Hair = FColor(98, 60, 42), HairShade = FColor(66, 40, 30), HairLight = FColor(140, 92, 60);
	EOnyxHairStyle HairStyle = EOnyxHairStyle::Short;
	EOnyxHat Hat = EOnyxHat::None;
	FColor HatColor = FColor(226, 190, 102), HatShade = FColor(176, 136, 66);
	FColor Shirt = FColor(60, 108, 172), ShirtShade = FColor(44, 80, 136), ShirtDark = FColor(32, 58, 100);
	/** An open jacket's shirt underneath, and the collar. */
	bool bOpenJacket = true;
	FColor ShirtInner = FColor(230, 224, 208);
	FColor Sleeve = FColor(60, 108, 172), SleeveShade = FColor(44, 80, 136);
	FColor Trousers = FColor(62, 58, 80), TrousersShade = FColor(44, 40, 58);
	FColor Boots = FColor(108, 70, 46), BootsShade = FColor(74, 48, 32);
	FColor Belt = FColor(90, 58, 38), Buckle = FColor(228, 194, 96);
	FColor Blade = FColor(224, 234, 242), BladeEdge = FColor(150, 170, 190), Grip = FColor(112, 74, 48);
	FColor Eyes = FColor(30, 28, 42);
	bool bBeard = false;
	FColor OutlineColor = FColor(26, 24, 36);

	/** The character template's part colours, a brown-haired adventurer in a blue jacket. */
	FOnyxOutfit();

	/** Dresses one frame of a character looking Direction (Down, DownSide, Side: looking right, UpSide, Up). */
	FOnyxPixelCanvas Dress(const FOnyxPixelCanvas& Source, EOnyxDirection Direction) const;

	/** Dresses a strip of square frames (as tall as they are wide), all looking Direction. */
	FOnyxPixelCanvas DressStrip(const FOnyxPixelCanvas& Strip, EOnyxDirection Direction) const;

	/** Which part a colour is (None: not one of the part colours). */
	EOnyxOutfitPart Classify(const FColor& Color) const;

	/** How far (summed over R, G and B) a colour may be from a part colour and still count as it. */
	int32 ColorTolerance = 12;

	/** The head, drawn upright in a box Width x Height (Mask: which pixels are head; Shade: the art's shaded side), Margin pixels round it for hair and a hat. */
	FOnyxPixelCanvas DrawHead(const TArray<uint8>& Mask, const TArray<uint8>& Shade, int32 Width, int32 Height, int32 Margin, EOnyxDirection Direction) const;
};
