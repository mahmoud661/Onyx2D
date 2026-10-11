// Copyright Onyx2D. All Rights Reserved.

#include "Combat/OnyxCombatComponent.h"
#include "Animation/OnyxAnimatorComponent.h"
#include "Combat/OnyxHitboxComponent.h"
#include "GameFramework/Actor.h"
#include "Movement/OnyxMovementComponent.h"

UOnyxCombatComponent::UOnyxCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	FOnyxAttack First;
	First.Animation = TEXT("Attack1");
	FOnyxAttack Second = First;
	Second.Animation = TEXT("Attack2");
	FOnyxAttack Finisher = First;
	Finisher.Animation = TEXT("Attack3");
	Finisher.Duration = 0.42f;
	Finisher.HitWindowEnd = 0.24f;
	Finisher.Damage = 2.f;
	Finisher.Knockback = FVector2D(700.f, 380.f);
	Finisher.ForwardImpulse = 320.f;
	Combo = { First, Second, Finisher };
}

void UOnyxCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		if (!Hitbox)
		{
			Hitbox = Owner->FindComponentByClass<UOnyxHitboxComponent>();
		}
		Animator = Owner->FindComponentByClass<UOnyxAnimatorComponent>();
		Movement = Owner->FindComponentByClass<UOnyxMovementComponent>();
	}
}

bool UOnyxCombatComponent::CanStartAttack() const
{
	if (Combo.IsEmpty())
	{
		return false;
	}
	if (!bAllowAirAttacks && Movement && Movement->IsFalling())
	{
		return false;
	}
	return !Movement || !Movement->IsDashing();
}

void UOnyxCombatComponent::RequestAttack()
{
	BufferRemaining = InputBufferTime;
	if (!IsAttacking() && CanStartAttack())
	{
		BufferRemaining = 0.f;
		StartAttack(0);
	}
}

void UOnyxCombatComponent::StartAttack(int32 Index)
{
	const FOnyxAttack& Attack = Combo[Index];
	CurrentIndex = Index;
	Elapsed = 0.f;
	SetHitWindow(false);

	const float AnimationLength = Animator ? Animator->PlayOneShot(Attack.Animation) : 0.f;
	CurrentDuration = AnimationLength > 0.f ? AnimationLength : Attack.Duration;

	if (Hitbox)
	{
		Hitbox->Damage = Attack.Damage;
		Hitbox->Knockback = Attack.Knockback;
	}
	if (Movement)
	{
		Movement->SetMovementLocked(Attack.bLockMovement);
		if (Attack.ForwardImpulse != 0.f)
		{
			Movement->Velocity.X = FacingSign * Attack.ForwardImpulse;
		}
	}
	OnAttackStarted.Broadcast(Index);
}

void UOnyxCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	BufferRemaining = FMath::Max(BufferRemaining - DeltaTime, 0.f);
	if (!IsAttacking())
	{
		if (BufferRemaining > 0.f && CanStartAttack())
		{
			BufferRemaining = 0.f;
			StartAttack(0);
		}
		return;
	}

	Elapsed += DeltaTime;
	const FOnyxAttack& Attack = Combo[CurrentIndex];
	SetHitWindow(Elapsed >= Attack.HitWindowStart && Elapsed < Attack.HitWindowEnd);

	const bool bHasNext = Combo.IsValidIndex(CurrentIndex + 1);
	if (bHasNext && BufferRemaining > 0.f && Elapsed >= Attack.ChainTime)
	{
		BufferRemaining = 0.f;
		StartAttack(CurrentIndex + 1);
		return;
	}
	if (Elapsed >= CurrentDuration)
	{
		FinishAttack();
	}
}

void UOnyxCombatComponent::CancelAttack()
{
	if (IsAttacking())
	{
		if (Animator)
		{
			Animator->StopOneShot();
		}
		FinishAttack();
	}
	BufferRemaining = 0.f;
}

void UOnyxCombatComponent::FinishAttack()
{
	const int32 Finished = CurrentIndex;
	SetHitWindow(false);
	CurrentIndex = INDEX_NONE;
	if (Movement)
	{
		Movement->SetMovementLocked(false);
	}
	OnAttackFinished.Broadcast(Finished);
}

void UOnyxCombatComponent::SetHitWindow(bool bOpen)
{
	if (bOpen == bHitWindowOpen || !Hitbox)
	{
		bHitWindowOpen = bOpen && Hitbox;
		return;
	}
	bHitWindowOpen = bOpen;
	bOpen ? Hitbox->BeginHitWindow() : Hitbox->EndHitWindow();
}
