// Copyright Onyx2D. All Rights Reserved.

#include "Combat/OnyxCombatComponent.h"
#include "AI/OnyxBossComponent.h"
#include "AI/OnyxEnemyAIComponent.h"
#include "Character/OnyxCharacter.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxClipWindow.h"
#include "Animation/OnyxMovesetComponent.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Combat/OnyxCombatLibrary.h"
#include "Combat/OnyxHealthComponent.h"
#include "Combat/OnyxHitboxComponent.h"
#include "Combat/OnyxProjectile.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"
#include "Feedback/OnyxFeedbackSubsystem.h"
#include "GameFramework/Actor.h"
#include "Movement/OnyxMovementComponent.h"
#include "World/OnyxFlipbookEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxCombat, Log, All);

UOnyxCombatComponent::UOnyxCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	FOnyxAttack First;
	First.Clip = TEXT("Attack1");
	FOnyxAttack Second = First;
	Second.Clip = TEXT("Attack2");
	FOnyxAttack Finisher = First;
	Finisher.Clip = TEXT("Attack3");
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
		MovesetComponent = Owner->FindComponentByClass<UOnyxMovesetComponent>();
		Movement = Owner->FindComponentByClass<UOnyxMovementComponent>();
	}
}

bool UOnyxCombatComponent::CanStartAttack() const
{
	const uint8 Saved = ActiveCombo;
	const_cast<UOnyxCombatComponent*>(this)->ActiveCombo = PickCombo();
	const bool bNone = GetActiveCombo().IsEmpty();
	const_cast<UOnyxCombatComponent*>(this)->ActiveCombo = Saved;
	if (bNone)
	{
		return false;
	}
	if (!bAllowAirAttacks && Movement && Movement->IsFalling())
	{
		return false;
	}
	if (Movement && Movement->IsOnLedge())
	{
		return false; // hanging from a ledge or pulling up onto it: the hands are on the edge
	}
	if (Movement && Movement->IsTopDown() && Movement->IsSwimmingInWater())
	{
		return false; // from above, swimming: arms busy, head just out of the water
	}
	return !Movement || !Movement->IsDashing() || !DashCombo.IsEmpty();
}

const TArray<FOnyxAttack>& UOnyxCombatComponent::GetActiveCombo() const
{
	switch (ActiveCombo)
	{
	case 5:
		return RunCombo.IsEmpty() ? Combo : RunCombo;
	case 4:
		return DashCombo.IsEmpty() ? Combo : DashCombo;
	case 3:
		return UpCombo.IsEmpty() ? Combo : UpCombo;
	case 2:
		return !DownAirCombo.IsEmpty() ? DownAirCombo : (!AirCombo.IsEmpty() ? AirCombo : Combo);
	case 1:
		return AirCombo.IsEmpty() ? Combo : AirCombo;
	default:
		return Combo;
	}
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
	if (Index == 0)
	{
		ActiveCombo = PickCombo();
	}
	const FOnyxAttack& Attack = GetActiveCombo()[Index];
	if (AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner()))
	{
		// Each blow goes where the player steers now (turning between blows of a combo too), off a wall it slides down.
		Character->FaceForAttack();
		Attack.HitboxSize.IsNearlyZero() ? Character->ClearAttackShape() : Character->SetAttackShape(Attack.HitboxOffset, Attack.HitboxSize);
	}
	CurrentIndex = Index;
	Elapsed = 0.f;
	SetHitWindow(false);
	ClashedThisAttack.Reset();

	const float ClipLength = MovesetComponent ? MovesetComponent->PlayOneShot(Attack.Clip) : 0.f;
	CurrentDuration = ClipLength > 0.f ? ClipLength : Attack.Duration;
	const UOnyxClip* Clip = ClipLength > 0.f ? MovesetComponent->GetCurrentClip() : nullptr;
	bClipOwnsHitWindow = Clip && Clip->HasWindow(UOnyxClipWindow_Hitbox::StaticClass());

	if (Hitbox)
	{
		Hitbox->Damage = Attack.Damage;
		Hitbox->Knockback = Attack.Knockback;
	}
	if (Movement)
	{
		Movement->CancelDash(); // a strike out of a dash takes its speed over
		Movement->SetMovementLocked(Attack.bLockMovement);
		if (Attack.ForwardImpulse != 0.f)
		{
			const AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
			if (Movement->IsTopDown() && Character)
			{
				const FVector2D Aim = Character->GetAimVector(); // top down: a step along the way it strikes
				Movement->Velocity = FVector(Aim.X, 0.f, Aim.Y) * Attack.ForwardImpulse;
			}
			else
			{
				Movement->Velocity.X = FacingSign * Attack.ForwardImpulse;
			}
		}
	}
	UE_LOG(LogOnyxCombat, Verbose, TEXT("%s at X %.0f: %s attack %d %s (%s)"), *GetNameSafe(GetOwner()), GetOwner()->GetActorLocation().X,
		ActiveCombo == 5 ? TEXT("run") : ActiveCombo == 4 ? TEXT("dash") : ActiveCombo == 3 ? TEXT("up") : ActiveCombo == 2 ? TEXT("down-air") : ActiveCombo == 1 ? TEXT("air") : TEXT("ground"), Index,
		*LookingWay(), *Attack.Clip.ToString());
	OnAttackStarted.Broadcast(Index);
}

FString UOnyxCombatComponent::LookingWay() const
{
	// Top down, the way it looks (Down, Up Side...), the side ones with their left or right.
	const AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	const EOnyxDirection Direction = Character ? Character->GetDirection() : EOnyxDirection::None;
	const TCHAR* Side = FacingSign > 0.f ? TEXT("right") : TEXT("left");
	if (Direction == EOnyxDirection::None)
	{
		return FString::Printf(TEXT("facing %s"), Side);
	}
	const FString Way = UEnum::GetDisplayValueAsText(Direction).ToString();
	return Direction == EOnyxDirection::Up || Direction == EOnyxDirection::Down ? TEXT("looking ") + Way : FString::Printf(TEXT("looking %s (%s)"), *Way, Side);
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
	const FOnyxAttack& Attack = GetActiveCombo()[CurrentIndex];
	if (!bClipOwnsHitWindow)
	{
		SetHitWindow(Elapsed >= Attack.HitWindowStart && Elapsed < Attack.HitWindowEnd);
	}
	if ((bClash || bCutProjectiles) && Hitbox && Hitbox->IsHitWindowOpen())
	{
		CheckClashes();
		if (!IsAttacking())
		{
			return;
		}
	}

	if (BufferRemaining > 0.f && Elapsed >= Attack.ChainTime)
	{
		// The next press: a different kind of attack now (Up held, in the air...) starts that one; else the combo goes on,
		// and round again when it loops.
		const bool bOtherKind = PickCombo() != ActiveCombo && CanStartAttack();
		const bool bHasNext = GetActiveCombo().IsValidIndex(CurrentIndex + 1);
		if (bOtherKind || (!bHasNext && bLoopCombo && CanStartAttack()))
		{
			BufferRemaining = 0.f;
			StartAttack(0);
			return;
		}
		if (bHasNext)
		{
			BufferRemaining = 0.f;
			StartAttack(CurrentIndex + 1);
			return;
		}
	}
	if (Elapsed >= CurrentDuration)
	{
		FinishAttack();
	}
}

void UOnyxCombatComponent::SetHitbox(UOnyxHitboxComponent* InHitbox)
{
	if (Hitbox)
	{
		Hitbox->OnHit.RemoveDynamic(this, &UOnyxCombatComponent::HandleHit);
	}
	Hitbox = InHitbox;
	if (Hitbox)
	{
		Hitbox->OnHit.AddUniqueDynamic(this, &UOnyxCombatComponent::HandleHit);
	}
}

uint8 UOnyxCombatComponent::PickCombo() const
{
	if (!Movement)
	{
		return 0;
	}
	if (Movement->IsDashing() && !DashCombo.IsEmpty())
	{
		return 4;
	}
	if (Movement->IsTopDown())
	{
		// From above Up and Down are just ways to walk (a blow upwards is the combo's Up clips): no up or air combos.
		return !RunCombo.IsEmpty() && Movement->GetLocomotionState() == EOnyxLocomotion::Run ? 5 : 0;
	}
	if (Movement->IsUpHeld() && !UpCombo.IsEmpty())
	{
		return 3;
	}
	if (!Movement->IsFalling())
	{
		return !RunCombo.IsEmpty() && Movement->GetLocomotionState() == EOnyxLocomotion::Run ? 5 : 0;
	}
	return Movement->IsDownHeld() && !DownAirCombo.IsEmpty() ? 2 : 1;
}

void UOnyxCombatComponent::CheckClashes()
{
	AActor* Owner = GetOwner();
	const FBox Blade = Hitbox->Bounds.GetBox();
	if (bClash)
	{
		for (TActorIterator<AOnyxCharacter> It(GetWorld()); It; ++It)
		{
			AOnyxCharacter* Other = *It;
			const UOnyxHitboxComponent* TheirBlade = Other->AttackHitbox;
			if (Other == Owner || !TheirBlade || !TheirBlade->IsHitWindowOpen() || TheirBlade->bContinuous || !TheirBlade->bClashable || ClashedThisAttack.Contains(Other)
				|| Other->Health->IsDead() || !UOnyxCombatLibrary::AreHostile(Owner, Other))
			{
				continue;
			}
			const FBox Theirs = TheirBlade->Bounds.GetBox();
			if (Blade.Intersect(Theirs))
			{
				Clash(*Other, Blade.Overlap(Theirs).GetCenter());
				if (!IsAttacking())
				{
					return;
				}
			}
		}
	}
	if (bCutProjectiles)
	{
		for (TActorIterator<AOnyxProjectile> It(GetWorld()); It; ++It)
		{
			AOnyxProjectile* Shot = *It;
			AActor* Shooter = Shot->GetShooter();
			if (Shooter == Owner || !UOnyxCombatLibrary::AreHostile(Owner, Shooter ? Shooter : Shot) || !Blade.Intersect(Shot->Collision->Bounds.GetBox()))
			{
				continue;
			}
			// Cut out of the air: it bursts where it is, harmless.
			if (Hitbox->HitEffect)
			{
				AOnyxFlipbookEffect::Spawn(this, Hitbox->HitEffect, Shot->GetActorLocation());
			}
			Shot->Burst();
			if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
			{
				Feedback->HitStop(0.05f);
				Feedback->AddTrauma(0.1f);
			}
		}
	}
}

void UOnyxCombatComponent::Clash(AOnyxCharacter& Other, const FVector& Where)
{
	AActor* Owner = GetOwner();
	ClashedThisAttack.Add(&Other);
	// Neither blade bites this time.
	Hitbox->IgnoreThisWindow(&Other);
	Other.AttackHitbox->IgnoreThisWindow(Owner);
	if (UOnyxHealthComponent* Health = Owner->FindComponentByClass<UOnyxHealthComponent>())
	{
		Health->SetInvulnerableFor(ClashInvulnerability);
	}
	// Pushed apart (top down: straight away from each other across the ground).
	const bool bTopDown = Movement && Movement->IsTopDown();
	const FVector Apart = bTopDown
		? (Owner->GetActorLocation() - Other.GetActorLocation()) * FVector(1.f, 0.f, 1.f)
		: FVector(Owner->GetActorLocation().X >= Other.GetActorLocation().X ? 1.f : -1.f, 0.f, 0.f);
	const FVector Away = Apart.IsNearlyZero() ? FVector(-FacingSign, 0.f, 0.f) : Apart.GetSafeNormal();
	if (Movement && bTopDown)
	{
		Movement->Velocity = Away * ClashPush;
	}
	else if (Movement)
	{
		Movement->Velocity.X = Away.X * ClashPush;
	}
	if (AOnyxCharacter* Character = Cast<AOnyxCharacter>(Owner))
	{
		Character->HoldFacing(0.4f); // pushed back, still facing the foe
	}
	if (Other.KnockbackScale > 0.f)
	{
		Other.LaunchCharacter(-Away * ClashPush * Other.KnockbackScale, true, bTopDown);
	}
	// Its strike is over: off balance (a foe with Enemy AI), or its attack stopped (a fighter with combos). A boss's
	// moves go on: the clash only saved this character.
	if (UOnyxEnemyAIComponent* AI = Other.FindComponentByClass<UOnyxEnemyAIComponent>())
	{
		AI->Interrupt(ClashStagger);
	}
	else if (!Other.FindComponentByClass<UOnyxBossComponent>())
	{
		Other.Combat->CancelAttack();
	}
	UPaperFlipbook* Effect = ClashEffect ? ClashEffect.Get() : Hitbox->HitEffect.Get();
	if (Effect)
	{
		AOnyxFlipbookEffect::Spawn(this, Effect, Where);
	}
	UOnyxAudioSubsystem::PlaySoundAt(this, ClashSound ? ClashSound.Get() : Hitbox->HitSound.Get(), Where, EOnyxSoundCategory::Effects, 1.f, FMath::FRandRange(1.1f, 1.25f));
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
	{
		Feedback->HitStop(ClashHitStop);
		Feedback->AddTrauma(0.3f);
		Feedback->FlashActor(Owner, FLinearColor::White, 0.12f);
		Feedback->FlashActor(&Other, FLinearColor::White, 0.12f);
	}
	UE_LOG(LogOnyxCombat, Log, TEXT("%s: blades clashed with %s"), *GetNameSafe(Owner), *Other.GetName());
	OnClash.Broadcast(&Other);
}

void UOnyxCombatComponent::HandleHit(AActor* Victim)
{
	if (!IsAttacking() || !Movement || !Victim)
	{
		return;
	}
	const FOnyxAttack& Attack = GetActiveCombo()[CurrentIndex];
	UE_LOG(LogOnyxCombat, Verbose, TEXT("%s: hit %s%s"), *GetNameSafe(GetOwner()), *Victim->GetName(), Attack.Bounce > 0.f ? TEXT(", bounced up off it") : TEXT(""));
	if (Attack.Bounce > 0.f && !Movement->IsTopDown())
	{
		// A down-slash: up off the foe, air moves back.
		Movement->Velocity.Z = Attack.Bounce;
		Movement->SetMovementMode(MOVE_Falling);
		Movement->RefreshAirMoves();
		return;
	}
	if (Attack.Recoil <= 0.f)
	{
		return;
	}
	// Away from the victim, whichever way it was struck, still facing it (top down: straight back across the ground).
	if (Movement->IsTopDown())
	{
		const FVector Apart = (GetOwner()->GetActorLocation() - Victim->GetActorLocation()) * FVector(1.f, 0.f, 1.f);
		Movement->Velocity = (Apart.IsNearlyZero() ? FVector(-FacingSign, 0.f, 0.f) : Apart.GetSafeNormal()) * Attack.Recoil;
	}
	else
	{
		const float Away = GetOwner()->GetActorLocation().X >= Victim->GetActorLocation().X ? 1.f : -1.f;
		Movement->Velocity.X = Away * Attack.Recoil;
	}
	if (AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner()))
	{
		Character->HoldFacing(0.35f);
	}
}

void UOnyxCombatComponent::CancelAttack()
{
	if (IsAttacking())
	{
		if (MovesetComponent)
		{
			MovesetComponent->StopOneShot();
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
	if (AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner()))
	{
		Character->ClearAttackShape();
	}
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
