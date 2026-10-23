// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxLight2DComponent.h"
#include "Engine/World.h"
#include "Lighting/OnyxLightingSubsystem.h"

UOnyxLight2DComponent::UOnyxLight2DComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // ticks only while fading
}

void UOnyxLight2DComponent::BeginPlay()
{
	Super::BeginPlay();
	bEnabledTarget = bStartEnabled;
	EnabledAlpha = bStartEnabled ? 1.f : 0.f;
}

void UOnyxLight2DComponent::SetLightEnabled(bool bEnabled, float FadeTime)
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

void UOnyxLight2DComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	EnabledAlpha = FMath::FInterpConstantTo(EnabledAlpha, bEnabledTarget ? 1.f : 0.f, DeltaTime, FadeSpeed);
	if (EnabledAlpha == (bEnabledTarget ? 1.f : 0.f))
	{
		SetComponentTickEnabled(false);
	}
}

FVector2D UOnyxLight2DComponent::GetPlanePosition() const
{
	const FVector Location = GetComponentLocation();
	return FVector2D(Location.X, Location.Z);
}

FVector2D UOnyxLight2DComponent::GetPlaneDirection() const
{
	const FVector Forward = GetForwardVector();
	const FVector2D Direction(Forward.X, Forward.Z);
	return Direction.IsNearlyZero() ? FVector2D(1.f, 0.f) : Direction.GetSafeNormal();
}

FVector2D UOnyxLight2DComponent::GetShapeHalfExtent() const
{
	switch (Shape)
	{
	case EOnyxLightShape::Line:
		return FVector2D(FMath::Max(Length, 0.f) * 0.5f, 0.f);
	case EOnyxLightShape::Area:
		return FVector2D(FMath::Max(Size.X, 0.f) * 0.5f, FMath::Max(Size.Y, 0.f) * 0.5f);
	default:
		return FVector2D::ZeroVector;
	}
}

float UOnyxLight2DComponent::GetReach() const
{
	return GetShapeHalfExtent().Size() + Radius;
}

void UOnyxLight2DComponent::OnRegister()
{
	Super::OnRegister();
	if (UOnyxLightingSubsystem* Lighting = GetWorld() ? GetWorld()->GetSubsystem<UOnyxLightingSubsystem>() : nullptr)
	{
		Lighting->RegisterLight(this);
	}
}

void UOnyxLight2DComponent::OnUnregister()
{
	if (UOnyxLightingSubsystem* Lighting = GetWorld() ? GetWorld()->GetSubsystem<UOnyxLightingSubsystem>() : nullptr)
	{
		Lighting->UnregisterLight(this);
	}
	Super::OnUnregister();
}

#if WITH_EDITOR
void UOnyxLight2DComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	IntensityScale = 1.f;
	// Made a spot: a cone to start from, not a full circle.
	if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UOnyxLight2DComponent, Shape) && Shape == EOnyxLightShape::Spot && ConeAngle >= 359.f)
	{
		ConeAngle = 60.f;
	}
}
#endif
