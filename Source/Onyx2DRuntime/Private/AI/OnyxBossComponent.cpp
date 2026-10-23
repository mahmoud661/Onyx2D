// Copyright Onyx2D. All Rights Reserved.

#include "AI/OnyxBossComponent.h"
#include "Animation/OnyxMovesetComponent.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Character/OnyxCharacter.h"
#include "Combat/OnyxCombatTypes.h"
#include "Combat/OnyxContactDamageComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "Combat/OnyxHitboxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/OnyxWorldSubsystem.h"
#include "Engine/World.h"
#include "Feedback/OnyxFeedbackSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Lighting/OnyxLight2DComponent.h"
#include "Movement/OnyxMovementComponent.h"
#include "PaperFlipbookComponent.h"
#include "World/OnyxFlipbookEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxBoss, Log, All);

namespace OnyxBoss
{
	/** Teleports and summons keep this far inside the arena's walls, cm (plus the capsule). */
	constexpr float ArenaMargin = 40.f;

	float SideOf(float Value)
	{
		return Value >= 0.f ? 1.f : -1.f;
	}
}

UOnyxBossComponent::UOnyxBossComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	Phases.AddDefaulted();
}

void UOnyxBossComponent::BeginPlay()
{
	Super::BeginPlay();
	AOnyxCharacter* Self = Cast<AOnyxCharacter>(GetOwner());
	if (!Self)
	{
		SetComponentTickEnabled(false);
		return;
	}
	Character = Self;
	Movement = Self->GetOnyxMovement();
	if (!Self->GetController())
	{
		Self->SpawnDefaultController(); // it needs one to walk
	}
	if (Phases.IsEmpty())
	{
		Phases.AddDefaulted();
	}
	BaseRunSpeed = Movement.IsValid() ? Movement->RunSpeed : 0.f;
	StartLocation = Self->GetActorLocation();
	Cooldowns.Init(0.f, Moves.Num());
	Self->bFaceMovement = false; // it looks where it means to
	Self->Health->OnDamaged.AddUniqueDynamic(this, &UOnyxBossComponent::HandleDamaged);
	Self->Health->OnDeath.AddUniqueDynamic(this, &UOnyxBossComponent::HandleDeath);
	Self->Health->SetInvulnerable(true); // until the fight starts
	if (bPassThroughFighters)
	{
		Self->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	if (bHiddenUntilFight)
	{
		SetHidden(true);
	}
}

// --- The fight -----------------------------------------------------------------------------------------------------------

void UOnyxBossComponent::StartFight(const FBox& ArenaBounds)
{
	AOnyxCharacter* Self = Character.Get();
	if (!Self || bFightActive || bDefeated)
	{
		return;
	}
	Arena = ArenaBounds;
	bFightActive = true;
	FightStartTime = GetWorld()->GetRealTimeSeconds();
	Phase = 0;
	PendingPhase = INDEX_NONE;
	HitsSinceStagger = 0;
	HitsSinceMove = 0;
	GraceLeft = 0.f;
	Self->CustomTimeDilation = Phases[0].Speed;
	Self->Health->SetInvulnerable(false);
	if (UOnyxWorldSubsystem* Registry = GetWorld()->GetSubsystem<UOnyxWorldSubsystem>())
	{
		Registry->SetActiveBoss(this);
	}
	LogState(TEXT("the fight begins"));
	OnFightStarted.Broadcast();
	const int32 Intro = FindMove(IntroMove);
	if (Intro != INDEX_NONE)
	{
		StartMove(Intro, true);
	}
	else
	{
		SetHidden(false);
		Rest();
	}
}

void UOnyxBossComponent::ResetFight()
{
	AOnyxCharacter* Self = Character.Get();
	if (!Self || bDefeated)
	{
		return;
	}
	Interrupt();
	KillSummons();
	State = EState::Waiting;
	bFightActive = false;
	Phase = 0;
	PendingPhase = INDEX_NONE;
	HitsSinceStagger = 0;
	HitsSinceMove = 0;
	GraceLeft = 0.f;
	LastMove = INDEX_NONE;
	for (float& Cooldown : Cooldowns)
	{
		Cooldown = 0.f;
	}
	Self->CustomTimeDilation = 1.f;
	Self->Health->Revive();
	Self->Health->SetInvulnerable(true);
	Self->SetActorLocation(StartLocation, false, nullptr, ETeleportType::TeleportPhysics);
	if (UOnyxMovementComponent* Move = Movement.Get())
	{
		Move->StopMovementImmediately();
	}
	SetHidden(bHiddenUntilFight);
	LogState(TEXT("back to the start"));
	OnFightReset.Broadcast();
}

float UOnyxBossComponent::GetHealthFraction() const
{
	const AOnyxCharacter* Self = Character.Get();
	return Self ? Self->Health->GetHealthFraction() : 0.f;
}

float UOnyxBossComponent::GetFightTime() const
{
	const UWorld* World = GetWorld();
	return World ? float(World->GetRealTimeSeconds() - (bDefeated ? DefeatTime : FightStartTime)) : 0.f;
}

FName UOnyxBossComponent::GetCurrentMove() const
{
	return State == EState::Moving && Moves.IsValidIndex(CurrentMove) ? Moves[CurrentMove].Name : NAME_None;
}

// --- Thinking ------------------------------------------------------------------------------------------------------------

void UOnyxBossComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AOnyxCharacter* Self = Character.Get();
	if (!Self || State == EState::Waiting || State == EState::Defeated)
	{
		return;
	}
	// The time here runs at the phase's speed (the actor's time dilation): its tells shorten as the fight goes on.
	for (float& Cooldown : Cooldowns)
	{
		Cooldown = FMath::Max(Cooldown - DeltaTime, 0.f);
	}
	GraceLeft = FMath::Max(GraceLeft - DeltaTime, 0.f);
	switch (State)
	{
	case EState::Resting:
	{
		FacePlayer();
		RestLeft -= DeltaTime;
		// Far from the player it closes in while it thinks; near, it stands its ground.
		const APawn* Player = GetPlayer();
		const float ToPlayer = Player ? Player->GetActorLocation().X - Self->GetActorLocation().X : 0.f;
		if (FMath::Abs(ToPlayer) > 650.f && Movement.IsValid() && Movement->IsMovingOnGround())
		{
			Self->AddMovementInput(FVector(OnyxBoss::SideOf(ToPlayer), 0.f, 0.f), 0.6f);
		}
		if (RestLeft <= 0.f)
		{
			const int32 Next = PickMove();
			if (Next != INDEX_NONE)
			{
				ChainCount = 0;
				StartMove(Next, false);
			}
			else
			{
				RestLeft = 0.25f;
			}
		}
		break;
	}
	case EState::Moving:
	{
		StepTime += DeltaTime;
		TickStep(DeltaTime);
		if (State != EState::Moving)
		{
			break; // the step ended the move (a stagger, a phase)
		}
		const FOnyxBossStep& Step = Moves[CurrentMove].Steps[StepIndex];
		if (StepTime >= Step.Time)
		{
			EndStep();
			++StepIndex;
			if (StepIndex < Moves[CurrentMove].Steps.Num())
			{
				BeginStep();
			}
			else
			{
				FinishMove();
			}
		}
		break;
	}
	case EState::Staggered:
	{
		StaggerLeft -= DeltaTime;
		if (UOnyxMovementComponent* Move = Movement.Get(); Move && Move->IsMovingOnGround())
		{
			Move->Velocity.X = 0.f;
		}
		if (StaggerLeft <= 0.f)
		{
			Self->MovesetComponent->bAutoLocomotion = true;
			GraceLeft = StaggerGrace;
			LogState(TEXT("back on its feet"));
			const int32 After = FindMove(AfterStaggerMove);
			if (After != INDEX_NONE)
			{
				StartMove(After, false);
			}
			else
			{
				Rest();
			}
		}
		break;
	}
	default:
		break;
	}
}

APawn* UOnyxBossComponent::GetPlayer() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

int32 UOnyxBossComponent::FindMove(FName Name) const
{
	if (Name.IsNone())
	{
		return INDEX_NONE;
	}
	return Moves.IndexOfByPredicate([Name](const FOnyxBossMove& Move) { return Move.Name == Name; });
}

int32 UOnyxBossComponent::PickMove() const
{
	const APawn* Player = GetPlayer();
	const AOnyxCharacter* Self = Character.Get();
	const UOnyxHealthComponent* PlayerHealth = Player ? Player->FindComponentByClass<UOnyxHealthComponent>() : nullptr;
	if (!Player || !Self || (PlayerHealth && PlayerHealth->IsDead()))
	{
		return INDEX_NONE; // the player is down: it stands and waits
	}
	const float Distance = FMath::Abs(Player->GetActorLocation().X - Self->GetActorLocation().X);
	TArray<int32> Fits;
	for (int32 Index = 0; Index < Moves.Num(); ++Index)
	{
		const FOnyxBossMove& Move = Moves[Index];
		if (Move.Weight > 0.f && !Move.Steps.IsEmpty() && Phase >= Move.FirstPhase && Phase <= Move.LastPhase && Cooldowns[Index] <= 0.f
			&& Distance >= Move.MinRange && Distance <= Move.MaxRange)
		{
			Fits.Add(Index);
		}
	}
	// Not the same move twice running when there is a choice: patterns, not repetition.
	if (Fits.Num() > 1)
	{
		Fits.Remove(LastMove);
	}
	float Total = 0.f;
	for (const int32 Index : Fits)
	{
		Total += Moves[Index].Weight;
	}
	float Roll = FMath::FRand() * Total;
	for (const int32 Index : Fits)
	{
		Roll -= Moves[Index].Weight;
		if (Roll <= 0.f)
		{
			return Index;
		}
	}
	return Fits.IsEmpty() ? INDEX_NONE : Fits.Last();
}

void UOnyxBossComponent::StartMove(int32 Index, bool bForced)
{
	if (!Moves.IsValidIndex(Index) || Moves[Index].Steps.IsEmpty())
	{
		Rest();
		return;
	}
	State = EState::Moving;
	CurrentMove = Index;
	LastMove = Index;
	HitsSinceMove = 0;
	++ChainCount;
	bForcedMove = bForced;
	StepIndex = 0;
	Cooldowns[Index] = Moves[Index].Cooldown;
	UE_LOG(LogOnyxBoss, Log, TEXT("%s: %s (phase %d, health %.0f%%)"), *GetNameSafe(GetOwner()), *Moves[Index].Name.ToString(), Phase,
		GetHealthFraction() * 100.f);
	BeginStep();
}

void UOnyxBossComponent::BeginStep()
{
	AOnyxCharacter* Self = Character.Get();
	UOnyxMovementComponent* Move = Movement.Get();
	if (!Self || !Move)
	{
		return;
	}
	const FOnyxBossStep& Step = Moves[CurrentMove].Steps[StepIndex];
	StepTime = 0.f;
	ShotTimer = 0.f;
	StepStart = Self->GetActorLocation();
	LastX = StepStart.X;
	StuckTime = 0.f;
	ShotsFired = 0;
	SetHidden(Step.bHidden);
	SetFlying(Step.bFlying);
	Self->Health->SetInvulnerable(bForcedMove || Step.bInvulnerable || Step.bHidden);
	if (Step.Action == EOnyxBossAction::Teleport)
	{
		const FVector Place = FindPlace(Step.Place, Step.Distance);
		Self->SetActorLocation(Place, false, nullptr, ETeleportType::TeleportPhysics);
		Move->StopMovementImmediately();
		UE_LOG(LogOnyxBoss, Verbose, TEXT("%s: comes back at X %.0f, %.0f cm over the arena floor"), *GetNameSafe(GetOwner()), Place.X,
			Arena.IsValid ? Place.Z - Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - Arena.Min.Z : 0.f);
	}
	if (Step.bFacePlayer)
	{
		FacePlayer();
	}
	if (!Step.Clip.IsNone())
	{
		Self->MovesetComponent->PlayOneShot(Step.Clip);
	}
	else if (Step.Action == EOnyxBossAction::Glide || Step.Action == EOnyxBossAction::Rush)
	{
		Self->MovesetComponent->StopOneShot(); // on the move: its moving clip, not a pose from before
	}
	TrailTimer = 0.f;
	UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this);
	if (Step.bTell && Feedback)
	{
		Feedback->FlashActor(Self, TellColor, 0.12f);
	}
	if (Step.Trauma > 0.f && Feedback)
	{
		Feedback->AddTrauma(Step.Trauma);
	}
	const FVector Here = Self->GetActorLocation();
	UOnyxAudioSubsystem::PlaySoundAt(this, Step.Sound, Here, EOnyxSoundCategory::Effects);
	if (Step.Effect)
	{
		AOnyxFlipbookEffect::Spawn(this, Step.Effect, Here + FVector(FacingSign() * Step.EffectOffset.X, 0.f, Step.EffectOffset.Y));
	}
	switch (Step.Action)
	{
	case EOnyxBossAction::Strike:
		SetHitbox(Step, true);
		break;
	case EOnyxBossAction::Rush:
		RushDirection = FacingSign();
		Move->RunSpeed = Step.Speed;
		Move->ApplyTuning();
		SetHitbox(Step, true);
		break;
	case EOnyxBossAction::Glide:
		Move->RunSpeed = Step.Speed;
		Move->ApplyTuning();
		break;
	case EOnyxBossAction::Leap:
		if (const APawn* Player = GetPlayer())
		{
			// Up Height and across to where the player stands, landing as it comes down.
			const float Gravity = FMath::Max(FMath::Abs(Move->GetGravityZ()), 1.f);
			const float Up = FMath::Sqrt(2.f * Gravity * Step.Height);
			const float Airtime = 2.f * Up / Gravity;
			const float Across = FMath::Clamp((Player->GetActorLocation().X - Here.X) / Airtime, -Step.Speed, Step.Speed);
			Self->LaunchCharacter(FVector(Across, 0.f, Up), true, true);
		}
		SetHitbox(Step, true);
		break;
	case EOnyxBossAction::Shoot:
		if (Step.ShotInterval <= 0.f)
		{
			for (int32 Shot = 0; Shot < Step.Shots; ++Shot)
			{
				FireShot(Step, Shot);
			}
			ShotsFired = Step.Shots;
		}
		break;
	case EOnyxBossAction::Summon:
		SummonMinions(Step);
		break;
	default:
		break;
	}
}

void UOnyxBossComponent::TickStep(float DeltaTime)
{
	AOnyxCharacter* Self = Character.Get();
	UOnyxMovementComponent* Move = Movement.Get();
	if (!Self || !Move)
	{
		return;
	}
	const FOnyxBossStep& Step = Moves[CurrentMove].Steps[StepIndex];
	const APawn* Player = GetPlayer();
	switch (Step.Action)
	{
	case EOnyxBossAction::Glide:
		if (Player)
		{
			const float ToPlayer = Player->GetActorLocation().X - Self->GetActorLocation().X;
			const bool bFar = FMath::Abs(ToPlayer) > Step.Distance;
			if (Step.bFlying)
			{
				Move->Velocity = FVector(bFar ? OnyxBoss::SideOf(ToPlayer) * Step.Speed : 0.f, 0.f, 0.f);
			}
			else if (bFar)
			{
				Self->AddMovementInput(FVector(OnyxBoss::SideOf(ToPlayer), 0.f, 0.f));
			}
			FacePlayer();
		}
		break;
	case EOnyxBossAction::Rush:
	{
		// Straight on until something stops it.
		const UCapsuleComponent* Capsule = Self->GetCapsuleComponent();
		const FVector From = Self->GetActorLocation();
		const FVector Ahead = From + FVector(RushDirection * (Capsule->GetScaledCapsuleRadius() + 30.f), 0.f, 0.f);
		FHitResult Hit;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(OnyxBossRush), false, Self);
		if (GetWorld()->LineTraceSingleByObjectType(Hit, From, Ahead, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
			{
				Feedback->AddTrauma(0.3f); // into the wall
			}
			StepTime = Step.Time;
			break;
		}
		// Anything else that stops it (a low ceiling, a body that blocks): the rush is over, never a run on the spot.
		const float Moved = FMath::Abs(From.X - LastX);
		LastX = From.X;
		StuckTime = StepTime > 0.1f && Moved < Step.Speed * DeltaTime * 0.2f ? StuckTime + DeltaTime : 0.f;
		if (StuckTime > 0.05f)
		{
			if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
			{
				Feedback->AddTrauma(0.3f);
			}
			StepTime = Step.Time;
			break;
		}
		Move->Velocity.X = RushDirection * Step.Speed;
		Self->AddMovementInput(FVector(RushDirection, 0.f, 0.f));
		break;
	}
	case EOnyxBossAction::Shoot:
		if (Step.ShotInterval > 0.f)
		{
			ShotTimer += DeltaTime;
			while (ShotsFired < Step.Shots && ShotTimer >= ShotsFired * Step.ShotInterval)
			{
				FireShot(Step, ShotsFired++);
			}
		}
		break;
	default:
		break;
	}
	if (Step.bFlying && Step.Action != EOnyxBossAction::Glide)
	{
		Move->Velocity = FVector::ZeroVector; // it hangs where it is
	}
	if (Step.Trail && !bHidden)
	{
		TrailTimer += DeltaTime;
		if (TrailTimer >= Step.TrailInterval)
		{
			TrailTimer = 0.f;
			AOnyxFlipbookEffect::Spawn(this, Step.Trail, Self->GetActorLocation() - FVector(FacingSign() * 40.f, 0.f, 30.f));
		}
	}
}

void UOnyxBossComponent::EndStep()
{
	const FOnyxBossStep& Step = Moves[CurrentMove].Steps[StepIndex];
	SetHitbox(Step, false);
	if (Step.Action == EOnyxBossAction::Rush || Step.Action == EOnyxBossAction::Glide)
	{
		if (const AActor* Owner = GetOwner())
		{
			UE_LOG(LogOnyxBoss, Verbose, TEXT("%s: %s took it %.0f cm (moving %d, speed %.0f)"), *GetNameSafe(Owner),
				Step.Action == EOnyxBossAction::Rush ? TEXT("the rush") : TEXT("the glide"), Owner->GetActorLocation().X - StepStart.X,
				Movement.IsValid() ? int32(Movement->MovementMode.GetValue()) : -1, Movement.IsValid() ? Movement->Velocity.Size() : 0.f);
		}
		if (UOnyxMovementComponent* Move = Movement.Get())
		{
			Move->RunSpeed = BaseRunSpeed;
			Move->ApplyTuning();
			Move->Velocity.X = 0.f;
		}
	}
}

void UOnyxBossComponent::FinishMove()
{
	AOnyxCharacter* Self = Character.Get();
	if (!Self)
	{
		return;
	}
	const FOnyxBossMove& Move = Moves[CurrentMove];
	SetHidden(false);
	SetFlying(false);
	Self->Health->SetInvulnerable(false);
	bForcedMove = false;
	if (PendingPhase != INDEX_NONE)
	{
		const int32 Next = PendingPhase;
		PendingPhase = INDEX_NONE;
		EnterPhase(Next);
		return;
	}
	// What comes next: one of its follow-ups (straight away), or a pause.
	const float Chance = Move.FollowUpChance.IsEmpty() ? 0.f : Move.FollowUpChance[FMath::Min(Phase, Move.FollowUpChance.Num() - 1)];
	TArray<int32> Next;
	for (const FName& Name : Move.FollowUps)
	{
		const int32 Index = FindMove(Name);
		if (Index != INDEX_NONE && Phase >= Moves[Index].FirstPhase && Phase <= Moves[Index].LastPhase)
		{
			Next.Add(Index); // the ones this stage of the fight uses
		}
	}
	if (!Next.IsEmpty() && ChainCount < MaxChain && FMath::FRand() < Chance)
	{
		StartMove(Next[FMath::RandRange(0, Next.Num() - 1)], false);
		return;
	}
	Rest();
}

void UOnyxBossComponent::ForceMove(FName Name)
{
	const int32 Index = FindMove(Name);
	if (Index != INDEX_NONE && bFightActive && !bDefeated)
	{
		Interrupt();
		ChainCount = 0;
		StartMove(Index, false);
	}
}

void UOnyxBossComponent::Interrupt()
{
	AOnyxCharacter* Self = Character.Get();
	if (!Self)
	{
		return;
	}
	if (State == EState::Moving && Moves.IsValidIndex(CurrentMove) && Moves[CurrentMove].Steps.IsValidIndex(StepIndex))
	{
		EndStep();
	}
	SetHidden(false);
	SetFlying(false);
	Self->Health->SetInvulnerable(false);
	Self->MovesetComponent->StopOneShot();
	Self->MovesetComponent->bAutoLocomotion = true;
	bForcedMove = false;
	CurrentMove = INDEX_NONE;
	State = EState::Resting;
}

void UOnyxBossComponent::Rest()
{
	ChainCount = 0;
	HitsSinceMove = 0; // the answer counts the blows of this rest: an opening it gave is the player's to take
	if (AOnyxCharacter* Self = Character.Get())
	{
		// Whatever the move's last clip had left is cut: from here its movement picks its clips (no sliding in a pose).
		Self->MovesetComponent->StopOneShot();
	}
	State = EState::Resting;
	CurrentMove = INDEX_NONE;
	const FVector2D Range = Phases[Phase].Rest;
	RestLeft = FMath::FRandRange(FMath::Min(Range.X, Range.Y), FMath::Max(Range.X, Range.Y));
}

void UOnyxBossComponent::Stagger()
{
	AOnyxCharacter* Self = Character.Get();
	if (!Self || !bFightActive || bDefeated)
	{
		return;
	}
	Interrupt();
	State = EState::Staggered;
	StaggerLeft = StaggerTime;
	HitsSinceStagger = 0;
	if (Self->MovesetComponent->HasClip(StaggerClip))
	{
		Self->MovesetComponent->bAutoLocomotion = false;
		Self->MovesetComponent->PlayClip(StaggerClip);
	}
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
	{
		Feedback->HitStop(0.12f);
		Feedback->AddTrauma(0.35f);
		Feedback->FlashActor(Self, FLinearColor::White, 0.2f);
	}
	UOnyxAudioSubsystem::PlaySoundAt(this, StaggerSound, Self->GetActorLocation(), EOnyxSoundCategory::Effects);
	LogState(TEXT("staggered"));
	OnStaggered.Broadcast();
}

void UOnyxBossComponent::EnterPhase(int32 NewPhase)
{
	AOnyxCharacter* Self = Character.Get();
	if (!Self || !Phases.IsValidIndex(NewPhase))
	{
		return;
	}
	Interrupt();
	Phase = NewPhase;
	HitsSinceStagger = 0;
	Self->CustomTimeDilation = Phases[Phase].Speed;
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
	{
		Feedback->HitStop(0.15f);
		Feedback->AddTrauma(0.5f);
	}
	UE_LOG(LogOnyxBoss, Log, TEXT("%s: phase %d (health %.0f%%)"), *GetNameSafe(GetOwner()), Phase, GetHealthFraction() * 100.f);
	OnPhaseChanged.Broadcast(Phase);
	const int32 Opening = FindMove(Phases[Phase].OpeningMove);
	if (Opening != INDEX_NONE)
	{
		StartMove(Opening, true);
	}
	else
	{
		Rest();
	}
}

void UOnyxBossComponent::HandleDamaged(const FOnyxDamageInfo& Damage)
{
	if (!bFightActive || bDefeated)
	{
		return;
	}
	// A share of health that opens the next stage: straight away, or after the opening under way.
	const float Fraction = GetHealthFraction();
	int32 Target = Phase;
	for (int32 Index = Phase + 1; Index < Phases.Num(); ++Index)
	{
		if (Fraction <= Phases[Index].HealthFraction)
		{
			Target = Index;
		}
	}
	if (Target > Phase && Fraction > 0.f)
	{
		if (bForcedMove)
		{
			PendingPhase = Target;
		}
		else
		{
			EnterPhase(Target);
		}
		return;
	}
	if (State == EState::Staggered || bForcedMove)
	{
		return;
	}
	// Just back on its feet, blows land but don't knock it down again yet.
	if (GraceLeft <= 0.f)
	{
		++HitsSinceStagger;
		if (StaggerHits > 0 && HitsSinceStagger >= StaggerHits)
		{
			Stagger();
			return;
		}
	}
	// Hacked at between its moves: past Answer Hits it answers at once instead of resting on.
	++HitsSinceMove;
	if (State == EState::Resting && !AnswerMove.IsNone() && HitsSinceMove >= AnswerHits)
	{
		const int32 Answer = FindMove(AnswerMove);
		if (Answer != INDEX_NONE && Cooldowns[Answer] <= 0.f)
		{
			LogState(TEXT("answers the blows"));
			ChainCount = 0;
			StartMove(Answer, false);
		}
	}
}

void UOnyxBossComponent::HandleDeath(AActor* Killer)
{
	AOnyxCharacter* Self = Character.Get();
	if (!Self || bDefeated)
	{
		return;
	}
	// Not Interrupt: the character is already playing its death and holding it.
	if (State == EState::Moving && Moves.IsValidIndex(CurrentMove) && Moves[CurrentMove].Steps.IsValidIndex(StepIndex))
	{
		SetHitbox(Moves[CurrentMove].Steps[StepIndex], false);
	}
	SetHidden(false);
	SetFlying(false);
	State = EState::Defeated;
	bDefeated = true;
	bFightActive = false;
	DefeatTime = GetWorld()->GetRealTimeSeconds();
	Self->CustomTimeDilation = 1.f;
	KillSummons();
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
	{
		Feedback->HitStop(DeathSlowMotionTime, DeathSlowMotion);
		Feedback->AddTrauma(0.8f);
		Feedback->FlashActor(Self, FLinearColor::White, 0.3f);
	}
	if (DeathEffect)
	{
		AOnyxFlipbookEffect::Spawn(this, DeathEffect, Self->GetActorLocation(), 2.f);
	}
	LogState(TEXT("defeated"));
	OnDefeated.Broadcast();
}

// --- Doing ---------------------------------------------------------------------------------------------------------------

float UOnyxBossComponent::FacingSign() const
{
	const AOnyxCharacter* Self = Character.Get();
	return Self && Self->GetFacing() == EOnyxFacing::Left ? -1.f : 1.f;
}

void UOnyxBossComponent::FacePlayer()
{
	AOnyxCharacter* Self = Character.Get();
	const APawn* Player = GetPlayer();
	if (Self && Player)
	{
		Self->SetFacing(Player->GetActorLocation().X >= Self->GetActorLocation().X ? EOnyxFacing::Right : EOnyxFacing::Left);
	}
}

void UOnyxBossComponent::SetHidden(bool bInHidden)
{
	AOnyxCharacter* Self = Character.Get();
	if (!Self || bInHidden == bHidden)
	{
		return;
	}
	bHidden = bInHidden;
	if (UPaperFlipbookComponent* Sprite = Self->GetSprite())
	{
		Sprite->SetVisibility(!bHidden);
	}
	TArray<UOnyxLight2DComponent*> Lights;
	Self->GetComponents(Lights);
	for (UOnyxLight2DComponent* Light : Lights)
	{
		Light->SetVisibility(!bHidden);
	}
	// Vanished: the player walks through where it was, and nothing about it hurts.
	Self->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, bHidden || bPassThroughFighters ? ECR_Ignore : ECR_Block);
	if (UOnyxContactDamageComponent* Contact = Self->FindComponentByClass<UOnyxContactDamageComponent>())
	{
		bHidden ? Contact->EndHitWindow() : Contact->BeginHitWindow();
	}
}

void UOnyxBossComponent::SetFlying(bool bFlying)
{
	UOnyxMovementComponent* Move = Movement.Get();
	if (!Move)
	{
		return;
	}
	if (bFlying && Move->MovementMode != MOVE_Flying)
	{
		Move->SetMovementMode(MOVE_Flying);
		Move->Velocity = FVector::ZeroVector;
	}
	else if (!bFlying && Move->MovementMode == MOVE_Flying)
	{
		Move->SetMovementMode(MOVE_Falling);
	}
}

void UOnyxBossComponent::SetHitbox(const FOnyxBossStep& Step, bool bOpen)
{
	AOnyxCharacter* Self = Character.Get();
	UOnyxHitboxComponent* Hitbox = Self ? Self->AttackHitbox.Get() : nullptr;
	const bool bUsesHitbox = Step.Action == EOnyxBossAction::Strike || Step.Action == EOnyxBossAction::Rush || Step.Action == EOnyxBossAction::Leap;
	if (!Hitbox || bOpen == bHitboxOpen || (bOpen && !bUsesHitbox))
	{
		return;
	}
	bHitboxOpen = bOpen;
	if (bOpen)
	{
		Hitbox->Damage = Step.Damage;
		Hitbox->Knockback = Step.Knockback;
		Hitbox->bClashable = Step.bClashable;
		if (!Step.HitboxSize.IsNearlyZero())
		{
			Self->SetAttackShape(Step.HitboxOffset, Step.HitboxSize);
		}
		Hitbox->BeginHitWindow();
	}
	else
	{
		Hitbox->EndHitWindow();
		Self->ClearAttackShape();
	}
}

float UOnyxBossComponent::FloorZ(float X, float FromZ) const
{
	const AOnyxCharacter* Self = Character.Get();
	if (!Self)
	{
		return 0.f;
	}
	const float Half = Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Bottom = Arena.IsValid ? Arena.Min.Z - 400.f : FromZ - 1200.f;
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(OnyxBossFloor), false, Self);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(X, 0.f, FromZ), FVector(X, 0.f, Bottom), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return Hit.ImpactPoint.Z + Half + 2.f;
	}
	return Self->GetActorLocation().Z;
}

float UOnyxBossComponent::LowZ() const
{
	const AOnyxCharacter* Self = Character.Get();
	return Arena.IsValid ? Arena.Min.Z + 150.f : (Self ? Self->GetActorLocation().Z + 100.f : 0.f);
}

FVector UOnyxBossComponent::FindPlace(EOnyxBossPlace Place, float Distance) const
{
	const AOnyxCharacter* Self = Character.Get();
	const APawn* Player = GetPlayer();
	if (!Self)
	{
		return FVector::ZeroVector;
	}
	const FVector Here = Self->GetActorLocation();
	const FBox Box = Arena.IsValid ? Arena : FBox(Here - FVector(1000.f, 100.f, 500.f), Here + FVector(1000.f, 100.f, 500.f));
	const float Margin = Self->GetCapsuleComponent()->GetScaledCapsuleRadius() + OnyxBoss::ArenaMargin;
	const float Left = Box.Min.X + Margin;
	const float Right = Box.Max.X - Margin;
	const float Middle = (Left + Right) * 0.5f;
	const float PlayerX = Player ? Player->GetActorLocation().X : Middle;
	const AOnyxCharacter* PlayerCharacter = Cast<AOnyxCharacter>(Player);
	const float PlayerFacing = PlayerCharacter && PlayerCharacter->GetFacing() == EOnyxFacing::Left ? -1.f : 1.f;
	// A side of the player, the other side when the wall is in the way.
	auto Beside = [&](float Side)
	{
		const float First = PlayerX + Side * Distance;
		const float X = (First < Left || First > Right) ? PlayerX - Side * Distance : First;
		return FMath::Clamp(X, Left, Right);
	};
	float X = Here.X;
	switch (Place)
	{
	case EOnyxBossPlace::BehindPlayer:
		X = Beside(-PlayerFacing);
		break;
	case EOnyxBossPlace::InFrontOfPlayer:
		X = Beside(PlayerFacing);
		break;
	case EOnyxBossPlace::FarFromPlayer:
		X = PlayerX < Middle ? Right : Left;
		break;
	case EOnyxBossPlace::ArenaCentre:
		X = Middle;
		break;
	case EOnyxBossPlace::ArenaTop:
	{
		const float Half = Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float UnderRoof = Box.Max.Z - Half - OnyxBoss::ArenaMargin;
		return FVector(Middle, Here.Y, Distance > 0.f ? FMath::Min(Box.Min.Z + Distance + Half, UnderRoof) : UnderRoof);
	}
	case EOnyxBossPlace::Start:
		return StartLocation;
	}
	return FVector(X, Here.Y, FloorZ(X, LowZ()));
}

void UOnyxBossComponent::FireShot(const FOnyxBossStep& Step, int32 Index)
{
	const AOnyxCharacter* Self = Character.Get();
	if (!Self)
	{
		return;
	}
	const FVector From = Self->GetActorLocation() + FVector(FacingSign() * Step.Muzzle.X, 0.f, Step.Muzzle.Y);
	const float Fan = FMath::DegreesToRadians((Index - (Step.Shots - 1) * 0.5f) * Step.Spread);
	auto Rotated = [Fan](const FVector& Aim)
	{
		return FVector(Aim.X * FMath::Cos(Fan) - Aim.Z * FMath::Sin(Fan), 0.f, Aim.X * FMath::Sin(Fan) + Aim.Z * FMath::Cos(Fan));
	};
	FVector Direction(FacingSign(), 0.f, 0.f);
	switch (Step.Aim)
	{
	case EOnyxBossAim::AtPlayer:
		if (const APawn* Player = GetPlayer())
		{
			Direction = Rotated(Player->GetActorLocation() + FVector(0.f, 0.f, 20.f) - From);
		}
		break;
	case EOnyxBossAim::Down:
		Direction = Rotated(FVector(0.f, 0.f, -1.f));
		break;
	case EOnyxBossAim::AcrossFloor:
	{
		// Shot after shot further along the floor, with a little give, so the safe places move.
		const FBox Box = Arena.IsValid ? Arena : FBox(From - FVector(800.f, 0.f, 600.f), From + FVector(800.f, 0.f, 0.f));
		const float Along = (Index + 0.5f) / FMath::Max(Step.Shots, 1);
		const float Margin = 60.f;
		const float T = SweepDirection > 0.f ? Along : 1.f - Along;
		const float X = FMath::Lerp(Box.Min.X + Margin, Box.Max.X - Margin, T) + FMath::FRandRange(-30.f, 30.f);
		const float Floor = FloorZ(X, LowZ()) - Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Direction = FVector(X, From.Y, Floor) - From;
		if (Index == Step.Shots - 1)
		{
			SweepDirection = -SweepDirection; // the next sweep comes back the other way
		}
		break;
	}
	}
	AOnyxProjectile::Fire(const_cast<UOnyxBossComponent*>(this), Step.Projectile, From, Direction, const_cast<AOnyxCharacter*>(Self));
}

void UOnyxBossComponent::SummonMinions(const FOnyxBossStep& Step)
{
	AOnyxCharacter* Self = Character.Get();
	if (!Self || !Step.SummonClass)
	{
		return;
	}
	const APawn* Player = GetPlayer();
	const FVector Here = Self->GetActorLocation();
	const FBox Box = Arena.IsValid ? Arena : FBox(Here - FVector(600.f, 100.f, 300.f), Here + FVector(600.f, 100.f, 300.f));
	const float Left = Box.Min.X + 120.f;
	const float Right = Box.Max.X - 120.f;
	const float PlayerX = Player ? Player->GetActorLocation().X : Here.X;
	for (int32 Index = 0; Index < Step.SummonCount; ++Index)
	{
		// Spread over the arena, never on top of the player.
		float X = FMath::Lerp(Left, Right, (Index + 0.5f) / Step.SummonCount);
		if (FMath::Abs(X - PlayerX) < 300.f)
		{
			X = FMath::Clamp(PlayerX + (X >= PlayerX ? 300.f : -300.f), Left, Right);
		}
		const FVector Where(X, Here.Y, FloorZ(X, LowZ()) + 40.f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (AActor* Minion = GetWorld()->SpawnActor<AActor>(Step.SummonClass, Where, FRotator::ZeroRotator, Params))
		{
			Summoned.Add(Minion);
			if (Step.Effect)
			{
				AOnyxFlipbookEffect::Spawn(this, Step.Effect, Where);
			}
		}
	}
	Summoned.RemoveAll([](const TWeakObjectPtr<AActor>& Minion) { return !Minion.IsValid(); });
}

void UOnyxBossComponent::KillSummons()
{
	for (const TWeakObjectPtr<AActor>& Weak : Summoned)
	{
		AActor* Minion = Weak.Get();
		if (!Minion)
		{
			continue;
		}
		UOnyxHealthComponent* Health = Minion->FindComponentByClass<UOnyxHealthComponent>();
		if (Health && !Health->IsDead())
		{
			Health->Kill(nullptr); // its master is gone: it falls with it
		}
		else if (!Health)
		{
			Minion->Destroy();
		}
	}
	Summoned.Reset();
}

void UOnyxBossComponent::LogState(const TCHAR* What) const
{
	UE_LOG(LogOnyxBoss, Log, TEXT("%s: %s (phase %d, health %.0f%%)"), *GetNameSafe(GetOwner()), What, Phase, GetHealthFraction() * 100.f);
}
