// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OnyxLightingTypes.generated.h"

/** Ready-made moods. Pick one, or Custom to set every value yourself. */
UENUM(BlueprintType)
enum class EOnyxMoodPreset : uint8
{
	Custom,
	Day,
	Dusk,
	Night,
	Cave,
	Dungeon
};

/**
 * The look of a 2D level, lit the 2D way (like Unity's 2D renderer, not with 3D lights):
 * a global light that reaches every sprite, a directional "sun" for normal-map relief, how strongly the
 * 2D lights shine, a tint for the parallax background, and the camera film (bloom, vignette, grading).
 * Blendable, so zones and scripted moments fade from one mood to another.
 */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxLightingMood
{
	GENERATED_BODY()

	// --- Global light: reaches every lit sprite equally (Unity's "Global Light 2D") ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Global Light")
	FLinearColor AmbientColor = FLinearColor(1.f, 0.98f, 0.95f);

	/** 1 = sprites show their true colours with no other light. Low values let the 2D lights do the work. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Global Light", meta = (ClampMin = "0", UIMax = "2"))
	float AmbientIntensity = 0.75f;

	// --- Sun: a directional light in the 2D plane (only changes how normal maps shade) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun")
	FLinearColor SunColor = FLinearColor(1.f, 0.95f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun", meta = (ClampMin = "0", UIMax = "2"))
	float SunIntensity = 0.35f;

	/** Where the sun shines from, in degrees: 90 = from above, 135 = upper left, 45 = upper right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun", meta = (UIMin = "0", UIMax = "360"))
	float SunAngle = 120.f;

	// --- 2D lights ---

	/** Multiplies every Onyx 2D light (night = lights matter more). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "2D Lights", meta = (ClampMin = "0", UIMax = "4"))
	float LightStrength = 1.f;

	/** Soft shadow and light edges: 0 = razor sharp, 1 = very soft. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "2D Lights", meta = (ClampMin = "0", ClampMax = "1"))
	float ShadowSoftness = 0.35f;

	/** How much normal maps shape the light: 0 = flat cut-out look, 1 = full relief. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "2D Lights", meta = (ClampMin = "0", ClampMax = "1"))
	float NormalStrength = 0.8f;

	// --- Background ---

	/** Multiplies the parallax background (darken the sky at night, warm it at dusk). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Background")
	FLinearColor BackgroundTint = FLinearColor::White;

	// --- Camera film ---

	/** Exposure in stops. Fixed, so the image never pumps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film", meta = (UIMin = "-3", UIMax = "3"))
	float Exposure = 0.f;

	/** Glow around bright pixels (lights, glowing pickups). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film", meta = (ClampMin = "0", UIMax = "4"))
	float BloomIntensity = 0.35f;

	/** Only pixels brighter than this glow (1 = only things brighter than white). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film", meta = (ClampMin = "0", UIMax = "4"))
	float BloomThreshold = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film", meta = (ClampMin = "0", ClampMax = "1"))
	float Vignette = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film", meta = (ClampMin = "0", UIMax = "2"))
	float Saturation = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film", meta = (ClampMin = "0", UIMax = "2"))
	float Contrast = 1.f;

	/** Colour multiplied over the whole image. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film")
	FLinearColor Tint = FLinearColor::White;

	/**
	 * 0 = pixel colours exactly as painted (2D look). Higher = Unreal's filmic curve, which rolls off bright
	 * lights softly but washes flat art out.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film", meta = (ClampMin = "0", ClampMax = "1"))
	float FilmicCurve = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film", meta = (ClampMin = "0", ClampMax = "1"))
	float FilmGrain = 0.f;

	static FOnyxLightingMood FromPreset(EOnyxMoodPreset Preset);
	static FOnyxLightingMood Blend(const FOnyxLightingMood& A, const FOnyxLightingMood& B, float Alpha);
	bool Equals(const FOnyxLightingMood& Other, float Tolerance = KINDA_SMALL_NUMBER) const;

	/** Unit vector (screen right / up) pointing towards the sun. */
	FVector2D GetSunDirection() const;
};

/** A saved lighting mood, used by Camera Zones (enter a cave, it gets dark) and scripted moments. */
UCLASS(BlueprintType)
class ONYX2DRUNTIME_API UOnyxLightingProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Start from a preset; editing any value switches to Custom. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lighting")
	EOnyxMoodPreset Preset = EOnyxMoodPreset::Day;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lighting", meta = (ShowOnlyInnerProperties))
	FOnyxLightingMood Mood;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
};
