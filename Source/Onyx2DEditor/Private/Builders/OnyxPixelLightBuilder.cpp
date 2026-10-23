// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxPixelLightBuilder.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxMoveset.h"
#include "Builders/OnyxAssetUtils.h"
#include "Engine/Texture2D.h"
#include "ImageCore.h"
#include "Misc/PackageName.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"

namespace OnyxPixelLight
{
	/** A picture's pixels as painted (sRGB), read once from its source. */
	struct FPicture
	{
		FIntPoint Size = FIntPoint::ZeroValue;
		TArray64<FColor> Pixels;

		FColor At(int32 X, int32 Y) const
		{
			return X >= 0 && Y >= 0 && X < Size.X && Y < Size.Y ? Pixels[int64(Y) * Size.X + X] : FColor(0, 0, 0, 0);
		}
	};

	class FPictures
	{
	public:
		const FPicture* Get(UTexture2D* Texture)
		{
			if (!Texture)
			{
				return nullptr;
			}
			if (const FPicture* Known = Cache.Find(Texture))
			{
				return Known->Size.X > 0 ? Known : nullptr;
			}
			FPicture& Made = Cache.Add(Texture);
			FImage Image;
			if (Texture->Source.IsValid() && Texture->Source.GetMipImage(Image, 0))
			{
				Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
				Made.Size = FIntPoint(Image.SizeX, Image.SizeY);
				Made.Pixels = TArray64<FColor>(Image.AsBGRA8().GetData(), Image.AsBGRA8().Num());
			}
			return Made.Size.X > 0 ? &Made : nullptr;
		}

	private:
		TMap<UTexture2D*, FPicture> Cache;
	};

	/** One frame's light before it is packed: linear colour, Size at the sheet's resolution, and where it sits around the pivot. */
	struct FFrameLight
	{
		UPaperSprite* Sprite = nullptr;
		FIntPoint Size = FIntPoint::ZeroValue;
		/** The spread light, and the lit pixels themselves (the source is the brightest place). */
		TArray<FLinearColor> Light;
		TArray<FLinearColor> Core;
		/** The halo: the lit pixels spread only a little. */
		TArray<FLinearColor> Halo;
		float HaloPeak = 0.f;
		FVector2f LocalMin;
		FVector2f LocalMax;
		float Peak = 0.f;
		FIntPoint At = FIntPoint::ZeroValue;
	};

	/** A soft spread (two passes of a Gaussian, along X then Y) of Source, Width x Height, Radius pixels wide. */
	void Spread(TArray<FLinearColor>& Image, int32 Width, int32 Height, int32 Radius)
	{
		const float Sigma = FMath::Max(Radius / 2.5f, 0.5f);
		TArray<float> Kernel;
		float Total = 0.f;
		for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
		{
			const float Weight = FMath::Exp(-0.5f * FMath::Square(Offset / Sigma));
			Kernel.Add(Weight);
			Total += Weight;
		}
		for (float& Weight : Kernel)
		{
			Weight /= Total;
		}
		TArray<FLinearColor> Pass;
		Pass.SetNumZeroed(Image.Num());
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				FLinearColor Sum(0.f, 0.f, 0.f, 0.f);
				for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
				{
					const int32 Column = X + Offset;
					if (Column >= 0 && Column < Width)
					{
						Sum += Image[Y * Width + Column] * Kernel[Offset + Radius];
					}
				}
				Pass[Y * Width + X] = Sum;
			}
		}
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				FLinearColor Sum(0.f, 0.f, 0.f, 0.f);
				for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
				{
					const int32 Row = Y + Offset;
					if (Row >= 0 && Row < Height)
					{
						Sum += Pass[Row * Width + X] * Kernel[Offset + Radius];
					}
				}
				Image[Y * Width + X] = Sum;
			}
		}
	}

	TArray<UPaperSprite*> SpritesOf(TConstArrayView<UPaperFlipbook*> Flipbooks)
	{
		TArray<UPaperSprite*> Sprites;
		for (const UPaperFlipbook* Flipbook : Flipbooks)
		{
			for (int32 Index = 0; Flipbook && Index < Flipbook->GetNumKeyFrames(); ++Index)
			{
				if (UPaperSprite* Sprite = Flipbook->GetKeyFrameChecked(Index).Sprite)
				{
					Sprites.AddUnique(Sprite);
				}
			}
		}
		return Sprites;
	}
}

TArray<UPaperFlipbook*> FOnyxPixelLightBuilder::FlipbooksOf(const UOnyxMoveset* Moveset)
{
	TArray<UPaperFlipbook*> Flipbooks;
	if (Moveset)
	{
		for (const TPair<FName, TObjectPtr<UOnyxClip>>& Clip : Moveset->Clips)
		{
			if (Clip.Value && Clip.Value->Flipbook)
			{
				Flipbooks.AddUnique(Clip.Value->Flipbook);
			}
		}
	}
	return Flipbooks;
}

UOnyxPixelLightSet* FOnyxPixelLightBuilder::BuildForMoveset(const UOnyxMoveset* Moveset, const FOnyxPixelLightRecipe& Recipe, const FString& Folder, const FString& Name,
	FText& OutError)
{
	return Build(FlipbooksOf(Moveset), Recipe, Folder, Name, OutError);
}

UOnyxPixelLightSet* FOnyxPixelLightBuilder::BuildForMoveset(const UOnyxMoveset* Moveset, const FOnyxPixelLightRecipe& Recipe, FText& OutError)
{
	FString Folder;
	FString Name;
	GetMovesetSetPath(Moveset, Folder, Name);
	return BuildForMoveset(Moveset, Recipe, Folder, Name, OutError);
}

void FOnyxPixelLightBuilder::GetMovesetSetPath(const UOnyxMoveset* Moveset, FString& OutFolder, FString& OutName)
{
	FString Base = Moveset ? Moveset->GetName() : FString(TEXT("Character"));
	Base.RemoveFromStart(TEXT("MS_"));
	OutFolder = Moveset ? FPackageName::GetLongPackagePath(Moveset->GetPackage()->GetName()) : FString(TEXT("/Game/Characters"));
	OutName = TEXT("PL_") + Base;
}

UOnyxPixelLightSet* FOnyxPixelLightBuilder::Build(TConstArrayView<UPaperFlipbook*> Flipbooks, const FOnyxPixelLightRecipe& Recipe, const FString& Folder, const FString& Name,
	FText& OutError)
{
	using namespace OnyxPixelLight;
	if (!Recipe.NamesPixels())
	{
		OutError = INVTEXT("Choose which pixels give light: some colours, a brightness, or a glow mask painted for the art.");
		return nullptr;
	}
	FPictures Pictures;
	TArray<FFrameLight> Frames;
	const int32 Pad = FMath::Clamp(Recipe.Spread, 1, 64);
	// The halo hugs the pixels: a few art pixels of glow past their edge, the way a white-hot edge looks through haze.
	const int32 HaloSpread = FMath::Clamp(FMath::RoundToInt(Pad * 0.3f), 2, 8);
	float HaloPeak = 0.f;
	const float Resolution = FMath::Clamp(Recipe.Resolution, 0.125f, 1.f);
	float Peak = 0.f;
	for (UPaperSprite* Sprite : SpritesOf(Flipbooks))
	{
		const FPicture* Picture = Pictures.Get(Sprite->GetSourceTexture());
		// A painted mask names the pixels itself (it must match the art pixel for pixel); frames without one give no light.
		const FPicture* Painted = Recipe.bFromPaintedMask ? Pictures.Get(FindGlowMask(Sprite->GetSourceTexture())) : nullptr;
		if (!Picture || (Recipe.bFromPaintedMask && (!Painted || Painted->Size != Picture->Size)))
		{
			continue;
		}
		const FIntPoint Origin(FMath::RoundToInt(Sprite->GetSourceUV().X), FMath::RoundToInt(Sprite->GetSourceUV().Y));
		const FIntPoint Extent(FMath::RoundToInt(Sprite->GetSourceSize().X), FMath::RoundToInt(Sprite->GetSourceSize().Y));
		// The frame's lit pixels, with room round them for the light to spread into.
		const int32 Width = Extent.X + 2 * Pad;
		const int32 Height = Extent.Y + 2 * Pad;
		TArray<FLinearColor> Full;
		Full.SetNumZeroed(Width * Height);
		bool bAny = false;
		for (int32 Y = 0; Y < Extent.Y; ++Y)
		{
			for (int32 X = 0; X < Extent.X; ++X)
			{
				const FColor Pixel = Picture->At(Origin.X + X, Origin.Y + Y);
				if (Painted ? Pixel.A >= 128 && Painted->At(Origin.X + X, Origin.Y + Y).R >= 128 : Recipe.Matches(Pixel))
				{
					const FLinearColor Colour = Recipe.bPixelColours ? FLinearColor(Pixel) : FLinearColor::White;
					Full[(Y + Pad) * Width + X + Pad] = Colour * Recipe.Tint;
					bAny = true;
				}
			}
		}
		if (!bAny)
		{
			continue; // this frame gives no light
		}
		// Soft light round the pixels; the pixels themselves kept apart, to sit brightest on top of it.
		const TArray<FLinearColor> Core = Full;
		TArray<FLinearColor> HaloFull = Full;
		Spread(HaloFull, Width, Height, HaloSpread);
		Spread(Full, Width, Height, Pad);
		// The spread's tail still holds a few percent at the frame's edge: fade it out over the last half of the border, so the
		// light never ends in a line where its rectangle does.
		const float Fade = FMath::Max(Pad * 0.5f, 1.f);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const float Edge = FMath::Min(FMath::Min(X, Width - 1 - X), FMath::Min(Y, Height - 1 - Y)) + 0.5f;
				Full[Y * Width + X] *= FMath::SmoothStep(0.f, Fade, Edge);
			}
		}
		// Down to the sheet's resolution.
		FFrameLight& Frame = Frames.AddDefaulted_GetRef();
		Frame.Sprite = Sprite;
		Frame.Size = FIntPoint(FMath::Max(1, FMath::CeilToInt(Width * Resolution)), FMath::Max(1, FMath::CeilToInt(Height * Resolution)));
		Frame.Light.SetNumZeroed(Frame.Size.X * Frame.Size.Y);
		Frame.Core.SetNumZeroed(Frame.Size.X * Frame.Size.Y);
		Frame.Halo.SetNumZeroed(Frame.Size.X * Frame.Size.Y);
		for (int32 Y = 0; Y < Frame.Size.Y; ++Y)
		{
			for (int32 X = 0; X < Frame.Size.X; ++X)
			{
				FLinearColor Sum(0.f, 0.f, 0.f, 0.f);
				FLinearColor CoreSum(0.f, 0.f, 0.f, 0.f);
				FLinearColor HaloSum(0.f, 0.f, 0.f, 0.f);
				int32 Count = 0;
				for (int32 SY = FMath::FloorToInt(Y / Resolution); SY < FMath::Min(Height, FMath::FloorToInt((Y + 1) / Resolution)); ++SY)
				{
					for (int32 SX = FMath::FloorToInt(X / Resolution); SX < FMath::Min(Width, FMath::FloorToInt((X + 1) / Resolution)); ++SX)
					{
						Sum += Full[SY * Width + SX];
						CoreSum += Core[SY * Width + SX];
						HaloSum += HaloFull[SY * Width + SX];
						++Count;
					}
				}
				const int32 Index = Y * Frame.Size.X + X;
				Frame.Light[Index] = Count > 0 ? Sum / float(Count) : FLinearColor::Black;
				Frame.Core[Index] = Count > 0 ? CoreSum / float(Count) : FLinearColor::Black;
				Frame.Halo[Index] = Count > 0 ? HaloSum / float(Count) : FLinearColor::Black;
				Frame.Peak = FMath::Max(Frame.Peak, Frame.Light[Index].GetMax());
				Frame.HaloPeak = FMath::Max(Frame.HaloPeak, Frame.Halo[Index].GetMax());
			}
		}
		Peak = FMath::Max(Peak, Frame.Peak);
		HaloPeak = FMath::Max(HaloPeak, Frame.HaloPeak);
		// Where the frame's light sits around the sprite's pivot (cm; X right, Z up), the art's pixels scaled to the world.
		const FVector2D Pivot = Sprite->GetPivotPosition();
		const float PerCm = FMath::Max(Sprite->GetPixelsPerUnrealUnit(), 0.0001f);
		const float Left = Origin.X - Pad;
		const float Top = Origin.Y - Pad;
		Frame.LocalMin = FVector2f((Left - Pivot.X) / PerCm, (Pivot.Y - (Top + Height)) / PerCm);
		Frame.LocalMax = FVector2f((Left + Width - Pivot.X) / PerCm, (Pivot.Y - Top) / PerCm);
	}
	if (Frames.IsEmpty())
	{
		OutError = Recipe.bFromPaintedMask
			? INVTEXT("No frame's texture has a glow mask painted for it (\"<texture>_GlowMask\" beside it, the same size, white where it gives light).")
			: INVTEXT("No frame has any of those pixels: nothing would give light.");
		return nullptr;
	}

	// One brightness for every frame (the spread light's brightest is near full), so the light doesn't swell and shrink with
	// the art; the lit pixels themselves a little brighter on top.
	// Packed in rows, a pixel of empty space round each so the smooth sampling never borrows a neighbour's light.
	constexpr int32 SheetWidthLimit = 2048;
	constexpr int32 Gutter = 2;
	Frames.Sort([](const FFrameLight& A, const FFrameLight& B) { return A.Size.Y > B.Size.Y; });
	FIntPoint Cursor(Gutter, Gutter);
	int32 RowHeight = 0;
	int32 SheetWidth = 0;
	for (FFrameLight& Frame : Frames)
	{
		if (Cursor.X + Frame.Size.X + Gutter > SheetWidthLimit)
		{
			Cursor = FIntPoint(Gutter, Cursor.Y + RowHeight + Gutter);
			RowHeight = 0;
		}
		Frame.At = Cursor;
		Cursor.X += Frame.Size.X + Gutter;
		RowHeight = FMath::Max(RowHeight, Frame.Size.Y);
		SheetWidth = FMath::Max(SheetWidth, Cursor.X);
	}
	const FIntPoint SheetSize(SheetWidth, Cursor.Y + RowHeight + Gutter);
	TArray<FColor> Sheet;
	Sheet.SetNumZeroed(SheetSize.X * SheetSize.Y);
	TArray<FColor> HaloSheet;
	HaloSheet.SetNumZeroed(SheetSize.X * SheetSize.Y);
	constexpr float CoreBoost = 0.5f;
	const float Scale = 1.f / FMath::Max(Peak, 1e-4f);
	const float HaloScale = 1.f / FMath::Max(HaloPeak, 1e-4f);
	for (const FFrameLight& Frame : Frames)
	{
		for (int32 Y = 0; Y < Frame.Size.Y; ++Y)
		{
			for (int32 X = 0; X < Frame.Size.X; ++X)
			{
				const int32 Index = Y * Frame.Size.X + X;
				const FLinearColor Value = (Frame.Light[Index] * Scale + Frame.Core[Index] * CoreBoost) / (1.f + CoreBoost);
				Sheet[(Frame.At.Y + Y) * SheetSize.X + Frame.At.X + X] = Value.ToFColor(false).WithAlpha(255);
				HaloSheet[(Frame.At.Y + Y) * SheetSize.X + Frame.At.X + X] = (Frame.Halo[Index] * HaloScale).ToFColor(false).WithAlpha(255);
			}
		}
	}

	UTexture2D* Texture = FOnyxAssetUtils::CreateTexture(Folder, TEXT("T_") + Name, Sheet, SheetSize);
	UTexture2D* HaloTexture = FOnyxAssetUtils::CreateTexture(Folder, TEXT("T_") + Name + TEXT("_Halo"), HaloSheet, SheetSize);
	UOnyxPixelLightSet* Set = FOnyxAssetUtils::CreateOrReuse<UOnyxPixelLightSet>(Folder, Name);
	if (!Texture || !HaloTexture || !Set)
	{
		OutError = INVTEXT("Could not create the light sheet or its set.");
		return nullptr;
	}
	// Light, not pixel art: linear, smooth, nothing beyond its edges.
	for (UTexture2D* Made : { Texture, HaloTexture })
	{
		Made->Modify();
		Made->SRGB = false;
		Made->Filter = TF_Bilinear;
		Made->LODGroup = TEXTUREGROUP_World;
		Made->AddressX = TA_Clamp;
		Made->AddressY = TA_Clamp;
		Made->PostEditChange();
	}
	Set->Modify();
	Set->Sheet = Texture;
	Set->HaloSheet = HaloTexture;
	Set->Recipe = Recipe;
	Set->Frames.Reset();
	for (const FFrameLight& Frame : Frames)
	{
		FOnyxPixelLightFrame& Made = Set->Frames.AddDefaulted_GetRef();
		Made.Sprite = Frame.Sprite;
		Made.UVMin = FVector2f(float(Frame.At.X) / SheetSize.X, float(Frame.At.Y) / SheetSize.Y);
		Made.UVMax = FVector2f(float(Frame.At.X + Frame.Size.X) / SheetSize.X, float(Frame.At.Y + Frame.Size.Y) / SheetSize.Y);
		Made.LocalMin = Frame.LocalMin;
		Made.LocalMax = Frame.LocalMax;
	}
	Set->Sources.Reset();
	for (UPaperFlipbook* Flipbook : Flipbooks)
	{
		if (Flipbook)
		{
			Set->Sources.Add(Flipbook);
		}
	}
	Set->MarkPackageDirty();
	FOnyxAssetUtils::SavePackages({ Texture->GetPackage(), HaloTexture->GetPackage(), Set->GetPackage() });
	return Set;
}

bool FOnyxPixelLightBuilder::Rebuild(UOnyxPixelLightSet* Set, FText& OutError)
{
	if (!Set)
	{
		return false;
	}
	TArray<UPaperFlipbook*> Flipbooks;
	for (const TSoftObjectPtr<UPaperFlipbook>& Source : Set->Sources)
	{
		if (UPaperFlipbook* Flipbook = Source.LoadSynchronous())
		{
			Flipbooks.Add(Flipbook);
		}
	}
	const FString Folder = FPackageName::GetLongPackagePath(Set->GetPackage()->GetName());
	return Build(Flipbooks, Set->Recipe, Folder, Set->GetName(), OutError) != nullptr;
}

int32 FOnyxPixelLightBuilder::ApplyGlowMasks(TConstArrayView<UPaperFlipbook*> Flipbooks, const FOnyxPixelLightRecipe& Recipe, FText& OutError)
{
	using namespace OnyxPixelLight;
	FPictures Pictures;
	TMap<UTexture2D*, UTexture2D*> Masks;
	TArray<UPackage*> Packages;
	int32 Given = 0;
	for (UPaperSprite* Sprite : SpritesOf(Flipbooks))
	{
		UTexture2D* Source = Sprite->GetSourceTexture();
		UTexture2D** Mask = Masks.Find(Source);
		if (!Mask)
		{
			// One mask per picture, as big as it: white where a pixel gives light (painted already, or made from the recipe here).
			UTexture2D* Made = Recipe.bFromPaintedMask ? FindGlowMask(Source) : nullptr;
			const FPicture* Picture = Recipe.bFromPaintedMask ? nullptr : Pictures.Get(Source);
			if (Picture)
			{
				TArray<FColor> Pixels;
				Pixels.SetNumUninitialized(Picture->Size.X * Picture->Size.Y);
				for (int64 Index = 0; Index < Picture->Pixels.Num(); ++Index)
				{
					Pixels[Index] = Recipe.Matches(Picture->Pixels[Index]) ? FColor::White : FColor::Black;
				}
				const FString Folder = FPackageName::GetLongPackagePath(Source->GetPackage()->GetName());
				Made = FOnyxAssetUtils::CreateTexture(Folder, Source->GetName() + TEXT("_GlowMask"), Pixels, Picture->Size);
				if (Made)
				{
					Packages.Add(Made->GetPackage());
				}
			}
			Mask = &Masks.Add(Source, Made);
		}
		if (!*Mask)
		{
			continue;
		}
		// Slot 0 stays the normal map; slot 1 is the glow mask. Protected in Paper2D: through reflection and the edit-change path
		// (it rebuilds the sprite's render data).
		FArrayProperty* Property = FindFProperty<FArrayProperty>(Sprite->GetClass(), TEXT("AdditionalSourceTextures"));
		if (!Property)
		{
			continue;
		}
		Sprite->Modify();
		Sprite->PreEditChange(Property);
		TArray<TObjectPtr<UTexture>>& Textures = *Property->ContainerPtrToValuePtr<TArray<TObjectPtr<UTexture>>>(Sprite);
		if (Textures.Num() < 2)
		{
			Textures.SetNum(2);
		}
		Textures[1] = *Mask;
		FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
		Sprite->PostEditChangeProperty(Event);
		Sprite->MarkPackageDirty();
		Packages.Add(Sprite->GetPackage());
		++Given;
	}
	if (Given == 0)
	{
		OutError = Recipe.bFromPaintedMask ? INVTEXT("No glow mask painted for these frames' textures (\"<texture>_GlowMask\" beside each).")
			: INVTEXT("No frames to give a glow mask to.");
	}
	FOnyxAssetUtils::SavePackages(Packages);
	return Given;
}

UTexture2D* FOnyxPixelLightBuilder::FindGlowMask(const UTexture2D* Picture)
{
	if (!Picture)
	{
		return nullptr;
	}
	const FString Name = Picture->GetName() + TEXT("_GlowMask");
	const FString Path = FPackageName::GetLongPackagePath(Picture->GetPackage()->GetName()) / Name + TEXT(".") + Name;
	return LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

TArray<TPair<FColor, int32>> FOnyxPixelLightBuilder::CountColours(const UPaperSprite* Sprite)
{
	using namespace OnyxPixelLight;
	TMap<FColor, int32> Counts;
	FPictures Pictures;
	if (const FPicture* Picture = Sprite ? Pictures.Get(Sprite->GetSourceTexture()) : nullptr)
	{
		const FIntPoint Origin(FMath::RoundToInt(Sprite->GetSourceUV().X), FMath::RoundToInt(Sprite->GetSourceUV().Y));
		const FIntPoint Extent(FMath::RoundToInt(Sprite->GetSourceSize().X), FMath::RoundToInt(Sprite->GetSourceSize().Y));
		for (int32 Y = 0; Y < Extent.Y; ++Y)
		{
			for (int32 X = 0; X < Extent.X; ++X)
			{
				const FColor Pixel = Picture->At(Origin.X + X, Origin.Y + Y);
				if (Pixel.A >= 128)
				{
					++Counts.FindOrAdd(Pixel.WithAlpha(255));
				}
			}
		}
	}
	TArray<TPair<FColor, int32>> Result;
	for (const TPair<FColor, int32>& Count : Counts)
	{
		Result.Add(Count);
	}
	Result.Sort([](const TPair<FColor, int32>& A, const TPair<FColor, int32>& B) { return A.Value > B.Value; });
	return Result;
}
