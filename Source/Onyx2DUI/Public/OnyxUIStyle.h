// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

struct FOnyxUITheme;
class UTexture2D;

/** Style helpers built on the theme in Project Settings > Onyx2D UI. */
namespace OnyxUI
{
	enum class EFont : uint8
	{
		Title,
		Display,
		Body,
		Bold,
		Light,
	};

	ONYX2DUI_API const FOnyxUITheme& Theme();
	ONYX2DUI_API FSlateFontInfo Font(EFont Face, int32 Size, int32 LetterSpacing = 0);
	ONYX2DUI_API FSlateBrush SolidBrush(const FLinearColor& Color);
	ONYX2DUI_API FSlateBrush OutlineBrush(const FLinearColor& Fill, const FLinearColor& Outline, float Width = 1.f, float Radius = 2.f);
	ONYX2DUI_API FButtonStyle TransparentButtonStyle();

	/** White texture whose alpha follows AlphaFn(u, v); tint it with the image color. */
	ONYX2DUI_API UTexture2D* CreateAlphaTexture(int32 Width, int32 Height, TFunctionRef<float(float U, float V)> AlphaFn);

	inline FLinearColor WithAlpha(FLinearColor Color, float Alpha) { Color.A = Alpha; return Color; }
	inline FLinearColor Black(float Alpha) { return FLinearColor(0.f, 0.f, 0.f, Alpha); }
	inline float SmoothStep01(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }
}
