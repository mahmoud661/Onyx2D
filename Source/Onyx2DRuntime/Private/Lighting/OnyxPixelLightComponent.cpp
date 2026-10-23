// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxPixelLightComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Lighting/OnyxLightingSubsystem.h"
#include "Lighting/OnyxPixelLightSet.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"

UOnyxPixelLightComponent::UOnyxPixelLightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only while fading
}

void UOnyxPixelLightComponent::OnRegister()
{
	Super::OnRegister();
	if (!Target.IsValid() && GetOwner())
	{
		Target = GetOwner()->FindComponentByClass<UPaperFlipbookComponent>();
	}
	if (UOnyxLightingSubsystem* Lighting = GetWorld() ? GetWorld()->GetSubsystem<UOnyxLightingSubsystem>() : nullptr)
	{
		Lighting->RegisterPixelLight(this);
	}
}

void UOnyxPixelLightComponent::OnUnregister()
{
	if (UOnyxLightingSubsystem* Lighting = GetWorld() ? GetWorld()->GetSubsystem<UOnyxLightingSubsystem>() : nullptr)
	{
		Lighting->UnregisterPixelLight(this);
	}
	Super::OnUnregister();
}

void UOnyxPixelLightComponent::BeginPlay()
{
	Super::BeginPlay();
	bEnabledTarget = bStartEnabled;
	EnabledAlpha = bStartEnabled ? 1.f : 0.f;
}

void UOnyxPixelLightComponent::SetLightEnabled(bool bEnabled, float FadeTime)
{
	bEnabledTarget = bEnabled;
	if (FadeTime <= KINDA_SMALL_NUMBER)
	{
		EnabledAlpha = bEnabled ? 1.f : 0.f;
		SetComponentTickEnabled(false);
		return;
	}
	FadeSpeed = 1.f / FadeTime;
	SetComponentTickEnabled(true);
}

void UOnyxPixelLightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	EnabledAlpha = FMath::FInterpConstantTo(EnabledAlpha, bEnabledTarget ? 1.f : 0.f, DeltaTime, FadeSpeed);
	if (EnabledAlpha == (bEnabledTarget ? 1.f : 0.f))
	{
		SetComponentTickEnabled(false);
	}
}

void UOnyxPixelLightComponent::SetTarget(UPaperFlipbookComponent* Sprite)
{
	Target = Sprite;
}

bool UOnyxPixelLightComponent::GetLightQuad(FOnyxPixelLightQuad& Out) const
{
	UPaperFlipbookComponent* Sprite = Target.Get();
	const float Brightness = Intensity * EnabledAlpha;
	if (!Sprite || !LightSet || !LightSet->Sheet || (Intensity + Halo) * EnabledAlpha <= KINDA_SMALL_NUMBER || !Sprite->IsVisible())
	{
		return false;
	}
	UPaperFlipbook* Flipbook = Sprite->GetFlipbook();
	const UPaperSprite* Frame = Flipbook ? Flipbook->GetSpriteAtTime(Sprite->GetPlaybackPosition(), true) : nullptr;
	const FOnyxPixelLightFrame* Light = LightSet->FindFrame(Frame);
	const FTexture* Sheet = LightSet->Sheet->GetResource();
	if (!Light || !Sheet)
	{
		return false;
	}
	// The frame's light rectangle around the sprite's pivot, placed (and turned round) with the sprite.
	const FTransform& ToWorld = Sprite->GetComponentTransform();
	auto Corner = [&ToWorld](float X, float Z)
	{
		const FVector World = ToWorld.TransformPosition(FVector(X, 0.f, Z));
		return FVector2D(World.X, World.Z);
	};
	Out.Corners[0] = Corner(Light->LocalMin.X, Light->LocalMax.Y);
	Out.Corners[1] = Corner(Light->LocalMax.X, Light->LocalMax.Y);
	Out.Corners[2] = Corner(Light->LocalMax.X, Light->LocalMin.Y);
	Out.Corners[3] = Corner(Light->LocalMin.X, Light->LocalMin.Y);
	Out.UVs[0] = FVector2D(Light->UVMin.X, Light->UVMin.Y);
	Out.UVs[1] = FVector2D(Light->UVMax.X, Light->UVMin.Y);
	Out.UVs[2] = FVector2D(Light->UVMax.X, Light->UVMax.Y);
	Out.UVs[3] = FVector2D(Light->UVMin.X, Light->UVMax.Y);
	Out.Texture = Sheet;
	Out.HaloTexture = LightSet->HaloSheet ? LightSet->HaloSheet->GetResource() : nullptr;
	Out.Color = Color * Brightness;
	Out.Halo = Color * (Halo * EnabledAlpha);
	Out.bOwnGlow = bOwnGlow;
	return true;
}
