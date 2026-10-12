// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxLight2D.h"
#include "Sound/SoundBase.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DSettings.h"
#include "Engine/StaticMesh.h"
#include "Lighting/OnyxLight2DComponent.h"
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
	};

	FLightLook LookFor(EOnyxLightPreset Preset)
	{
		switch (Preset)
		{
		case EOnyxLightPreset::Candle:  return { FLinearColor(1.f, 0.72f, 0.42f), 1.2f, 320.f, EOnyxLightAnimation::Flicker, 0.3f, 9.f, 0.3f, 0.45f };
		case EOnyxLightPreset::Lantern: return { FLinearColor(1.f, 0.86f, 0.62f), 1.6f, 650.f, EOnyxLightAnimation::Flicker, 0.06f, 3.f, 0.3f, 0.3f };
		case EOnyxLightPreset::Magic:   return { FLinearColor(0.6f, 0.45f, 1.f), 1.8f, 560.f, EOnyxLightAnimation::Pulse, 0.35f, 2.5f, 0.4f, 0.5f };
		case EOnyxLightPreset::Crystal: return { FLinearColor(0.35f, 0.9f, 1.f), 1.5f, 520.f, EOnyxLightAnimation::Pulse, 0.25f, 1.5f, 0.35f, 0.45f };
		case EOnyxLightPreset::Lava:    return { FLinearColor(1.f, 0.38f, 0.12f), 2.f, 800.f, EOnyxLightAnimation::Pulse, 0.25f, 1.2f, 0.5f, 0.35f };
		case EOnyxLightPreset::Window:  return { FLinearColor(0.82f, 0.9f, 1.f), 1.3f, 700.f, EOnyxLightAnimation::None, 0.f, 0.f, 0.5f, 0.15f };
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
}

void UOnyxLightFlickerComponent::SetLight(UOnyxLight2DComponent* InLight)
{
	Light = InLight;
}

void UOnyxLightFlickerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Light && GetOwner())
	{
		Light = GetOwner()->FindComponentByClass<UOnyxLight2DComponent>();
	}
	Time = FMath::FRandRange(0.f, 100.f); // desynchronise neighbouring lights
}

void UOnyxLightFlickerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Light)
	{
		return;
	}
	if (Strength <= 0.f || Animation == EOnyxLightAnimation::None)
	{
		Scale = 1.f;
	}
	else
	{
		Time += DeltaTime * Speed;
		const float Wave = Animation == EOnyxLightAnimation::Flicker
			? 0.5f * (FMath::PerlinNoise1D(Time) + 1.f) // 0..1, organic
			: 0.5f * (FMath::Sin(Time) + 1.f);          // 0..1, smooth
		Scale = 1.f - Strength * Wave;
	}
	Light->IntensityScale = Scale;
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
	Halo->SetupAttachment(RootComponent);
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
	Flicker->Animation = Look.Animation;
	Flicker->Strength = Look.Strength;
	Flicker->Speed = Look.Speed;
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
	const float Diameter = Light2D->Radius * HaloSize;
	Halo->SetRelativeScale3D(FVector(Diameter / PlaneMeshSize, Diameter / PlaneMeshSize, 1.f));
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
