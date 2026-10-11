// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxLightingEnvironment.h"
#include "Components/BillboardComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Core/Onyx2DSettings.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Lighting/OnyxLightingSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FName LightBufferParam(TEXT("LightBuffer"));
	const FName PositionBufferParam(TEXT("PositionBuffer"));
	const FName AmbientParam(TEXT("Ambient"));
	const FName SunColorParam(TEXT("SunColor"));
	const FName SunDirectionParam(TEXT("SunDirection"));
	const FName NormalStrengthParam(TEXT("NormalStrength"));
	const FName SoftnessParam(TEXT("Softness"));
	/** Blur kernel step (screen-height fraction) at Shadow Softness 1. */
	constexpr float MaxSoftnessStep = 0.006f;

	/** Manual exposure maps luminance 1 to 1/1.2 by default; this bias makes it exactly 1 (painted colours). */
	constexpr float ExposureCalibration = -0.263f;
}

AOnyxLightingEnvironment::AOnyxLightingEnvironment()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(RootComponent);
	PostProcess->bUnbound = true;
	PostProcess->Priority = 1.f;

#if WITH_EDITORONLY_DATA
	Icon = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Icon"));
	if (Icon)
	{
		Icon->SetupAttachment(RootComponent);
		Icon->bIsScreenSizeScaled = true;
	}
#endif

	Mood = FOnyxLightingMood::FromPreset(Preset);
}

FOnyxLightingMood AOnyxLightingEnvironment::GetBaseMood() const
{
	return Profile ? Profile->Mood : Mood;
}

void AOnyxLightingEnvironment::EnsureComposite()
{
	UMaterialInterface* Base = UOnyx2DSettings::Get()->LightCompositeMaterial.LoadSynchronous();
	if (!Base || (Composite && Composite->Parent == Base))
	{
		return;
	}
	Composite = UMaterialInstanceDynamic::Create(Base, this);
	PostProcess->Settings.WeightedBlendables.Array.Reset();
	PostProcess->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, Composite));
}

void AOnyxLightingEnvironment::SetLightBuffers(UTexture* LightBuffer, UTexture* PositionBuffer)
{
	EnsureComposite();
	if (Composite)
	{
		Composite->SetTextureParameterValue(LightBufferParam, LightBuffer);
		Composite->SetTextureParameterValue(PositionBufferParam, PositionBuffer);
	}
}

void AOnyxLightingEnvironment::ApplyMood(const FOnyxLightingMood& InMood)
{
	EnsureComposite();
	if (Composite)
	{
		Composite->SetVectorParameterValue(AmbientParam, InMood.AmbientColor * InMood.AmbientIntensity);
		Composite->SetVectorParameterValue(SunColorParam, InMood.SunColor * InMood.SunIntensity);
		const FVector2D Sun = InMood.GetSunDirection();
		Composite->SetVectorParameterValue(SunDirectionParam, FLinearColor(Sun.X, Sun.Y, 0.6f, 0.f));
		Composite->SetScalarParameterValue(NormalStrengthParam, InMood.NormalStrength);
		Composite->SetScalarParameterValue(SoftnessParam, InMood.ShadowSoftness * MaxSoftnessStep);
	}

	FPostProcessSettings& Settings = PostProcess->Settings;
	// Fixed exposure, calibrated so a sprite under a light of 1 shows its painted colour.
	Settings.bOverride_AutoExposureMethod = true;
	Settings.AutoExposureMethod = AEM_Manual;
	Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Settings.AutoExposureApplyPhysicalCameraExposure = false;
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = InMood.Exposure + ExposureCalibration;
	Settings.bOverride_LocalExposureHighlightContrastScale = true;
	Settings.LocalExposureHighlightContrastScale = 1.f;
	Settings.bOverride_LocalExposureShadowContrastScale = true;
	Settings.LocalExposureShadowContrastScale = 1.f;

	// Neutral tonemapping: the filmic curve and gamut expansion wash flat art out.
	Settings.bOverride_ToneCurveAmount = true;
	Settings.ToneCurveAmount = InMood.FilmicCurve;
	Settings.bOverride_ExpandGamut = true;
	Settings.ExpandGamut = InMood.FilmicCurve;
	Settings.bOverride_BlueCorrection = true;
	Settings.BlueCorrection = InMood.FilmicCurve * 0.6f;

	Settings.bOverride_BloomIntensity = true;
	Settings.BloomIntensity = InMood.BloomIntensity;
	Settings.bOverride_BloomThreshold = true;
	Settings.BloomThreshold = InMood.BloomThreshold;
	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = InMood.Vignette;
	Settings.bOverride_ColorSaturation = true;
	Settings.ColorSaturation = FVector4(InMood.Saturation, InMood.Saturation, InMood.Saturation, 1.f);
	Settings.bOverride_ColorContrast = true;
	Settings.ColorContrast = FVector4(InMood.Contrast, InMood.Contrast, InMood.Contrast, 1.f);
	Settings.bOverride_ColorGain = true;
	Settings.ColorGain = FVector4(InMood.Tint.R, InMood.Tint.G, InMood.Tint.B, 1.f);
	Settings.bOverride_FilmGrainIntensity = true;
	Settings.FilmGrainIntensity = InMood.FilmGrain;
	// Flat art has no depth worth blurring.
	Settings.bOverride_DepthOfFieldFocalDistance = true;
	Settings.DepthOfFieldFocalDistance = 0.f;
	Settings.bOverride_MotionBlurAmount = true;
	Settings.MotionBlurAmount = 0.f;
}

void AOnyxLightingEnvironment::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	ApplyMood(GetBaseMood());
	if (UOnyxLightingSubsystem* Lighting = GetWorld() ? GetWorld()->GetSubsystem<UOnyxLightingSubsystem>() : nullptr)
	{
		Lighting->RegisterEnvironment(this);
	}
}

void AOnyxLightingEnvironment::PostUnregisterAllComponents()
{
	if (UOnyxLightingSubsystem* Lighting = GetWorld() ? GetWorld()->GetSubsystem<UOnyxLightingSubsystem>() : nullptr)
	{
		Lighting->UnregisterEnvironment(this);
	}
	Super::PostUnregisterAllComponents();
}

#if WITH_EDITOR
void AOnyxLightingEnvironment::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	const FName Member = Event.GetMemberPropertyName();
	if (Member == GET_MEMBER_NAME_CHECKED(AOnyxLightingEnvironment, Preset))
	{
		if (Preset != EOnyxMoodPreset::Custom)
		{
			Mood = FOnyxLightingMood::FromPreset(Preset);
		}
	}
	else if (Member == GET_MEMBER_NAME_CHECKED(AOnyxLightingEnvironment, Mood))
	{
		Preset = EOnyxMoodPreset::Custom;
	}
	Super::PostEditChangeProperty(Event);
	ApplyMood(GetBaseMood());
}
#endif
