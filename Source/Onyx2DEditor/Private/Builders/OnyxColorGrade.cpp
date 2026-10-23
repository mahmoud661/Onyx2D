// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxColorGrade.h"
#include "Engine/Texture2D.h"

FOnyxColorGrade FOnyxColorGrade::RainyNight()
{
	FOnyxColorGrade Grade;
	Grade.Ramp = {
		{ 0.f, FColor(0x05, 0x07, 0x0f) },
		{ 0.15f, FColor(0x0b, 0x12, 0x26) },
		{ 0.35f, FColor(0x18, 0x28, 0x4a) },
		{ 0.55f, FColor(0x2f, 0x4d, 0x76) },
		{ 0.75f, FColor(0x5b, 0x84, 0xab) },
		{ 0.9f, FColor(0x9c, 0xc3, 0xdc) },
		{ 1.f, FColor(0xdf, 0xf1, 0xfa) },
	};
	return Grade;
}

FLinearColor FOnyxColorGrade::GradePixel(const FLinearColor& Painted) const
{
	const float R = Painted.R;
	const float G = Painted.G;
	const float B = Painted.B;
	// Lightness as painted (sRGB values, the way the artist judged them).
	const float Lightness = 0.299f * R + 0.587f * G + 0.114f * B;
	const float Max = FMath::Max3(R, G, B);
	const float Min = FMath::Min3(R, G, B);
	const float Saturation = Max > 0.f ? (Max - Min) / Max : 0.f;
	float Hue = 0.f;
	if (Max > Min)
	{
		if (Max == R)
		{
			Hue = 60.f * FMath::Fmod((G - B) / (Max - Min), 6.f);
		}
		else if (Max == G)
		{
			Hue = 60.f * ((B - R) / (Max - Min) + 2.f);
		}
		else
		{
			Hue = 60.f * ((R - G) / (Max - Min) + 4.f);
		}
		Hue = Hue < 0.f ? Hue + 360.f : Hue;
	}

	// A flame or a lit window: brighter than anything on the ramp.
	if (bGlow && Hue >= 20.f && Hue <= 65.f && Saturation > 0.5f && Max > 0.7f)
	{
		const float Heat = FMath::Min((Max - 0.7f) / 0.2f, 1.f);
		return FMath::Lerp(GlowColor * FLinearColor(0.84f, 0.95f, 1.f), GlowColor, Heat).CopyWithNewOpacity(Painted.A);
	}

	FLinearColor Result = FLinearColor::Black;
	if (Ramp.Num() > 0)
	{
		const float T = FMath::Clamp(Lightness * Exposure + Lift, 0.f, 1.f);
		int32 Index = 0;
		while (Index < Ramp.Num() - 2 && T > Ramp[Index + 1].Position)
		{
			++Index;
		}
		const FStop& From = Ramp[Index];
		const FStop& To = Ramp[FMath::Min(Index + 1, Ramp.Num() - 1)];
		const float Span = To.Position - From.Position;
		const float Alpha = Span > KINDA_SMALL_NUMBER ? FMath::Clamp((T - From.Position) / Span, 0.f, 1.f) : 0.f;
		auto Channel = [](uint8 Value) { return Value / 255.f; };
		Result = FLinearColor(FMath::Lerp(Channel(From.Color.R), Channel(To.Color.R), Alpha), FMath::Lerp(Channel(From.Color.G), Channel(To.Color.G), Alpha),
			FMath::Lerp(Channel(From.Color.B), Channel(To.Color.B), Alpha));
	}
	// The pixel's own hue, a little, so wood still reads as wood and stone as stone.
	Result += FLinearColor(R - Lightness, G - Lightness, B - Lightness, 0.f) * Chroma;
	Result = FMath::Lerp(Result, HazeColor, Haze);
	return FLinearColor(FMath::Clamp(Result.R, 0.f, 1.f), FMath::Clamp(Result.G, 0.f, 1.f), FMath::Clamp(Result.B, 0.f, 1.f), Painted.A);
}

bool FOnyxColorGrade::Apply(UTexture2D* Texture) const
{
	if (!Texture || !Texture->Source.IsValid() || Texture->Source.GetFormat() != TSF_BGRA8)
	{
		return false;
	}
	Texture->Modify();
	FTextureSource& Source = Texture->Source;
	const int64 Count = int64(Source.GetSizeX()) * Source.GetSizeY();
	uint8* Pixels = Source.LockMip(0);
	if (!Pixels)
	{
		return false;
	}
	for (int64 Index = 0; Index < Count; ++Index)
	{
		uint8* Pixel = Pixels + Index * 4; // B G R A
		if (Pixel[3] == 0)
		{
			continue;
		}
		const FLinearColor Graded = GradePixel(FLinearColor(Pixel[2] / 255.f, Pixel[1] / 255.f, Pixel[0] / 255.f, Pixel[3] / 255.f));
		Pixel[2] = uint8(FMath::RoundToInt(Graded.R * 255.f));
		Pixel[1] = uint8(FMath::RoundToInt(Graded.G * 255.f));
		Pixel[0] = uint8(FMath::RoundToInt(Graded.B * 255.f));
	}
	Source.UnlockMip(0);
	Texture->PostEditChange();
	Texture->MarkPackageDirty();
	return true;
}
