// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxPixelLightSet.h"
#include "PaperSprite.h"

bool FOnyxPixelLightRecipe::Matches(const FColor& Pixel) const
{
	if (Pixel.A < 128)
	{
		return false;
	}
	for (const FColor& Wanted : Colors)
	{
		const float Distance = FMath::Max3(FMath::Abs(int32(Pixel.R) - int32(Wanted.R)), FMath::Abs(int32(Pixel.G) - int32(Wanted.G)),
			FMath::Abs(int32(Pixel.B) - int32(Wanted.B))) / 255.f;
		if (Distance <= Tolerance + 1e-4f)
		{
			return true;
		}
	}
	return MinBrightness > 0.f && FMath::Max3(Pixel.R, Pixel.G, Pixel.B) / 255.f >= MinBrightness;
}

const FOnyxPixelLightFrame* UOnyxPixelLightSet::FindFrame(const UPaperSprite* Sprite) const
{
	if (!Sprite)
	{
		return nullptr;
	}
	if (LookupFrames != Frames.Num())
	{
		Lookup.Reset();
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			if (Frames[Index].Sprite)
			{
				Lookup.Add(TObjectKey<UPaperSprite>(Frames[Index].Sprite.Get()), Index);
			}
		}
		LookupFrames = Frames.Num();
	}
	const int32* Index = Lookup.Find(TObjectKey<UPaperSprite>(Sprite));
	return Index && Frames.IsValidIndex(*Index) ? &Frames[*Index] : nullptr;
}
