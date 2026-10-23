// Copyright Onyx2D. All Rights Reserved.

#include "Combat/OnyxEnergyComponent.h"
#include "Animation/OnyxMovesetComponent.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Character/OnyxCharacter.h"
#include "Combat/OnyxCombatComponent.h"
#include "Combat/OnyxCombatTypes.h"
#include "Combat/OnyxHealthComponent.h"
#include "Combat/OnyxHitboxComponent.h"
#include "Feedback/OnyxFeedbackSubsystem.h"
#include "Movement/OnyxMovementComponent.h"
#include "World/OnyxBreakable.h"
#include "World/OnyxFlipbookEffect.h"

UOnyxEnergyComponent::UOnyxEnergyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UOnyxEnergyComponent::BeginPlay()
{
	Super::BeginPlay();
	Energy = FMath::Clamp(StartEnergy, 0.f, MaxEnergy);
	if (AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner()))
	{
		if (Character->AttackHitbox)
		{
			Character->AttackHitbox->OnHit.AddUniqueDynamic(this, &UOnyxEnergyComponent::HandleAttackHit);
		}
	}
	if (UOnyxHealthComponent* Health = GetOwner()->FindComponentByClass<UOnyxHealthComponent>())
	{
		Health->OnDamaged.AddUniqueDynamic(this, &UOnyxEnergyComponent::HandleDamaged);
		Health->OnDeath.AddUniqueDynamic(this, &UOnyxEnergyComponent::HandleDeath);
	}
}

void UOnyxEnergyComponent::HandleAttackHit(AActor* Victim)
{
	// Smashing pots and urns doesn't feed it (unless they say so); only real blows do.
	if (const AOnyxBreakable* Breakable = Cast<AOnyxBreakable>(Victim); Breakable && !Breakable->bFeedsEnergy)
	{
		return;
	}
	AddEnergy(EnergyPerHit);
}

void UOnyxEnergyComponent::HandleDamaged(const FOnyxDamageInfo& Damage)
{
	if (bMending)
	{
		EndMend(true);
	}
}

void UOnyxEnergyComponent::HandleDeath(AActor* Killer)
{
	if (bMending)
	{
		EndMend(true);
	}
}

void UOnyxEnergyComponent::AddEnergy(float Amount)
{
	if (Amount > 0.f)
	{
		SetEnergy(Energy + Amount);
	}
}

bool UOnyxEnergyComponent::SpendEnergy(float Amount)
{
	if (Amount < 0.f || Energy < Amount)
	{
		return false;
	}
	SetEnergy(Energy - Amount);
	return true;
}

void UOnyxEnergyComponent::SetEnergy(float NewEnergy)
{
	const float Before = Energy;
	Energy = FMath::Clamp(NewEnergy, 0.f, MaxEnergy);
	if (!FMath::IsNearlyEqual(Before, Energy))
	{
		OnEnergyChanged.Broadcast(Energy, Energy - Before);
		if (bCanMend && Before < MendCost && Energy >= MendCost)
		{
			OnMendReady.Broadcast();
		}
	}
}

bool UOnyxEnergyComponent::IsOnGround() const
{
	const UOnyxMovementComponent* Movement = GetOwner()->FindComponentByClass<UOnyxMovementComponent>();
	return !Movement || Movement->IsMovingOnGround();
}

bool UOnyxEnergyComponent::IsWounded() const
{
	const UOnyxHealthComponent* Health = GetOwner()->FindComponentByClass<UOnyxHealthComponent>();
	return Health && !Health->IsDead() && Health->GetHealth() < Health->MaxHealth;
}

bool UOnyxEnergyComponent::CanMend() const
{
	if (!bCanMend || Energy < MendCost || !IsWounded() || (!bMendInAir && !IsOnGround()))
	{
		return false;
	}
	const UOnyxMovementComponent* Movement = GetOwner()->FindComponentByClass<UOnyxMovementComponent>();
	return !Movement || (!Movement->IsDashing() && !Movement->IsOnLedge() && !Movement->IsSwimmingInWater());
}

bool UOnyxEnergyComponent::StartMend()
{
	if (bMending || !CanMend())
	{
		return false;
	}
	bMending = true;
	bDraining = false;
	MendTimer = 0.f;
	Drained = 0.f;
	AActor* Owner = GetOwner();
	if (UOnyxCombatComponent* Combat = Owner->FindComponentByClass<UOnyxCombatComponent>())
	{
		Combat->CancelAttack();
	}
	if (UOnyxMovementComponent* Movement = Owner->FindComponentByClass<UOnyxMovementComponent>())
	{
		Movement->SetMovementLocked(true);
		Movement->Velocity.X = 0.f;
	}
	if (UOnyxMovesetComponent* Moveset = Owner->FindComponentByClass<UOnyxMovesetComponent>(); Moveset && Moveset->HasClip(MendClip))
	{
		Moveset->bAutoLocomotion = false;
		Moveset->PlayClip(MendClip);
	}
	if (MendWindUp <= 0.f)
	{
		BeginDrain();
	}
	OnMendStarted.Broadcast();
	return true;
}

void UOnyxEnergyComponent::BeginDrain()
{
	bDraining = true;
	MendTimer = 0.f;
	Drained = 0.f;
	const AActor* Owner = GetOwner();
	if (GatherEffect)
	{
		AOnyxFlipbookEffect::Spawn(this, GatherEffect, Owner->GetActorLocation());
	}
	UOnyxAudioSubsystem::PlaySoundAt(this, GatherSound, Owner->GetActorLocation(), EOnyxSoundCategory::Effects);
}

float UOnyxEnergyComponent::GetMendProgress() const
{
	return bMending && bDraining ? FMath::Clamp(MendTimer / FMath::Max(MendTime, 0.05f), 0.f, 1.f) : 0.f;
}

void UOnyxEnergyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bMending)
	{
		return;
	}
	if (!bMendInAir && !IsOnGround())
	{
		EndMend(true); // the ground went from under it
		return;
	}
	MendTimer += DeltaTime;
	if (!bDraining)
	{
		if (MendTimer >= MendWindUp)
		{
			BeginDrain();
		}
		return;
	}
	// The energy flows out over Mend Time, so letting go early shows on the meter what it cost.
	const float Time = FMath::Max(MendTime, 0.05f);
	const float Owed = FMath::Min(MendCost * MendTimer / Time, MendCost);
	if (Owed > Drained)
	{
		SetEnergy(Energy - (Owed - Drained));
		Drained = Owed;
	}
	if (MendTimer < Time)
	{
		return;
	}
	AActor* Owner = GetOwner();
	if (UOnyxHealthComponent* Health = Owner->FindComponentByClass<UOnyxHealthComponent>())
	{
		Health->Heal(MendHeal);
	}
	if (MendedEffect)
	{
		AOnyxFlipbookEffect::Spawn(this, MendedEffect, Owner->GetActorLocation());
	}
	UOnyxAudioSubsystem::PlaySoundAt(this, MendedSound, Owner->GetActorLocation(), EOnyxSoundCategory::Effects);
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
	{
		if (MendedFlash.A > 0.f)
		{
			Feedback->FlashActor(Owner, MendedFlash, 0.15f);
		}
		Feedback->AddTrauma(0.08f);
	}
	OnMended.Broadcast();
	if (bKeepMending && Energy >= MendCost && IsWounded())
	{
		BeginDrain();
	}
	else
	{
		EndMend(false);
	}
}

void UOnyxEnergyComponent::EndMend(bool bInterrupted)
{
	if (!bMending)
	{
		return;
	}
	bMending = false;
	bDraining = false;
	AActor* Owner = GetOwner();
	const UOnyxHealthComponent* Health = Owner->FindComponentByClass<UOnyxHealthComponent>();
	const bool bDead = Health && Health->IsDead();
	if (UOnyxMovementComponent* Movement = Owner->FindComponentByClass<UOnyxMovementComponent>(); Movement && !bDead)
	{
		Movement->SetMovementLocked(false);
	}
	if (UOnyxMovesetComponent* Moveset = Owner->FindComponentByClass<UOnyxMovesetComponent>(); Moveset && !bDead)
	{
		Moveset->bAutoLocomotion = true; // the movement clips take over again
	}
	OnMendStopped.Broadcast(bInterrupted);
}
