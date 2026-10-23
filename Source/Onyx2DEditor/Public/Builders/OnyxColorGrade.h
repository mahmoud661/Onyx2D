// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/**
 * Re-themes pixel art in place with a gradient map: each pixel's lightness picks a colour on a ramp, so a sunset pack
 * becomes a rainy night (or any palette) while every shape and shade stays as the artist drew it. A little of the
 * original colour can be kept, far layers can be pulled into a haze, and bright warm pixels (flames, lit windows) become
 * a glow colour instead of joining the ramp.
 */
struct ONYX2DEDITOR_API FOnyxColorGrade
{
	struct FStop
	{
		float Position;
		/** sRGB, as painted. */
		FColor Color;
	};

	/** Dark to light. */
	TArray<FStop> Ramp;
	/** Lightness x Exposure + Lift picks the ramp position: lower Exposure darkens (a sky), higher brightens (props). */
	float Exposure = 1.25f;
	float Lift = 0.f;
	/** How much of each pixel's own hue survives on top of the ramp (0 = pure ramp). */
	float Chroma = 0.18f;
	/** Pulls the result towards HazeColor (0-1): distance on far layers. */
	float Haze = 0.f;
	FLinearColor HazeColor = FLinearColor(0.36f, 0.5f, 0.66f);
	/** Bright, saturated orange-to-yellow pixels become GlowColor (the brighter the pixel, the closer to it). */
	bool bGlow = true;
	FLinearColor GlowColor = FLinearColor(0.95f, 0.98f, 1.f);

	/** Deep navy to pale ice blue: a rainy night. */
	static FOnyxColorGrade RainyNight();

	/** One pixel (sRGB 0-1 in and out); glow pixels are left as they are when bGlow is off. */
	FLinearColor GradePixel(const FLinearColor& Painted) const;

	/** Grades a texture's source pixels (BGRA8, like every imported PNG) and rebuilds it. False when it can't read them. */
	bool Apply(UTexture2D* Texture) const;
};
