// Copyright Onyx2D. All Rights Reserved.

#include "Combat/OnyxContactDamageComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"

UOnyxContactDamageComponent::UOnyxContactDamageComponent()
{
	bContinuous = true;
	Knockback = FVector2D(500.f, 350.f);
	HitStopDuration = 0.05f;
	HitTrauma = 0.35f;
	DamageType = TEXT("Contact");
	InitBoxExtent(FVector(32.f, 50.f, 56.f)); // SetBoxExtent is not allowed in constructors
}

void UOnyxContactDamageComponent::BeginPlay()
{
	if (bFitToCapsule)
	{
		if (const UCapsuleComponent* Capsule = GetOwner()->FindComponentByClass<UCapsuleComponent>())
		{
			SetBoxExtent(FVector(Capsule->GetScaledCapsuleRadius() * 1.1f, 50.f, Capsule->GetScaledCapsuleHalfHeight() * 1.05f));
		}
	}
	Super::BeginPlay();
	if (UOnyxHealthComponent* Health = GetOwner()->FindComponentByClass<UOnyxHealthComponent>())
	{
		Health->OnDeath.AddUniqueDynamic(this, &UOnyxContactDamageComponent::HandleOwnerDeath);
	}
}

void UOnyxContactDamageComponent::HandleOwnerDeath(AActor* Killer)
{
	EndHitWindow();
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
