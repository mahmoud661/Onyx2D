// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

/** Getting around seen from above (Top Down game style): walking towards somewhere without walking into walls. */
namespace OnyxTopDown
{
	/** Nothing solid within Reach along Way (X right, Y up the screen) from where the character stands. */
	inline bool IsOpen(const ACharacter& Character, const FVector2D& Way, float Reach)
	{
		const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
		const FVector Start = Character.GetActorLocation();
		const FVector End = Start + FVector(Way.X, 0.f, Way.Y) * Reach;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(OnyxTopDownStep), false, &Character);
		FHitResult Hit;
		// Its footprint swept a little thinner, so brushing past a wall's corner doesn't count as blocked.
		return !Character.GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
			FCollisionShape::MakeSphere(Capsule->GetScaledCapsuleRadius() * 0.8f), Params);
	}

	/**
	 * Which way to walk to get along Wanted: straight there, else the nearest way round a wall (turning towards the side
	 * it turned last, Bias), else none.
	 */
	inline FVector2D FindOpenWay(const ACharacter& Character, const FVector2D& Wanted, float Reach, float& Bias)
	{
		const FVector2D Way = Wanted.GetSafeNormal();
		if (Way.IsZero())
		{
			return FVector2D::ZeroVector;
		}
		const float Side = Bias < 0.f ? -1.f : 1.f;
		for (const float Degrees : { 0.f, 40.f * Side, -40.f * Side, 80.f * Side, -80.f * Side })
		{
			const FVector2D Turned = Way.GetRotated(Degrees);
			if (IsOpen(Character, Turned, Reach))
			{
				Bias = Degrees != 0.f ? FMath::Sign(Degrees) : Bias;
				return Turned;
			}
		}
		return FVector2D::ZeroVector;
	}
}
