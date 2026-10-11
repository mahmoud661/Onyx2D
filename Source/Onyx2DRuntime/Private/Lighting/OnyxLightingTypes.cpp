// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxLightingTypes.h"

FOnyxLightingMood FOnyxLightingMood::FromPreset(EOnyxMoodPreset Preset)
{
	FOnyxLightingMood Mood; // defaults = Day
	switch (Preset)
	{
	case EOnyxMoodPreset::Dusk:
		Mood.AmbientColor = FLinearColor(0.85f, 0.62f, 0.62f);
		Mood.AmbientIntensity = 0.5f;
		Mood.SunColor = FLinearColor(1.f, 0.55f, 0.28f);
		Mood.SunIntensity = 0.45f;
		Mood.SunAngle = 160.f;
		Mood.LightStrength = 1.3f;
		Mood.BackgroundTint = FLinearColor(1.f, 0.62f, 0.55f);
		Mood.BloomIntensity = 0.5f;
		Mood.Vignette = 0.3f;
		Mood.Saturation = 1.08f;
		break;
	case EOnyxMoodPreset::Night:
		Mood.AmbientColor = FLinearColor(0.35f, 0.45f, 0.85f);
		Mood.AmbientIntensity = 0.22f;
		Mood.SunColor = FLinearColor(0.55f, 0.65f, 1.f);
		Mood.SunIntensity = 0.12f;
		Mood.SunAngle = 60.f;
		Mood.LightStrength = 1.8f;
		Mood.BackgroundTint = FLinearColor(0.18f, 0.24f, 0.48f);
		Mood.BloomIntensity = 0.7f;
		Mood.BloomThreshold = 0.8f;
		Mood.Vignette = 0.45f;
		Mood.Saturation = 0.9f;
		Mood.Contrast = 1.05f;
		Mood.FilmGrain = 0.08f;
		break;
	case EOnyxMoodPreset::Cave:
		Mood.AmbientColor = FLinearColor(0.4f, 0.5f, 0.65f);
		Mood.AmbientIntensity = 0.08f;
		Mood.SunIntensity = 0.f;
		Mood.LightStrength = 2.2f;
		Mood.BackgroundTint = FLinearColor(0.08f, 0.1f, 0.14f);
		Mood.BloomIntensity = 0.9f;
		Mood.BloomThreshold = 0.7f;
		Mood.Vignette = 0.6f;
		Mood.Contrast = 1.08f;
		Mood.FilmGrain = 0.1f;
		break;
	case EOnyxMoodPreset::Dungeon:
		Mood.AmbientColor = FLinearColor(0.6f, 0.4f, 0.35f);
		Mood.AmbientIntensity = 0.1f;
		Mood.SunIntensity = 0.f;
		Mood.LightStrength = 2.f;
		Mood.BackgroundTint = FLinearColor(0.14f, 0.08f, 0.07f);
		Mood.BloomIntensity = 0.8f;
		Mood.BloomThreshold = 0.75f;
		Mood.Vignette = 0.55f;
		Mood.Contrast = 1.08f;
		Mood.FilmGrain = 0.1f;
		break;
	case EOnyxMoodPreset::Day:
	case EOnyxMoodPreset::Custom:
	default:
		break;
	}
	return Mood;
}

FOnyxLightingMood FOnyxLightingMood::Blend(const FOnyxLightingMood& A, const FOnyxLightingMood& B, float Alpha)
{
	if (Alpha <= 0.f)
	{
		return A;
	}
	if (Alpha >= 1.f)
	{
		return B;
	}
	FOnyxLightingMood Out;
	Out.AmbientColor = FMath::Lerp(A.AmbientColor, B.AmbientColor, Alpha);
	Out.AmbientIntensity = FMath::Lerp(A.AmbientIntensity, B.AmbientIntensity, Alpha);
	Out.SunColor = FMath::Lerp(A.SunColor, B.SunColor, Alpha);
	Out.SunIntensity = FMath::Lerp(A.SunIntensity, B.SunIntensity, Alpha);
	// Shortest way round the circle.
	Out.SunAngle = A.SunAngle + FMath::FindDeltaAngleDegrees(A.SunAngle, B.SunAngle) * Alpha;
	Out.LightStrength = FMath::Lerp(A.LightStrength, B.LightStrength, Alpha);
	Out.NormalStrength = FMath::Lerp(A.NormalStrength, B.NormalStrength, Alpha);
	Out.ShadowSoftness = FMath::Lerp(A.ShadowSoftness, B.ShadowSoftness, Alpha);
	Out.BackgroundTint = FMath::Lerp(A.BackgroundTint, B.BackgroundTint, Alpha);
	Out.Exposure = FMath::Lerp(A.Exposure, B.Exposure, Alpha);
	Out.BloomIntensity = FMath::Lerp(A.BloomIntensity, B.BloomIntensity, Alpha);
	Out.BloomThreshold = FMath::Lerp(A.BloomThreshold, B.BloomThreshold, Alpha);
	Out.Vignette = FMath::Lerp(A.Vignette, B.Vignette, Alpha);
	Out.Saturation = FMath::Lerp(A.Saturation, B.Saturation, Alpha);
	Out.Contrast = FMath::Lerp(A.Contrast, B.Contrast, Alpha);
	Out.Tint = FMath::Lerp(A.Tint, B.Tint, Alpha);
	Out.FilmicCurve = FMath::Lerp(A.FilmicCurve, B.FilmicCurve, Alpha);
	Out.FilmGrain = FMath::Lerp(A.FilmGrain, B.FilmGrain, Alpha);
	return Out;
}

bool FOnyxLightingMood::Equals(const FOnyxLightingMood& Other, float Tolerance) const
{
	return AmbientColor.Equals(Other.AmbientColor, Tolerance) && FMath::IsNearlyEqual(AmbientIntensity, Other.AmbientIntensity, Tolerance)
		&& SunColor.Equals(Other.SunColor, Tolerance) && FMath::IsNearlyEqual(SunIntensity, Other.SunIntensity, Tolerance)
		&& FMath::IsNearlyEqual(SunAngle, Other.SunAngle, Tolerance) && FMath::IsNearlyEqual(LightStrength, Other.LightStrength, Tolerance)
		&& FMath::IsNearlyEqual(NormalStrength, Other.NormalStrength, Tolerance) && FMath::IsNearlyEqual(ShadowSoftness, Other.ShadowSoftness, Tolerance) && BackgroundTint.Equals(Other.BackgroundTint, Tolerance)
		&& FMath::IsNearlyEqual(Exposure, Other.Exposure, Tolerance) && FMath::IsNearlyEqual(BloomIntensity, Other.BloomIntensity, Tolerance)
		&& FMath::IsNearlyEqual(BloomThreshold, Other.BloomThreshold, Tolerance) && FMath::IsNearlyEqual(Vignette, Other.Vignette, Tolerance)
		&& FMath::IsNearlyEqual(Saturation, Other.Saturation, Tolerance) && FMath::IsNearlyEqual(Contrast, Other.Contrast, Tolerance)
		&& Tint.Equals(Other.Tint, Tolerance) && FMath::IsNearlyEqual(FilmicCurve, Other.FilmicCurve, Tolerance)
		&& FMath::IsNearlyEqual(FilmGrain, Other.FilmGrain, Tolerance);
}

FVector2D FOnyxLightingMood::GetSunDirection() const
{
	const float Radians = FMath::DegreesToRadians(SunAngle);
	return FVector2D(FMath::Cos(Radians), FMath::Sin(Radians));
}

#if WITH_EDITOR
void UOnyxLightingProfile::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	if (Event.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UOnyxLightingProfile, Preset))
	{
		if (Preset != EOnyxMoodPreset::Custom)
		{
			Mood = FOnyxLightingMood::FromPreset(Preset);
		}
	}
	else if (Event.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UOnyxLightingProfile, Mood))
	{
		Preset = EOnyxMoodPreset::Custom;
	}
}
#endif
