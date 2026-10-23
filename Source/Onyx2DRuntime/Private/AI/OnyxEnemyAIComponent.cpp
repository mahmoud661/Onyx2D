// Copyright Onyx2D. All Rights Reserved.

#include "AI/OnyxEnemyAIComponent.h"
#include "AI/OnyxPatrolComponent.h"
#include "AI/OnyxTopDownSteering.h"
#include "Animation/OnyxMovesetComponent.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Character/OnyxCharacter.h"
#include "Combat/OnyxCombatTypes.h"
#include "Combat/OnyxHealthComponent.h"
#include "Combat/OnyxHitboxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Feedback/OnyxFeedbackSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Movement/OnyxMovementComponent.h"
#include "World/OnyxPickup.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxEnemyAI, Log, All);

namespace
{
	/** How far ahead walkers look for walls and drops, cm. */
	constexpr float LookAhead = 30.f;
	/** Walkers stop this close to the player instead of shuffling into it, cm. */
	constexpr float CloseEnough = 50.f;

	float SignOf(float Value)
	{
		return Value >= 0.f ? 1.f : -1.f;
	}
}

UOnyxEnemyAIComponent::UOnyxEnemyAIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UOnyxEnemyAIComponent::BeginPlay()
{
	Super::BeginPlay();
	AOnyxCharacter* Self = Cast<AOnyxCharacter>(GetOwner());
	if (!Self)
	{
		SetComponentTickEnabled(false);
		return;
	}
	Character = Self;
	Patrol = Self->FindComponentByClass<UOnyxPatrolComponent>();
	Movement = Self->GetOnyxMovement();
	if (!Self->GetController())
	{
		Self->SpawnDefaultController(); // placed or spawned without one: it still needs one to walk
	}
	BaseRunSpeed = Movement.IsValid() ? Movement->RunSpeed : 0.f;
	bFlyer = Patrol.IsValid() && Patrol->Style == EOnyxPatrolStyle::Fly;
	Cooldowns.Init(0.f, Attacks.Num());
	Self->Health->OnDamaged.AddUniqueDynamic(this, &UOnyxEnemyAIComponent::HandleDamaged);
	Self->Health->OnDeath.AddUniqueDynamic(this, &UOnyxEnemyAIComponent::HandleDeath);
	Enter(bStartsDormant ? EOnyxEnemyState::Dormant : EOnyxEnemyState::Patrol);
}

void UOnyxEnemyAIComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AOnyxCharacter* Self = Character.Get();
	if (!Self || State == EOnyxEnemyState::Dead)
	{
		return;
	}
	StateTime += DeltaTime;
	for (float& Cooldown : Cooldowns)
	{
		Cooldown = FMath::Max(Cooldown - DeltaTime, 0.f);
	}
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	SinceSeen = CanSee(Player) ? 0.f : SinceSeen + DeltaTime;
	const bool bSees = SinceSeen == 0.f;
	const FOnyxEnemyAttack* Attack = GetAttack();

	switch (State)
	{
	case EOnyxEnemyState::Dormant:
		if (bSees)
		{
			FacePlayer(Player);
			WakeLength = Self->MovesetComponent->PlayOneShot(WakeClip);
			Enter(EOnyxEnemyState::Waking);
		}
		break;
	case EOnyxEnemyState::Waking:
		if (StateTime >= WakeLength)
		{
			Self->MovesetComponent->bAutoLocomotion = true;
			Enter(EOnyxEnemyState::Chase);
		}
		break;
	case EOnyxEnemyState::Patrol:
		if (bSees)
		{
			Enter(EOnyxEnemyState::Alert);
		}
		break;
	case EOnyxEnemyState::Alert:
		FacePlayer(Player);
		if (IsFlyer())
		{
			Fly(FVector::ZeroVector, DeltaTime);
		}
		if (StateTime >= AlertTime)
		{
			Enter(EOnyxEnemyState::Chase);
		}
		break;
	case EOnyxEnemyState::Chase:
		TickChase(DeltaTime, Player);
		break;
	case EOnyxEnemyState::WindUp:
		if (Attack)
		{
			// The tell: rearing back before a dash, rising before a dive, standing still before a cast.
			if (IsTopDown())
			{
				const bool bRearsBack = Attack->Move != EOnyxEnemyMove::Shoot && StateTime < 0.2f && CanWalkToward(-StrikeVector);
				Movement->Velocity = bRearsBack ? FVector(-StrikeVector.X, 0.f, -StrikeVector.Y) * 60.f : FVector::ZeroVector;
			}
			else if (bFlyer)
			{
				Fly(Attack->Move == EOnyxEnemyMove::Dive ? FVector(0.f, 0.f, Attack->Height / FMath::Max(Attack->WindUp, 0.1f)) : FVector::ZeroVector, DeltaTime);
			}
			else if ((Attack->Move == EOnyxEnemyMove::Lunge || Attack->Move == EOnyxEnemyMove::Charge) && StateTime < 0.2f && CanWalk(-StrikeDirection))
			{
				Movement->Velocity.X = -StrikeDirection * 60.f;
			}
			else
			{
				StopMoving();
			}
			if (StateTime >= Attack->WindUp)
			{
				BeginStrike(Player);
				if (State == EOnyxEnemyState::WindUp) // a hit while it began (a trade of blows) staggers it instead
				{
					Enter(EOnyxEnemyState::Strike);
				}
			}
		}
		else
		{
			Enter(EOnyxEnemyState::Chase);
		}
		break;
	case EOnyxEnemyState::Strike:
		TickStrike(DeltaTime, Player);
		break;
	case EOnyxEnemyState::Recover:
		if (IsFlyer() && Player)
		{
			// Back up to hovering height after a swoop.
			const FVector Goal = Player->GetActorLocation() + FVector(0.f, 0.f, HoverHeight);
			Fly((Goal - Self->GetActorLocation()).GetClampedToMaxSize(FlySpeed * 0.6f), DeltaTime);
		}
		else
		{
			StopMoving();
		}
		if (!Attack || StateTime >= Attack->Recover)
		{
			Enter(EOnyxEnemyState::Chase);
		}
		break;
	case EOnyxEnemyState::Stagger:
		if (IsFlyer())
		{
			Fly(FVector::ZeroVector, DeltaTime);
		}
		if (StateTime >= (StaggerFor > 0.f ? StaggerFor : StaggerTime))
		{
			Enter(EOnyxEnemyState::Chase);
		}
		break;
	default:
		break;
	}
}

void UOnyxEnemyAIComponent::Enter(EOnyxEnemyState NewState)
{
	const EOnyxEnemyState OldState = State;
	State = NewState;
	StateTime = 0.f;
	UE_LOG(LogOnyxEnemyAI, Verbose, TEXT("%s: %s -> %s"), *GetNameSafe(GetOwner()), *UEnum::GetDisplayValueAsText(OldState).ToString(),
		*UEnum::GetDisplayValueAsText(NewState).ToString());
	AOnyxCharacter* Self = Character.Get();
	if (!Self)
	{
		return;
	}
	const bool bEngaged = NewState != EOnyxEnemyState::Patrol && NewState != EOnyxEnemyState::Dormant && NewState != EOnyxEnemyState::Dead;
	if (UOnyxPatrolComponent* Walk = Patrol.Get())
	{
		Walk->SetComponentTickEnabled(NewState == EOnyxEnemyState::Patrol);
	}
	Self->bFaceMovement = !bEngaged; // while it fights, it looks at the player, whichever way it moves
	if (NewState != EOnyxEnemyState::Strike)
	{
		SetHitbox(false);
	}
	const FOnyxEnemyAttack* Attack = GetAttack();
	switch (NewState)
	{
	case EOnyxEnemyState::Dormant:
		Self->MovesetComponent->bAutoLocomotion = false;
		Self->MovesetComponent->PlayClip(DormantClip);
		break;
	case EOnyxEnemyState::Patrol:
		SetRunSpeed(BaseRunSpeed);
		break;
	case EOnyxEnemyState::Alert:
		if (bHopWhenAlerted && !bFlyer && Movement.IsValid() && Movement->IsMovingOnGround() && !IsTopDown())
		{
			Self->LaunchCharacter(FVector(0.f, 0.f, 260.f), true, true);
		}
		UOnyxAudioSubsystem::PlaySoundAt(this, AlertSound, Self->GetActorLocation(), EOnyxSoundCategory::Effects);
		break;
	case EOnyxEnemyState::Waking:
		UOnyxAudioSubsystem::PlaySoundAt(this, AlertSound, Self->GetActorLocation(), EOnyxSoundCategory::Effects);
		break;
	case EOnyxEnemyState::Chase:
		SetRunSpeed(BaseRunSpeed * ChaseSpeedScale);
		break;
	case EOnyxEnemyState::WindUp:
		StrikeDirection = Self->GetFacing() == EOnyxFacing::Right ? 1.f : -1.f; // it commits to a direction now
		StrikeVector = Self->GetAimVector();
		if (Attack && !Attack->Clip.IsNone())
		{
			Self->MovesetComponent->PlayOneShot(Attack->Clip);
		}
		if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
		{
			Feedback->FlashActor(Self, TellColor, 0.1f);
		}
		if (Attack)
		{
			UOnyxAudioSubsystem::PlaySoundAt(this, Attack->TellSound, Self->GetActorLocation(), EOnyxSoundCategory::Effects);
		}
		break;
	case EOnyxEnemyState::Recover:
		SetRunSpeed(BaseRunSpeed);
		if (Cooldowns.IsValidIndex(CurrentAttack) && Attack)
		{
			Cooldowns[CurrentAttack] = Attack->Cooldown;
		}
		break;
	case EOnyxEnemyState::Stagger:
		SetRunSpeed(BaseRunSpeed);
		Self->MovesetComponent->bAutoLocomotion = true;
		break;
	case EOnyxEnemyState::Dead:
		SetRunSpeed(BaseRunSpeed); // its death clip holds: the character took the animation over
		break;
	default:
		break;
	}
	OnStateChanged.Broadcast(NewState, OldState);
}

bool UOnyxEnemyAIComponent::CanSee(const APawn* Player) const
{
	const AOnyxCharacter* Self = Character.Get();
	if (!Player || !Self)
	{
		return false;
	}
	if (const UOnyxHealthComponent* PlayerHealth = Player->FindComponentByClass<UOnyxHealthComponent>(); PlayerHealth && PlayerHealth->IsDead())
	{
		return false;
	}
	const FVector ToPlayer = Player->GetActorLocation() - Self->GetActorLocation();
	if (IsTopDown())
	{
		// From above: all round within Sight Range, walls in between hiding the player.
		if (FVector2D(ToPlayer.X, ToPlayer.Z).Size() > SightRange)
		{
			return false;
		}
		FHitResult Hit;
		return !bNeedLineOfSight || !GetWorld()->LineTraceSingleByObjectType(Hit, Self->GetActorLocation(), Player->GetActorLocation(),
			FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(OnyxEnemySight), false, Self));
	}
	if (FMath::Abs(ToPlayer.X) > SightRange || FMath::Abs(ToPlayer.Z) > SightHeight)
	{
		return false;
	}
	if (!bNeedLineOfSight)
	{
		return true;
	}
	// From its eyes (near the top of its body) to the player's body or head: the edge of the ledge it stands on doesn't
	// hide a player just below it.
	const float SelfHalf = Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const ACharacter* PlayerCharacter = Cast<ACharacter>(Player);
	const float PlayerHalf = PlayerCharacter ? PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 50.f;
	const FVector Eyes = Self->GetActorLocation() + FVector(0.f, 0.f, SelfHalf * 0.7f);
	const FCollisionObjectQueryParams Blockers(ECC_WorldStatic);
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(OnyxEnemySight), false, Self);
	for (const float Height : { 0.f, PlayerHalf * 0.7f })
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Eyes, Player->GetActorLocation() + FVector(0.f, 0.f, Height), Blockers, Params))
		{
			return true;
		}
	}
	return false;
}

void UOnyxEnemyAIComponent::FacePlayer(const APawn* Player)
{
	AOnyxCharacter* Self = Character.Get();
	if (!Self || !Player)
	{
		return;
	}
	const FVector ToPlayer = Player->GetActorLocation() - Self->GetActorLocation();
	if (IsTopDown())
	{
		Self->SetHeading(FVector2D(ToPlayer.X, ToPlayer.Z));
		return;
	}
	Self->SetFacing(ToPlayer.X >= 0.f ? EOnyxFacing::Right : EOnyxFacing::Left);
}

void UOnyxEnemyAIComponent::StopMoving()
{
	UOnyxMovementComponent* Move = Movement.Get();
	if (Move && Move->IsTopDown())
	{
		Move->Velocity = FVector::ZeroVector;
	}
	else if (Move && Move->IsMovingOnGround())
	{
		Move->Velocity.X = 0.f;
	}
}

bool UOnyxEnemyAIComponent::IsTopDown() const
{
	return Movement.IsValid() && Movement->IsTopDown();
}

bool UOnyxEnemyAIComponent::CanWalkToward(const FVector2D& Way) const
{
	const AOnyxCharacter* Self = Character.Get();
	return Self && OnyxTopDown::IsOpen(*Self, Way.GetSafeNormal(), Self->GetCapsuleComponent()->GetScaledCapsuleRadius() + LookAhead);
}

void UOnyxEnemyAIComponent::WalkToward(const FVector2D& Way)
{
	AOnyxCharacter* Self = Character.Get();
	if (!Self || Way.IsNearlyZero())
	{
		return;
	}
	const FVector2D Open = OnyxTopDown::FindOpenWay(*Self, Way, Self->GetCapsuleComponent()->GetScaledCapsuleRadius() + LookAhead, WayBias);
	if (!Open.IsZero())
	{
		Self->AddMovementInput(FVector(Open.X, 0.f, Open.Y));
	}
}

bool UOnyxEnemyAIComponent::CanWalk(float Direction) const
{
	const AOnyxCharacter* Self = Character.Get();
	if (!Self)
	{
		return false;
	}
	const UCapsuleComponent* Capsule = Self->GetCapsuleComponent();
	const FVector Start = Self->GetActorLocation();
	const FVector Ahead = Start + FVector(Direction * (Capsule->GetScaledCapsuleRadius() + LookAhead), 0.f, 0.f);
	const FCollisionObjectQueryParams Solid(ECC_WorldStatic);
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(OnyxEnemyStep), false, Self);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, Ahead, Solid, Params))
	{
		return false; // a wall
	}
	if (bFlyer || !Movement.IsValid() || !Movement->IsMovingOnGround())
	{
		return true;
	}
	return GetWorld()->LineTraceSingleByObjectType(Hit, Ahead, Ahead - FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 60.f), Solid, Params);
}

void UOnyxEnemyAIComponent::Fly(const FVector& Velocity, float DeltaTime)
{
	if (UOnyxMovementComponent* Move = Movement.Get())
	{
		Move->Velocity = FMath::VInterpTo(Move->Velocity, FVector(Velocity.X, 0.f, Velocity.Z), DeltaTime, 5.f);
	}
}

void UOnyxEnemyAIComponent::TickChase(float DeltaTime, const APawn* Player)
{
	AOnyxCharacter* Self = Character.Get();
	if (!Player || SinceSeen > ForgetTime)
	{
		Enter(EOnyxEnemyState::Patrol);
		return;
	}
	const FVector ToPlayer = Player->GetActorLocation() - Self->GetActorLocation();
	const bool bTopDown = IsTopDown();
	// Top down, ranges are all round; from the side, sideways (and no more than Max Height above or below).
	const float Distance = bTopDown ? FVector2D(ToPlayer.X, ToPlayer.Z).Size() : FMath::Abs(ToPlayer.X);
	FacePlayer(Player);

	// An attack that fits now: in range, ready, and the player in sight.
	if (SinceSeen == 0.f)
	{
		for (int32 Index = 0; Index < Attacks.Num(); ++Index)
		{
			const FOnyxEnemyAttack& Attack = Attacks[Index];
			if (Cooldowns[Index] <= 0.f && Distance >= Attack.MinRange && Distance <= Attack.MaxRange && (bTopDown || FMath::Abs(ToPlayer.Z) <= Attack.MaxHeight))
			{
				CurrentAttack = Index;
				Enter(EOnyxEnemyState::WindUp);
				return;
			}
		}
	}

	if (bTopDown)
	{
		// Straight for the player (round walls), or keeping its distance: backing off, holding its ground in between.
		const FVector2D ToTarget(ToPlayer.X, ToPlayer.Z);
		float Approach = 1.f;
		if (KeepDistance > 0.f)
		{
			Approach = Distance < KeepDistance * 0.8f ? -1.f : Distance < KeepDistance * 1.2f ? 0.f : 1.f;
		}
		else if (Distance < CloseEnough)
		{
			Approach = 0.f;
		}
		if (Approach != 0.f)
		{
			WalkToward(ToTarget * Approach);
		}
		return;
	}
	if (bFlyer)
	{
		// Hover above the player, a little to its own side.
		const FVector Goal = Player->GetActorLocation() + FVector(-SignOf(ToPlayer.X) * FMath::Max(KeepDistance, 120.f), 0.f, HoverHeight);
		const FVector ToGoal = Goal - Self->GetActorLocation();
		Fly(ToGoal.Size() > 20.f ? ToGoal.GetSafeNormal() * FlySpeed : FVector::ZeroVector, DeltaTime);
		return;
	}
	float Direction = SignOf(ToPlayer.X);
	if (KeepDistance > 0.f)
	{
		// Casters keep their distance: back off when the player comes close, hold their ground in between.
		Direction = Distance < KeepDistance * 0.8f ? -Direction : Distance < KeepDistance * 1.2f ? 0.f : Direction;
	}
	else if (Distance < CloseEnough)
	{
		Direction = 0.f;
	}
	if (Direction != 0.f && CanWalk(Direction))
	{
		Self->AddMovementInput(FVector(Direction, 0.f, 0.f));
	}
}

void UOnyxEnemyAIComponent::BeginStrike(const APawn* Player)
{
	AOnyxCharacter* Self = Character.Get();
	const FOnyxEnemyAttack* Attack = GetAttack();
	if (!Self || !Attack)
	{
		return;
	}
	UOnyxAudioSubsystem::PlaySoundAt(this, Attack->Sound, Self->GetActorLocation(), EOnyxSoundCategory::Effects);
	const FVector Here = Self->GetActorLocation();
	if (IsTopDown() && Attack->Move != EOnyxEnemyMove::Shoot)
	{
		// From above every rush is along the ground the way it faces: a lunge, a charge (a pounce or a dive too).
		SetRunSpeed(Attack->Speed);
		Movement->Velocity = FVector(StrikeVector.X, 0.f, StrikeVector.Y) * Attack->Speed;
		SetHitbox(true);
		return;
	}
	switch (Attack->Move)
	{
	case EOnyxEnemyMove::Lunge:
	case EOnyxEnemyMove::Charge:
		SetRunSpeed(Attack->Speed);
		Movement->Velocity.X = StrikeDirection * Attack->Speed;
		SetHitbox(true);
		break;
	case EOnyxEnemyMove::Pounce:
	{
		const float Up = FMath::Sqrt(2.f * FMath::Abs(Movement->GetGravityZ()) * Attack->Height);
		Self->LaunchCharacter(FVector(StrikeDirection * Attack->Speed, 0.f, Up), true, true);
		SetHitbox(true);
		break;
	}
	case EOnyxEnemyMove::Shoot:
	{
		// Top down, the shot leaves Muzzle.X ahead the way it faces and flies straight at the player across the ground.
		const FVector From = IsTopDown() ? Here + FVector(StrikeVector.X, 0.f, StrikeVector.Y) * Attack->Muzzle.X
			: Here + FVector(StrikeDirection * Attack->Muzzle.X, 0.f, Attack->Muzzle.Y);
		const FVector Aim = Player ? Player->GetActorLocation() - From : FVector(StrikeDirection, 0.f, 0.f);
		for (int32 Shot = 0; Shot < Attack->Shots; ++Shot)
		{
			// Fanned out evenly around the aim, in the side view's plane.
			const float Angle = FMath::DegreesToRadians((Shot - (Attack->Shots - 1) * 0.5f) * Attack->Spread);
			const FVector Direction(Aim.X * FMath::Cos(Angle) - Aim.Z * FMath::Sin(Angle), 0.f, Aim.X * FMath::Sin(Angle) + Aim.Z * FMath::Cos(Angle));
			AOnyxProjectile::Fire(this, Attack->Projectile, From, Direction, Self);
		}
		break;
	}
	case EOnyxEnemyMove::Dive:
		DiveDirection = Player ? (Player->GetActorLocation() + FVector(0.f, 0.f, 20.f) - Here).GetSafeNormal() : FVector(StrikeDirection, 0.f, -1.f).GetSafeNormal();
		SetHitbox(true);
		break;
	}
}

void UOnyxEnemyAIComponent::TickStrike(float DeltaTime, const APawn* Player)
{
	AOnyxCharacter* Self = Character.Get();
	const FOnyxEnemyAttack* Attack = GetAttack();
	if (!Self || !Attack)
	{
		Enter(EOnyxEnemyState::Chase);
		return;
	}
	if (IsTopDown() && Attack->Move != EOnyxEnemyMove::Shoot)
	{
		if (!CanWalkToward(StrikeVector))
		{
			if (Attack->Move == EOnyxEnemyMove::Charge)
			{
				if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
				{
					Feedback->AddTrauma(0.2f); // slammed into a wall
				}
			}
			Enter(EOnyxEnemyState::Recover);
			return;
		}
		Movement->Velocity = FVector(StrikeVector.X, 0.f, StrikeVector.Y) * Attack->Speed;
		Self->AddMovementInput(FVector(StrikeVector.X, 0.f, StrikeVector.Y));
		if (StateTime >= Attack->StrikeTime)
		{
			Enter(EOnyxEnemyState::Recover);
		}
		return;
	}
	switch (Attack->Move)
	{
	case EOnyxEnemyMove::Lunge:
	case EOnyxEnemyMove::Charge:
		if (!CanWalk(StrikeDirection))
		{
			if (Attack->Move == EOnyxEnemyMove::Charge)
			{
				if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
				{
					Feedback->AddTrauma(0.2f); // slammed into a wall
				}
			}
			Enter(EOnyxEnemyState::Recover);
			return;
		}
		Movement->Velocity.X = StrikeDirection * Attack->Speed;
		Self->AddMovementInput(FVector(StrikeDirection, 0.f, 0.f));
		break;
	case EOnyxEnemyMove::Pounce:
		if (StateTime > 0.15f && Movement->IsMovingOnGround())
		{
			Enter(EOnyxEnemyState::Recover);
			return;
		}
		break;
	case EOnyxEnemyMove::Dive:
		Movement->Velocity = DiveDirection * Attack->Speed;
		break;
	default:
		break;
	}
	if (StateTime >= Attack->StrikeTime + (Attack->Move == EOnyxEnemyMove::Pounce ? 1.5f : 0.f))
	{
		Enter(EOnyxEnemyState::Recover);
	}
}

void UOnyxEnemyAIComponent::SetHitbox(bool bOpen)
{
	AOnyxCharacter* Self = Character.Get();
	UOnyxHitboxComponent* Hitbox = Self ? Self->AttackHitbox.Get() : nullptr;
	if (!Hitbox || bOpen == bHitboxOpen)
	{
		return;
	}
	bHitboxOpen = bOpen;
	if (bOpen)
	{
		const FOnyxEnemyAttack* Attack = GetAttack();
		Hitbox->Damage = Attack ? Attack->Damage : 1.f;
		Hitbox->Knockback = Attack ? Attack->Knockback : FVector2D(450.f, 300.f);
		if (Attack && !Attack->HitboxSize.IsNearlyZero())
		{
			Self->SetAttackShape(Attack->HitboxOffset, Attack->HitboxSize);
		}
		Hitbox->BeginHitWindow();
	}
	else
	{
		Hitbox->EndHitWindow();
		Self->ClearAttackShape();
	}
}

void UOnyxEnemyAIComponent::SetRunSpeed(float Speed)
{
	if (UOnyxMovementComponent* Move = Movement.Get(); Move && Speed > 0.f && !FMath::IsNearlyEqual(Move->RunSpeed, Speed))
	{
		Move->RunSpeed = Speed;
		Move->ApplyTuning();
	}
}

void UOnyxEnemyAIComponent::HandleDamaged(const FOnyxDamageInfo& Damage)
{
	if (State == EOnyxEnemyState::Dead)
	{
		return;
	}
	SinceSeen = 0.f; // hit from behind or while asleep: it knows where you are now
	if (State == EOnyxEnemyState::Strike && !bStaggerDuringStrike)
	{
		return; // committed
	}
	if (State == EOnyxEnemyState::Dormant || State == EOnyxEnemyState::Waking)
	{
		if (AOnyxCharacter* Self = Character.Get())
		{
			Self->MovesetComponent->bAutoLocomotion = true;
		}
	}
	StaggerFor = 0.f;
	Enter(EOnyxEnemyState::Stagger);
}

void UOnyxEnemyAIComponent::Interrupt(float Seconds)
{
	if (State == EOnyxEnemyState::Dead || State == EOnyxEnemyState::Dormant || State == EOnyxEnemyState::Waking)
	{
		return;
	}
	if (AOnyxCharacter* Self = Character.Get())
	{
		Self->MovesetComponent->StopOneShot(); // the strike is over
	}
	StaggerFor = Seconds;
	Enter(EOnyxEnemyState::Stagger);
}

void UOnyxEnemyAIComponent::HandleDeath(AActor* Killer)
{
	Enter(EOnyxEnemyState::Dead);
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
	{
		Feedback->HitStop(0.09f);
		Feedback->AddTrauma(0.3f);
	}
	DropCoins();
}

void UOnyxEnemyAIComponent::DropCoins()
{
	AOnyxCharacter* Self = Character.Get();
	if (!Self || CoinDrop <= 0 || !CoinLook)
	{
		return;
	}
	const FTransform Where(Self->GetActorLocation() + FVector(0.f, 0.f, 20.f));
	for (int32 Coin = 0; Coin < CoinDrop; ++Coin)
	{
		// Set up before it finishes spawning, so its construction shows the coin.
		AOnyxPickup* Pickup = GetWorld()->SpawnActorDeferred<AOnyxPickup>(AOnyxPickup::StaticClass(), Where, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Pickup)
		{
			continue;
		}
		Pickup->Type = EOnyxPickupType::Coin;
		Pickup->Amount = 1;
		Pickup->Flipbook = CoinLook;
		Pickup->CollectEffect = CoinCollectEffect;
		Pickup->CollectSound = CoinSound;
		Pickup->BobHeight = 4.f;
		Pickup->FinishSpawning(Where);
		// A burst of coins, fanned upwards.
		Pickup->Pop(FVector2D(FMath::FRandRange(-220.f, 220.f), FMath::FRandRange(380.f, 620.f)));
	}
}
