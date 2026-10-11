// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxLight2DComponent.h"
#include "Engine/World.h"
#include "Lighting/OnyxLightingSubsystem.h"

UOnyxLight2DComponent::UOnyxLight2DComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
}
#endif
