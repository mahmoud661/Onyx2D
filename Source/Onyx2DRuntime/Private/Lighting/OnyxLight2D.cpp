// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxLight2D.h"
#include "Sound/SoundBase.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DSettings.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Lighting/OnyxLight2DComponent.h"
#include "Lighting/OnyxLightingEnvironment.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName HaloColorParam(TEXT("Color"));
	const FName HaloIntensityParam(TEXT("Intensity"));
	const FName HaloFalloffParam(TEXT("Falloff"));
	/** Engine plane mesh is 100 x 100 units. */
	constexpr float PlaneMeshSize = 100.f;
	/** Halo sits just in front of the sprites so it glows over them. */
	constexpr float HaloDepth = 30.f;

	struct FLightLook
	{
		FLinearColor Color;
		float Intensity;
		float Radius;
		EOnyxLightAnimation Animation;
		float Strength;
		float Speed;
		float HaloSize;
		float HaloIntensity;
		EOnyxLightShape Shape = EOnyxLightShape::Point;
		EOnyxLightFalloff Falloff = EOnyxLightFalloff::Smooth;
		/** Spot: cone; Line: length; Area: width and height. */
		float ConeAngle = 360.f;
		float Length = 200.f;
		FVector2D Size = FVector2D(160.f, 220.f);
		float ColorShift = 0.f;
		FLinearColor ColorB = FLinearColor(0.4f, 0.6f, 1.f);
		float SweepAngle = 0.f;
		float SwayDistance = 0.f;
	};

	FLightLook LookFor(EOnyxLightPreset Preset)
	{
		FLightLook Look;
		switch (Preset)
		{
		case EOnyxLightPreset::Candle:  return { FLinearColor(1.f, 0.72f, 0.42f), 1.2f, 320.f, EOnyxLightAnimation::Flicker, 0.3f, 9.f, 0.3f, 0.45f };
		case EOnyxLightPreset::Lantern: return { FLinearColor(1.f, 0.86f, 0.62f), 1.6f, 650.f, EOnyxLightAnimation::Flicker, 0.06f, 3.f, 0.3f, 0.3f };
		case EOnyxLightPreset::Magic:   return { FLinearColor(0.6f, 0.45f, 1.f), 1.8f, 560.f, EOnyxLightAnimation::Pulse, 0.35f, 2.5f, 0.4f, 0.5f };
		case EOnyxLightPreset::Crystal: return { FLinearColor(0.35f, 0.9f, 1.f), 1.5f, 520.f, EOnyxLightAnimation::Pulse, 0.25f, 1.5f, 0.35f, 0.45f };
		case EOnyxLightPreset::Lava:    return { FLinearColor(1.f, 0.38f, 0.12f), 2.f, 800.f, EOnyxLightAnimation::Pulse, 0.25f, 1.2f, 0.5f, 0.35f };
		case EOnyxLightPreset::Window:  return { FLinearColor(0.82f, 0.9f, 1.f), 1.3f, 700.f, EOnyxLightAnimation::None, 0.f, 0.f, 0.5f, 0.15f };
		case EOnyxLightPreset::Neon:
			Look = { FLinearColor(1.f, 0.3f, 0.75f), 1.4f, 260.f, EOnyxLightAnimation::Stutter, 0.85f, 6.f, 0.5f, 0.55f };
			Look.Shape = EOnyxLightShape::Line;
			Look.Length = 240.f;
			Look.ColorShift = 1.f;
			Look.ColorB = FLinearColor(0.25f, 0.55f, 1.f);
			return Look;
		case EOnyxLightPreset::Fluorescent:
			Look = { FLinearColor(0.86f, 0.95f, 1.f), 1.3f, 340.f, EOnyxLightAnimation::Stutter, 0.9f, 3.f, 0.4f, 0.3f };
			Look.Shape = EOnyxLightShape::Line;
			Look.Length = 180.f;
			Look.Falloff = EOnyxLightFalloff::Even;
			return Look;
		case EOnyxLightPreset::Brazier:
			Look = { FLinearColor(1.f, 0.55f, 0.22f), 2.1f, 520.f, EOnyxLightAnimation::Flicker, 0.35f, 10.f, 0.4f, 0.5f };
			Look.Falloff = EOnyxLightFalloff::Sharp;
			return Look;
		case EOnyxLightPreset::Searchlight:
			Look = { FLinearColor(0.95f, 0.97f, 1.f), 2.2f, 1100.f, EOnyxLightAnimation::None, 0.f, 0.f, 0.f, 0.f };
			Look.Shape = EOnyxLightShape::Spot;
			Look.ConeAngle = 24.f;
			Look.Falloff = EOnyxLightFalloff::Even;
			Look.SweepAngle = 35.f;
			return Look;
		case EOnyxLightPreset::HangingLantern:
			Look = { FLinearColor(1.f, 0.8f, 0.5f), 1.5f, 480.f, EOnyxLightAnimation::Flicker, 0.08f, 4.f, 0.3f, 0.4f };
			Look.SwayDistance = 28.f;
			return Look;
		case EOnyxLightPreset::Alarm:
			Look = { FLinearColor(1.f, 0.12f, 0.08f), 2.f, 700.f, EOnyxLightAnimation::Strobe, 1.f, 1.5f, 0.35f, 0.6f };
			Look.Shape = EOnyxLightShape::Spot;
			Look.ConeAngle = 50.f;
			Look.SweepAngle = 70.f;
			return Look;
		case EOnyxLightPreset::Glowshroom:
			Look = { FLinearColor(0.3f, 1.f, 0.75f), 1.1f, 300.f, EOnyxLightAnimation::Pulse, 0.4f, 1.1f, 0.45f, 0.5f };
			Look.Falloff = EOnyxLightFalloff::Soft;
			return Look;
		case EOnyxLightPreset::MoonWindow:
			Look = { FLinearColor(0.62f, 0.76f, 1.f), 1.f, 360.f, EOnyxLightAnimation::None, 0.f, 0.f, 0.f, 0.f };
			Look.Shape = EOnyxLightShape::Area;
			Look.Size = FVector2D(120.f, 240.f);
			Look.Falloff = EOnyxLightFalloff::Soft;
			return Look;
		case EOnyxLightPreset::StainedGlass:
			Look = { FLinearColor(1.f, 0.95f, 0.9f), 1.6f, 520.f, EOnyxLightAnimation::None, 0.f, 0.f, 0.f, 0.f };
			return Look;
		case EOnyxLightPreset::Torch:
		case EOnyxLightPreset::Custom:
		default:                          return { FLinearColor(1.f, 0.62f, 0.32f), 1.8f, 620.f, EOnyxLightAnimation::Flicker, 0.22f, 6.f, 0.35f, 0.35f };
		}
	}
}

// --- Flicker / pulse ---------------------------------------------------------------------------

UOnyxLightFlickerComponent::UOnyxLightFlickerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only while something animates (Refresh)
}

void UOnyxLightFlickerComponent::SetLight(UOnyxLight2DComponent* InLight)
{
	Light = InLight;
	bHasBase = false;
	Refresh();
}

void UOnyxLightFlickerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Light && GetOwner())
	{
		Light = GetOwner()->FindComponentByClass<UOnyxLight2DComponent>();
	}
	Time = FMath::FRandRange(0.f, 100.f); // desynchronise neighbouring lights
	StutterTimer = FMath::FRandRange(0.2f, 2.f);
	Refresh();
}

void UOnyxLightFlickerComponent::Refresh()
{
	if (!Light || !HasBegunPlay())
	{
		return;
	}
	if (!bHasBase)
	{
		// What it moves and tints around: the light as placed.
		BaseColor = Light->Color;
		BaseLocation = Light->GetRelativeLocation();
		BaseRotation = Light->GetRelativeRotation();
		bHasBase = true;
	}
	const bool bAnimates = (Animation != EOnyxLightAnimation::None && Strength > 0.f) || ColorShift > 0.f || HasMotion();
	if (!bAnimates)
	{
		Scale = 1.f;
		Light->IntensityScale = 1.f;
	}
	SetComponentTickEnabled(bAnimates);
}

float UOnyxLightFlickerComponent::Darkness(float DeltaTime)
{
	switch (Animation)
	{
	case EOnyxLightAnimation::Flicker:
		return 0.5f * (FMath::PerlinNoise1D(Time * Speed) + 1.f); // organic
	case EOnyxLightAnimation::Pulse:
		return 0.5f * (FMath::Sin(Time * Speed) + 1.f); // smooth breathing
	case EOnyxLightAnimation::Strobe:
		return FMath::Frac(Time * FMath::Max(Speed, 0.01f)) < StrobeDuty ? 0.f : 1.f;
	case EOnyxLightAnimation::Stutter:
	{
		// Lit, then now and then a stutter: one to four quick drop-outs.
		StutterTimer -= DeltaTime;
		if (StutterBlinks == 0)
		{
			if (StutterTimer <= 0.f)
			{
				StutterBlinks = FMath::RandRange(1, 4);
				bStutterDark = true;
				StutterTimer = FMath::FRandRange(0.03f, 0.08f);
			}
			return 0.f;
		}
		if (StutterTimer <= 0.f)
		{
			bStutterDark = !bStutterDark;
			StutterTimer = FMath::FRandRange(0.03f, 0.1f);
			if (!bStutterDark && --StutterBlinks == 0)
			{
				StutterTimer = FMath::FRandRange(0.5f, 3.f) * 6.f / FMath::Max(Speed, 0.1f);
			}
		}
		return bStutterDark ? 1.f : 0.f;
	}
	default:
		return 0.f;
	}
}

void UOnyxLightFlickerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Light)
	{
		return;
	}
	Time += DeltaTime;
	Scale = Animation == EOnyxLightAnimation::None ? 1.f : 1.f - Strength * Darkness(DeltaTime);
	Light->IntensityScale = Scale;
	if (ColorShift > 0.f)
	{
		const float Toward = 0.5f - 0.5f * FMath::Cos(UE_TWO_PI * Time / ColorShiftPeriod);
		Light->Color = FMath::Lerp(BaseColor, ColorB, ColorShift * Toward);
	}
	if (HasMotion())
	{
		FVector Location = BaseLocation;
		FRotator Rotation = BaseRotation;
		if (SweepAngle > 0.f)
		{
			// Pitch turns it within the 2D plane.
			Rotation.Pitch += SweepAngle * FMath::Sin(UE_TWO_PI * Time / SweepPeriod);
		}
		if (SwayDistance > 0.f)
		{
			// A pendulum: across and a little up at each end, tilting with its chain.
			const float Most = FMath::Asin(FMath::Clamp(SwayDistance / SwayLength, 0.f, 1.f));
			const float Angle = Most * FMath::Sin(UE_TWO_PI * Time / SwayPeriod);
			Location += FVector(SwayLength * FMath::Sin(Angle), 0.f, SwayLength * (1.f - FMath::Cos(Angle)));
			Rotation.Pitch += FMath::RadiansToDegrees(Angle);
		}
		Light->SetRelativeLocationAndRotation(Location, Rotation);
	}
}

// --- Light ---------------------------------------------------------------------------------------

AOnyxLight2D::AOnyxLight2D()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Light2D = CreateDefaultSubobject<UOnyxLight2DComponent>(TEXT("Light2D"));
	Light2D->SetupAttachment(RootComponent);

	Flicker = CreateDefaultSubobject<UOnyxLightFlickerComponent>(TEXT("Flicker"));

	Halo = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Halo"));
	Halo->SetupAttachment(Light2D); // it swings and sweeps with the light
	Halo->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Halo->SetCastShadow(false);
	Halo->SetGenerateOverlapEvents(false);
	Halo->SetRelativeLocation(FVector(0.f, HaloDepth, 0.f));
	Halo->SetRelativeRotation(FRotator(0.f, 0.f, -90.f)); // plane faces +Y, towards the camera
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded())
	{
		Halo->SetStaticMesh(Plane.Object);
	}

	ApplyPreset(Preset);
}

void AOnyxLight2D::ApplyPreset(EOnyxLightPreset NewPreset)
{
	Preset = NewPreset;
	if (NewPreset == EOnyxLightPreset::Custom)
	{
		return;
	}
	const FLightLook Look = LookFor(NewPreset);
	Light2D->Color = Look.Color;
	Light2D->Intensity = Look.Intensity;
	Light2D->Radius = Look.Radius;
	Light2D->Shape = Look.Shape;
	Light2D->Falloff = Look.Falloff;
	Light2D->ConeAngle = Look.ConeAngle;
	Light2D->Length = Look.Length;
	Light2D->Size = Look.Size;
	Flicker->Animation = Look.Animation;
	Flicker->Strength = Look.Strength;
	Flicker->Speed = Look.Speed;
	Flicker->ColorShift = Look.ColorShift;
	Flicker->ColorB = Look.ColorB;
	Flicker->SweepAngle = Look.SweepAngle;
	Flicker->SwayDistance = Look.SwayDistance;
	Flicker->Refresh();
	HaloSize = Look.HaloSize;
	HaloIntensity = Look.HaloIntensity;
}

void AOnyxLight2D::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UMaterialInterface* HaloBase = UOnyx2DSettings::Get()->LightHaloMaterial.LoadSynchronous();
	if (HaloBase && (!HaloMaterial || HaloMaterial->Parent != HaloBase))
	{
		HaloMaterial = UMaterialInstanceDynamic::Create(HaloBase, this);
		Halo->SetMaterial(0, HaloMaterial);
	}
	UpdateHalo();
}

void AOnyxLight2D::UpdateHalo()
{
	const bool bVisible = bShowHalo && HaloMaterial;
	Halo->SetVisibility(bVisible);
	if (!bVisible)
	{
		return;
	}
	// Round around a point, stretched along a line or an area.
	const float Diameter = Light2D->Radius * HaloSize;
	const FVector2D Shape = Light2D->GetShapeHalfExtent() * 2.f;
	Halo->SetRelativeScale3D(FVector((Shape.X + Diameter) / PlaneMeshSize, (Shape.Y + Diameter) / PlaneMeshSize, 1.f));
	HaloMaterial->SetVectorParameterValue(HaloColorParam, Light2D->Color);
	HaloMaterial->SetScalarParameterValue(HaloIntensityParam, HaloIntensity * Light2D->IntensityScale * Light2D->GetEnabledAlpha());
	HaloMaterial->SetScalarParameterValue(HaloFalloffParam, HaloFalloff);
}

void AOnyxLight2D::BeginPlay()
{
	Super::BeginPlay();
	if (AmbientSound && Light2D->bStartEnabled)
	{
		SoundLoop = UOnyxAudioSubsystem::PlaySoundAttached(AmbientSound, RootComponent, EOnyxSoundCategory::Effects, SoundVolume);
	}
	if (ActiveTime != EOnyxLightActiveTime::Always)
	{
		// Lit with the night (or the day): as it is now, then whenever it turns.
		for (TActorIterator<AOnyxLightingEnvironment> It(GetWorld()); It; ++It)
		{
			It->OnDayPhaseChanged.AddUniqueDynamic(this, &AOnyxLight2D::HandleDayPhase);
			const bool bWanted = (ActiveTime == EOnyxLightActiveTime::Night) == It->IsNight();
			Light2D->SetLightEnabled(bWanted, 0.f);
			if (SoundLoop && !bWanted)
			{
				SoundLoop->Stop();
				SoundLoop = nullptr;
			}
			break;
		}
	}
}

void AOnyxLight2D::HandleDayPhase(bool bNight)
{
	SetLightEnabled((ActiveTime == EOnyxLightActiveTime::Night) == bNight, DayFadeTime);
}

void AOnyxLight2D::SetLightEnabled(bool bEnabled, float FadeTime)
{
	if (bEnabled == Light2D->IsLightEnabled())
	{
		return;
	}
	Light2D->SetLightEnabled(bEnabled, FadeTime);
	UOnyxAudioSubsystem::PlaySoundAt(this, SwitchSound, GetActorLocation());
	if (bEnabled && AmbientSound)
	{
		if (!SoundLoop)
		{
			SoundLoop = UOnyxAudioSubsystem::PlaySoundAttached(AmbientSound, RootComponent, EOnyxSoundCategory::Effects, SoundVolume);
		}
		else
		{
			SoundLoop->FadeIn(FMath::Max(FadeTime, 0.05f), 1.f);
		}
	}
	else if (!bEnabled && SoundLoop)
	{
		SoundLoop->FadeOut(FMath::Max(FadeTime, 0.05f), 0.f);
	}
	SetActorTickEnabled(true); // the halo follows the fade
}

void AOnyxLight2D::ToggleLight(float FadeTime)
{
	SetLightEnabled(!IsLightEnabled(), FadeTime);
}

bool AOnyxLight2D::IsLightEnabled() const
{
	return Light2D->IsLightEnabled();
}

void AOnyxLight2D::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The halo breathes with the light (and fades with it).
	Halo->SetVisibility(bShowHalo && HaloMaterial && Light2D->GetEnabledAlpha() > 0.f);
	if (bShowHalo && HaloMaterial)
	{
		HaloMaterial->SetScalarParameterValue(HaloIntensityParam, HaloIntensity * Light2D->IntensityScale * Light2D->GetEnabledAlpha());
	}
}

#if WITH_EDITOR
void AOnyxLight2D::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(AOnyxLight2D, Preset))
	{
		ApplyPreset(Preset);
	}
	Super::PostEditChangeProperty(Event);
}
#endif
