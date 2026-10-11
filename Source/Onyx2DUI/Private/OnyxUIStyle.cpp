// Copyright Onyx2D. All Rights Reserved.

#include "OnyxUIStyle.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "OnyxUISettings.h"
#include "TextureResource.h"

namespace OnyxUI
{
	const FOnyxUITheme& Theme()
	{
		return UOnyxUISettings::Get()->Theme;
	}

	static UObject* LoadFont(const FSoftObjectPath& Path)
	{
		UObject* Loaded = Path.IsValid() ? Path.TryLoad() : nullptr;
		return Loaded ? Loaded : LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	}

	FSlateFontInfo Font(EFont Face, int32 Size, int32 LetterSpacing)
	{
		const FOnyxUITheme& Style = Theme();
		UObject* FontObject = nullptr;
		FName Typeface = NAME_None;
		switch (Face)
		{
		case EFont::Title:   FontObject = LoadFont(Style.TitleFont); Typeface = TEXT("Light"); break;
		case EFont::Display: FontObject = LoadFont(Style.DisplayFont); Typeface = TEXT("Light"); break;
		case EFont::Bold:    FontObject = LoadFont(Style.BodyFont); Typeface = TEXT("Bold"); break;
		case EFont::Light:   FontObject = LoadFont(Style.BodyFont); Typeface = TEXT("Light"); break;
		default:             FontObject = LoadFont(Style.BodyFont); Typeface = TEXT("Regular"); break;
		}
		FSlateFontInfo Info(FontObject, Size, Typeface);
		Info.LetterSpacing = LetterSpacing;
		return Info;
	}

	FSlateBrush SolidBrush(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.TintColor = FSlateColor(Color);
		return Brush;
	}

	FSlateBrush OutlineBrush(const FLinearColor& Fill, const FLinearColor& Outline, float Width, float Radius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Outline), Width);
		return Brush;
	}

	FButtonStyle TransparentButtonStyle()
	{
		FSlateBrush None;
		None.DrawAs = ESlateBrushDrawType::NoDrawType;
		FButtonStyle Style;
		Style.SetNormal(None);
		Style.SetHovered(None);
		Style.SetPressed(None);
		Style.SetDisabled(None);
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f));
		return Style;
	}

	UTexture2D* CreateAlphaTexture(int32 Width, int32 Height, TFunctionRef<float(float U, float V)> AlphaFn)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		if (!Texture)
		{
			return nullptr;
		}
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->Filter = TF_Bilinear;
		Texture->SRGB = true;

		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const float U = Width > 1 ? float(X) / float(Width - 1) : 0.f;
				const float V = Height > 1 ? float(Y) / float(Height - 1) : 0.f;
				Pixels[Y * Width + X] = FColor(255, 255, 255, uint8(FMath::Clamp(AlphaFn(U, V), 0.f, 1.f) * 255.f + 0.5f));
			}
		}
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}
}
