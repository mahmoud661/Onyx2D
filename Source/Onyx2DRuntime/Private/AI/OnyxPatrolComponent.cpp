// Copyright Onyx2D. All Rights Reserved.

#include "AI/OnyxPatrolComponent.h"
#include "AI/OnyxTopDownSteering.h"
#include "Movement/OnyxMovementComponent.h"
#include "Character/OnyxCharacter.h"
#include "Combat/OnyxHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float ProbeDistance = 30.f;
	constexpr float ChaseHeightTolerance = 120.f;
}

UOnyxPatrolComponent::UOnyxPatrolComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UOnyxPatrolComponent::BeginPlay()
{
	Super::BeginPlay();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	if (!Character->GetController())
	{
		Character->SpawnDefaultController(); // spawned at runtime without auto-possess: still patrols
	}
	Home = Character->GetActorLocation();
	HopTimer = HopInterval * FMath::FRandRange(0.3f, 1.f); // neighbours don't hop in sync
	if (Style == EOnyxPatrolStyle::Fly)
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		Character->GetCharacterMovement()->GravityScale = 0.f;
	}
}

void UOnyxPatrolComponent::TurnAround()
{
	Direction = -FMath::Sign(Direction == 0.f ? 1.f : Direction);
	PauseRemaining = PauseAtTurn;
}

bool UOnyxPatrolComponent::IsBlockedAhead(const ACharacter* Character) const
{
	const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector Start = Character->GetActorLocation();
	const FVector End = Start + FVector(Direction * (Radius + ProbeDistance), 0.f, 0.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OnyxPatrolWall), false, Character);
	FHitResult Hit;
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params);
}

bool UOnyxPatrolComponent::HasGroundAhead(const ACharacter* Character) const
{
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const FVector Start = Character->GetActorLocation() + FVector(Direction * (Capsule->GetScaledCapsuleRadius() + ProbeDistance), 0.f, 0.f);
	const FVector End = Start - FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 60.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OnyxPatrolLedge), false, Character);
	FHitResult Hit;
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params);
}

bool UOnyxPatrolComponent::TryChase(const ACharacter* Character)
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!bChasePlayer || !Player)
	{
		return false;
	}
	const FVector ToPlayer = Player->GetActorLocation() - Character->GetActorLocation();
	if (FMath::Abs(ToPlayer.X) > ChaseRange || FMath::Abs(ToPlayer.Z) > ChaseHeightTolerance)
	{
		return false;
	}
	Direction = FMath::Sign(ToPlayer.X);
	return true;
}

void UOnyxPatrolComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	if (const UOnyxHealthComponent* Health = Character->FindComponentByClass<UOnyxHealthComponent>(); Health && Health->IsDead())
	{
		return;
	}
	if (PauseRemaining > 0.f)
	{
		PauseRemaining -= DeltaTime;
		return;
	}

	if (const UOnyxMovementComponent* Movement = Cast<UOnyxMovementComponent>(Character->GetCharacterMovement()); Movement && Movement->IsTopDown())
	{
		TickWander(Character, DeltaTime);
		return;
	}
	switch (Style)
	{
	case EOnyxPatrolStyle::Hop: TickHop(Character, DeltaTime); break;
	case EOnyxPatrolStyle::Fly: TickFly(Character, DeltaTime); break;
	case EOnyxPatrolStyle::Walk:
	default:                      TickWalk(Character); break;
	}
}

void UOnyxPatrolComponent::TickHop(ACharacter* Character, float DeltaTime)
{
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement->IsMovingOnGround())
	{
		return; // in the air: let the hop play out
	}
	Movement->Velocity.X = 0.f; // land and sit still
	HopTimer -= DeltaTime;
	if (HopTimer > 0.f)
	{
		return;
	}
	HopTimer = HopInterval;
	if (bAlternateHops || IsBlockedAhead(Character))
	{
		Direction = -FMath::Sign(Direction == 0.f ? 1.f : Direction);
	}
	if (AOnyxCharacter* Onyx = Cast<AOnyxCharacter>(Character))
	{
		Onyx->SetFacing(Direction > 0.f ? EOnyxFacing::Right : EOnyxFacing::Left);
	}
	Character->LaunchCharacter(FVector(Direction * HopVelocity.X, 0.f, HopVelocity.Y), true, true);
}

void UOnyxPatrolComponent::TickFly(ACharacter* Character, float DeltaTime)
{
	FlyTime += DeltaTime;
	const float Phase = UE_TWO_PI * FlyTime / FlyPeriod;
	const FVector Target = Home + FVector(FMath::Sin(Phase * 0.5f) * FlyRange.X, 0.f, FMath::Sin(Phase) * FlyRange.Y * 0.5f);
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->Velocity = (Target - Character->GetActorLocation()) / FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);
	Movement->Velocity.Y = 0.f;

	AOnyxCharacter* Onyx = Cast<AOnyxCharacter>(Character);
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Onyx && bFacePlayer && Player)
	{
		Onyx->SetFacing(Player->GetActorLocation().X >= Character->GetActorLocation().X ? EOnyxFacing::Right : EOnyxFacing::Left);
	}
}

void UOnyxPatrolComponent::PickWanderSpot()
{
	const float Angle = FMath::FRandRange(0.f, UE_TWO_PI);
	const float Distance = WanderRadius * FMath::Sqrt(FMath::FRand());
	WanderSpot = Home + FVector(FMath::Cos(Angle) * Distance, 0.f, FMath::Sin(Angle) * Distance);
	WanderTime = 0.f;
}

void UOnyxPatrolComponent::TickWander(ACharacter* Character, float DeltaTime)
{
	const float Reach = Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + ProbeDistance;
	FVector Goal = WanderSpot;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector ToPlayer = Player ? Player->GetActorLocation() - Character->GetActorLocation() : FVector::ZeroVector;
	const bool bChasing = bChasePlayer && Player && FVector2D(ToPlayer.X, ToPlayer.Z).Size() <= ChaseRange;
	if (bChasing)
	{
		Goal = Player->GetActorLocation();
	}
	else if (WanderSpot.IsZero())
	{
		PickWanderSpot();
		Goal = WanderSpot;
	}
	const FVector ToGoal = Goal - Character->GetActorLocation();
	const FVector2D Way(ToGoal.X, ToGoal.Z);
	WanderTime += DeltaTime;
	// There (or stuck on the way for long): a pause, then somewhere else.
	if (!bChasing && (Way.Size() < 24.f || WanderTime > 4.f))
	{
		PickWanderSpot();
		PauseRemaining = PauseAtTurn + FMath::FRandRange(0.f, 1.f);
		return;
	}
	const FVector2D Open = OnyxTopDown::FindOpenWay(*Character, Way, Reach, WayBias);
	if (Open.IsZero())
	{
		if (!bChasing)
		{
			PickWanderSpot();
		}
		return;
	}
	if (Style == EOnyxPatrolStyle::Hop)
	{
		// Sits, then a burst across the ground towards the spot.
		HopTimer -= DeltaTime;
		if (HopTimer <= 0.f)
		{
			HopTimer = HopInterval;
			Character->LaunchCharacter(FVector(Open.X, 0.f, Open.Y) * HopVelocity.X, true, true);
		}
		return;
	}
	Character->AddMovementInput(FVector(Open.X, 0.f, Open.Y));
}

void UOnyxPatrolComponent::TickWalk(ACharacter* Character)
{
	const bool bChasing = TryChase(Character);
	const bool bGrounded = Character->GetCharacterMovement()->IsMovingOnGround();
	if (bGrounded && (IsBlockedAhead(Character) || (bTurnAtLedges && !HasGroundAhead(Character))))
	{
		if (!bChasing)
		{
			TurnAround();
		}
		return; // never chase off a cliff
	}
	Character->AddMovementInput(FVector(Direction, 0.f, 0.f));
}
