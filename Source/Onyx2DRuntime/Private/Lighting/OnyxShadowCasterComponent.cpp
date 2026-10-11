// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxShadowCasterComponent.h"
#include "Engine/World.h"
#include "Lighting/OnyxLightingSubsystem.h"

UOnyxShadowCasterComponent::UOnyxShadowCasterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsOnUpdateTransform = true; // follow the owner
}

TArray<FVector2D> UOnyxShadowCasterComponent::GetOutline() const
{
	if (!bUseBox && Outline.Num() >= 3)
	{
		return Outline;
	}
	const FVector2D Half = BoxSize * 0.5f;
	return { FVector2D(-Half.X, -Half.Y), FVector2D(Half.X, -Half.Y), FVector2D(Half.X, Half.Y), FVector2D(-Half.X, Half.Y) };
}

void UOnyxShadowCasterComponent::UpdateOccluder()
{
	UOnyxLightingSubsystem* Lighting = GetWorld() ? GetWorld()->GetSubsystem<UOnyxLightingSubsystem>() : nullptr;
	if (!Lighting)
	{
		return;
	}
	const TArray<FVector2D> Points = GetOutline();
	const FTransform& Transform = GetComponentTransform();
	TArray<FOnyxOccluderSegment> Segments;
	Segments.Reserve(Points.Num());
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const FVector A = Transform.TransformPosition(FVector(Points[Index].X, 0.f, Points[Index].Y));
		const FVector B = Transform.TransformPosition(FVector(Points[(Index + 1) % Points.Num()].X, 0.f, Points[(Index + 1) % Points.Num()].Y));
		Segments.Add({ FVector2D(A.X, A.Z), FVector2D(B.X, B.Z), LightPenetration });
	}
	Lighting->SetOccluders(this, MoveTemp(Segments));
}

void UOnyxShadowCasterComponent::OnRegister()
{
	Super::OnRegister();
	UpdateOccluder();
}

void UOnyxShadowCasterComponent::OnUnregister()
{
	if (UOnyxLightingSubsystem* Lighting = GetWorld() ? GetWorld()->GetSubsystem<UOnyxLightingSubsystem>() : nullptr)
	{
		Lighting->ClearOccluders(this);
	}
	Super::OnUnregister();
}

void UOnyxShadowCasterComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	UpdateOccluder();
}

#if WITH_EDITOR
void UOnyxShadowCasterComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateOccluder();
}
#endif
