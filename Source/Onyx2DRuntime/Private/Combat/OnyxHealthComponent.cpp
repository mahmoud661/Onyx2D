// Copyright Onyx2D. All Rights Reserved.

#include "Combat/OnyxHealthComponent.h"
#include "Engine/World.h"

UOnyxHealthComponent::UOnyxHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOnyxHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
}

bool UOnyxHealthComponent::IsInvulnerable() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < InvulnerableUntil;
}

bool UOnyxHealthComponent::ApplyDamage(const FOnyxDamageInfo& Damage)
{
	if (bDead || Damage.Amount <= 0.f || (IsInvulnerable() && !Damage.bIgnoreInvulnerability))
	{
		return false;
	}

	SetHealth(Health - Damage.Amount);
	SetInvulnerableFor(InvulnerabilityTime);
	OnDamaged.Broadcast(Damage);

	if (Health <= 0.f)
	{
		Kill(Damage.Instigator);
	}
	return true;
}

void UOnyxHealthComponent::Heal(float Amount)
{
	if (!bDead && Amount > 0.f)
	{
		SetHealth(Health + Amount);
	}
}

void UOnyxHealthComponent::Kill(AActor* Killer)
{
	if (bDead)
	{
		return;
	}
	SetHealth(0.f);
	bDead = true;
	OnDeath.Broadcast(Killer);
}

void UOnyxHealthComponent::Revive()
{
	bDead = false;
	SetHealth(MaxHealth);
}

void UOnyxHealthComponent::SetInvulnerableFor(float Seconds)
{
	if (const UWorld* World = GetWorld())
	{
		InvulnerableUntil = FMath::Max(InvulnerableUntil, World->GetTimeSeconds() + Seconds);
	}
}

void UOnyxHealthComponent::SetHealth(float NewHealth)
{
	const float Clamped = FMath::Clamp(NewHealth, 0.f, MaxHealth);
	const float Delta = Clamped - Health;
	Health = Clamped;
	if (!FMath::IsNearlyZero(Delta))
	{
		OnHealthChanged.Broadcast(Health, Delta);
	}
}
