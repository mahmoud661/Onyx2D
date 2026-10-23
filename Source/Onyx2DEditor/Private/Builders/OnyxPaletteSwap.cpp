// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxPaletteSwap.h"
#include "Builders/OnyxAssetUtils.h"
#include "Engine/Texture2D.h"

void FOnyxPaletteSwap::Add(const FColor& From, const FColor& To)
{
	Pairs.Emplace(From, To);
	Cache.Reset();
}

void FOnyxPaletteSwap::Add(const TCHAR* From, const TCHAR* To)
{
	Add(FColor::FromHex(From), FColor::FromHex(To));
}

FColor FOnyxPaletteSwap::Map(const FColor& Color) const
{
	if (Color.A == 0 || Pairs.IsEmpty())
	{
		return Color;
	}
	const uint32 Key = Color.ToPackedARGB() | 0xFF000000u;
	if (const FColor* Known = Cache.Find(Key))
	{
		return FColor(Known->R, Known->G, Known->B, Color.A);
	}
	// The listed colour nearest to this one (an exact match is nearest of all).
	int32 Best = 0;
	int32 BestDistance = MAX_int32;
	for (int32 Index = 0; Index < Pairs.Num(); ++Index)
	{
		const FColor& From = Pairs[Index].Key;
		const int32 Distance = FMath::Square(int32(From.R) - Color.R) + FMath::Square(int32(From.G) - Color.G) + FMath::Square(int32(From.B) - Color.B);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Index;
		}
	}
	const FColor& To = Pairs[Best].Value;
	Cache.Add(Key, To);
	return FColor(To.R, To.G, To.B, Color.A);
}

void FOnyxPaletteSwap::Apply(TArray<FColor>& Pixels) const
{
	for (FColor& Pixel : Pixels)
	{
		Pixel = Map(Pixel);
	}
}

int32 FOnyxPaletteSwap::Apply(UTexture2D* Texture) const
{
	if (!Texture || !Texture->Source.IsValid() || Texture->Source.GetFormat() != TSF_BGRA8 || Pairs.IsEmpty())
	{
		return 0;
	}
	Texture->Modify();
	FTextureSource& Source = Texture->Source;
	const int64 Count = int64(Source.GetSizeX()) * Source.GetSizeY();
	FColor* Pixels = reinterpret_cast<FColor*>(Source.LockMip(0)); // FColor is laid out B G R A
	if (!Pixels)
	{
		return 0;
	}
	int32 Changed = 0;
	for (int64 Index = 0; Index < Count; ++Index)
	{
		const FColor Mapped = Map(Pixels[Index]);
		if (Mapped != Pixels[Index])
		{
			Pixels[Index] = Mapped;
			++Changed;
		}
	}
	Source.UnlockMip(0);
	Texture->PostEditChange();
	Texture->MarkPackageDirty();
	FOnyxAssetUtils::SavePackages({ Texture->GetPackage() });
	return Changed;
}
