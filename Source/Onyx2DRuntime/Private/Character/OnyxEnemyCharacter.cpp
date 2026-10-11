// Copyright Onyx2D. All Rights Reserved.

#include "Character/OnyxEnemyCharacter.h"
#include "AI/OnyxPatrolComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "Combat/OnyxHitboxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Movement/OnyxMovementComponent.h"
#include "Feedback/OnyxFeedbackSubsystem.h"
#include "GameFramework/Character.h"
#include "PaperFlipbookComponent.h"
#include "World/OnyxFlipbookEffect.h"

AOnyxEnemyCharacter::AOnyxEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TeamId = OnyxTeams::Enemy;
	Health->MaxHealth = 3.f;
	Health->InvulnerabilityTime = 0.15f;

	if (UOnyxMovementComponent* Movement = GetOnyxMovement())
	{
		Movement->RunSpeed = 170.f;
		Movement->DashSettings.bEnabled = false;
		Movement->WallSettings.bWallSlide = false;
		Movement->WallSettings.bWallJump = false;
		Movement->JumpSettings.MaxJumps = 1;
	}

	Patrol = CreateDefaultSubobject<UOnyxPatrolComponent>(TEXT("Patrol"));

	ContactHitbox = CreateDefaultSubobject<UOnyxHitboxComponent>(TEXT("ContactHitbox"));
	ContactHitbox->SetupAttachment(GetCapsuleComponent());
	ContactHitbox->SetBoxExtent(FVector(32.f, 50.f, 56.f));
	ContactHitbox->bContinuous = true;
	ContactHitbox->Knockback = FVector2D(500.f, 350.f);
	ContactHitbox->HitStopDuration = 0.05f;
	ContactHitbox->HitTrauma = 0.35f;
	ContactHitbox->DamageType = TEXT("Contact");

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AOnyxEnemyCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ContactHitbox->ShouldHit = [this](AActor* Victim) { return !TryStomp(Victim); };
}

bool AOnyxEnemyCharacter::TryStomp(AActor* Victim)
{
	ACharacter* Stomper = Cast<ACharacter>(Victim);
	if (!bStompable || !Stomper || Health->IsDead() || Stomper->GetVelocity().Z > 50.f)
	{
		return false;
	}
	// The stomper's feet must be above our middle: landing on the head, not walking into the body.
	const float Feet = Stomper->GetActorLocation().Z - Stomper->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (Feet < GetActorLocation().Z)
	{
		return false;
	}
	Health->Kill(Stomper);
	Stomper->LaunchCharacter(FVector(0.f, 0.f, StompBounce), false, true);
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
	{
		Feedback->HitStop(0.06f);
		Feedback->AddTrauma(0.25f);
	}
	return true;
}

void AOnyxEnemyCharacter::HandleDeath(AActor* Killer)
{
	Super::HandleDeath(Killer);
	ContactHitbox->EndHitWindow();
	ContactHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	if (DeathEffect)
	{
		// The effect replaces the body.
		AOnyxFlipbookEffect::Spawn(this, DeathEffect, GetActorLocation(), GetSprite()->GetComponentScale().X);
		GetSprite()->SetVisibility(false);
		SetLifeSpan(0.1f);
	}
	else if (CorpseLifetime > 0.f)
	{
		SetLifeSpan(CorpseLifetime);
	}
}
