// Copyright Onyx2D. All Rights Reserved.

#include "OnyxUISettings.h"

FOnyxUITheme::FOnyxUITheme()
	: Accent(FLinearColor::FromSRGBColor(FColor(143, 224, 220)))
	, Danger(FLinearColor::FromSRGBColor(FColor(232, 128, 84)))
	, TextBright(FLinearColor::FromSRGBColor(FColor(244, 240, 232)))
	, Text(FLinearColor::FromSRGBColor(FColor(201, 198, 191)))
	, TextDim(FLinearColor::FromSRGBColor(FColor(118, 126, 138)))
	, Panel(FLinearColor::FromSRGBColor(FColor(10, 13, 19, 245)))
	, TitleFont(TEXT("/Engine/EngineFonts/Roboto.Roboto"))
	, DisplayFont(TEXT("/Engine/EngineFonts/Roboto.Roboto"))
	, BodyFont(TEXT("/Engine/EngineFonts/Roboto.Roboto"))
{
}
