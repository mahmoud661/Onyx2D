// Copyright Onyx2D. All Rights Reserved.

#include "Combat/OnyxRangedComponent.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxMoveset.h"
#include "Animation/OnyxMovesetComponent.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Character/OnyxCharacter.h"
#include "Combat/OnyxCombatComponent.h"
#include "Combat/OnyxCombatTypes.h"
#include "Combat/OnyxEnergyComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Movement/OnyxMovementComponent.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "World/OnyxFlipbookEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxRanged, Log, All);

namespace OnyxRanged
{
	/** Where an upper body drawn alone rides the legs (on both), and where the shot leaves (on the upper body). */
	const FName UpperBodySocket(TEXT("UpperBody"));
	const FName ShotSocket(TEXT("Shot"));
	/** The movement clips drawn as legs alone. */
	const FName LegsVariant(TEXT("_Legs"));
}

UOnyxRangedComponent::UOnyxRangedComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true; // keeps the crosshair up; the rest only while a move plays
	// After the body and its clips have moved on this frame, so the upper body sits on the legs as they are shown.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

bool UOnyxRangedComponent::UsesUpperBody(const FOnyxRangedMove& Move) const
{
	return Move.bUpperBody && !UOnyx2DLibrary::IsTopDown(this);
}

FName UOnyxRangedComponent::GetUpperClip() const
{
	return UpperLayer && UpperLayer->IsVisible() ? UpperClip : NAME_None;
}

UPaperFlipbookComponent* UOnyxRangedComponent::GetUpperLayer()
{
	if (UpperLayer)
	{
		return UpperLayer;
	}
	AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	UPaperFlipbookComponent* Body = Character ? Character->GetSprite() : nullptr;
	if (!Body)
	{
		return nullptr;
	}
	UpperLayer = NewObject<UPaperFlipbookComponent>(Character, TEXT("OnyxUpperBody"));
	UpperLayer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UpperLayer->SetupAttachment(Body, OnyxRanged::UpperBodySocket);
	UpperLayer->RegisterComponent();
	UpperLayer->Stop();
	UpperLayer->SetLooping(false);
	// Drawn over the legs, in the body's own material (its light, its flashes).
	UpperLayer->TranslucencySortPriority = Body->TranslucencySortPriority + 1;
	UpperLayer->SetMaterial(0, Body->GetMaterial(0));
	UpperLayer->SetCastShadow(false);
	UpperLayer->SetVisibility(false);
	return UpperLayer;
}

void UOnyxRangedComponent::UpdateUpper()
{
	if (!UpperLayer)
	{
		return;
	}
	const AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	UPaperFlipbookComponent* Body = Character ? Character->GetSprite() : nullptr;
	const UOnyxMovesetComponent* Moveset = Character ? Character->MovesetComponent.Get() : nullptr;
	// Only over legs: a clip with no legs drawn alone (a dash, a roll) shows whole, as it is.
	const bool bShow = !UpperClip.IsNone() && Body && Moveset && Moveset->IsPlayingVariant() && Body->IsVisible();
	UpperLayer->SetVisibility(bShow);
	if (!bShow)
	{
		return;
	}
	UpperLayer->SetHiddenInGame(Body->bHiddenInGame); // blinking after a hit
	// Its anchor on the legs' (the layer hangs from their socket).
	const FVector Anchor = UpperLayer->DoesSocketExist(OnyxRanged::UpperBodySocket)
		? UpperLayer->GetSocketTransform(OnyxRanged::UpperBodySocket, RTS_Component).GetLocation() : FVector::ZeroVector;
	UpperLayer->SetRelativeLocation(-Anchor);
	// On the legs' socket as it is now: Paper2D moves what hangs from a socket only when the frame number changes, and two
	// clips of one frame each (standing, crouched) both show frame 0.
	UpperLayer->UpdateComponentToWorld();
	// Lit, flashing and fading as the body is; drawn a step in front of it (the draw order's step for a higher sort priority),
	// or the two would share one depth and the legs could show through the chest.
	const TArray<float>& From = Body->GetCustomPrimitiveData().Data;
	const TArray<float>& Mine = UpperLayer->GetCustomPrimitiveData().Data;
	for (int32 Index = 0; Index < From.Num(); ++Index)
	{
		const float Value = Index == Onyx2D::DrawOrderDataIndex ? From[Index] + Onyx2D::DrawOrderComponentStep : From[Index];
		if (!Mine.IsValidIndex(Index) || Mine[Index] != Value)
		{
			UpperLayer->SetCustomPrimitiveDataFloat(Index, Value);
		}
	}
}

void UOnyxRangedComponent::HideUpper()
{
	UpperClip = NAME_None;
	if (UpperLayer)
	{
		UpperLayer->Stop();
		UpperLayer->SetVisibility(false);
	}
}

bool UOnyxRangedComponent::DrawnShotPoint(const FOnyxRangedMove& Move, FVector& OutPoint) const
{
	UPaperFlipbook* Showing = UpperLayer && UsesUpperBody(Move) ? UpperLayer->GetFlipbook() : nullptr;
	if (!Showing)
	{
		return false;
	}
	// On the frame showing, else the last one before it that draws it (the bomb's last moment in the hand), else on the held
	// pose at this aim (the arrow on the drawn string).
	FTransform Local;
	bool bFound = false;
	for (int32 Frame = Showing->GetKeyFrameIndexAtTime(UpperLayer->GetPlaybackPosition()); Frame >= 0 && !bFound; --Frame)
	{
		bFound = Showing->FindSocket(OnyxRanged::ShotSocket, Frame, Local);
	}
	if (!bFound)
	{
		const AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
		const UOnyxMovesetComponent* Moveset = Character ? Character->MovesetComponent.Get() : nullptr;
		const UOnyxClip* Held = Moveset && Moveset->Moveset ? Moveset->Moveset->FindClip(ClipAt(Move.HoldClip, ShownAngle)) : nullptr;
		bFound = Held && Held->Flipbook && Held->Flipbook->FindSocket(OnyxRanged::ShotSocket, 0, Local);
	}
	if (bFound)
	{
		OutPoint = (Local * UpperLayer->GetComponentTransform()).GetLocation();
		OutPoint.Y = GetOwner()->GetActorLocation().Y; // on the plane it plays on
	}
	return bFound;
}

void UOnyxRangedComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UOnyxHealthComponent* Health = GetOwner()->FindComponentByClass<UOnyxHealthComponent>())
	{
		Health->OnDamaged.AddUniqueDynamic(this, &UOnyxRangedComponent::HandleDamaged);
	}
	ShowCrosshair();
}

double UOnyxRangedComponent::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UOnyxRangedComponent::ShowCrosshair()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!bAimWithMouse || !bShowCrosshair || !Player || !Player->IsLocalController() || UGameplayStatics::IsGamePaused(this))
	{
		return;
	}
	// Menus hide the cursor again when they close: put it back, as a crosshair.
	if (!Player->ShouldShowMouseCursor() || Player->CurrentMouseCursor != EMouseCursor::Crosshairs)
	{
		Player->SetShowMouseCursor(true);
		Player->CurrentMouseCursor = EMouseCursor::Crosshairs;
	}
}

bool UOnyxRangedComponent::MousePoint(FVector& OutPoint) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	FVector Origin, Direction;
	if (!bAimWithMouse || !Player || !Player->IsLocalController() || !Player->DeprojectMousePositionToWorld(Origin, Direction))
	{
		return false;
	}
	// Where the line under the cursor crosses the plane the character plays on.
	const float PlaneY = GetOwner()->GetActorLocation().Y;
	if (FMath::Abs(Direction.Y) < KINDA_SMALL_NUMBER)
	{
		OutPoint = FVector(Origin.X, PlaneY, Origin.Z);
		return true;
	}
	OutPoint = Origin + Direction * ((PlaneY - Origin.Y) / Direction.Y);
	return true;
}

FVector UOnyxRangedComponent::ShoulderLocation(const FOnyxRangedMove& Move) const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const float HalfHeight = Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	const FVector Feet = GetOwner()->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	return UOnyx2DLibrary::IsTopDown(this) ? Feet + FVector(0.f, 0.f, Move.Muzzle.Y * 0.5f) : Feet + FVector(0.f, 0.f, Move.Muzzle.Y);
}

FVector2D UOnyxRangedComponent::GetAimDirection() const
{
	const AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	if (!Character)
	{
		return FVector2D(1.f, 0.f);
	}
	const bool bTopDown = UOnyx2DLibrary::IsTopDown(this);
	const FVector2D Ahead = bTopDown ? Character->GetAimVector().GetSafeNormal() : FVector2D(Character->GetFacing() == EOnyxFacing::Left ? -1.f : 1.f, 0.f);
	if (bAimOverride)
	{
		const float A = FMath::DegreesToRadians(AimOverride);
		return FVector2D(Ahead.X * FMath::Cos(A), FMath::Sin(A)).GetSafeNormal();
	}
	if (StickAim.SizeSquared() > FMath::Square(0.35f))
	{
		return StickAim.GetSafeNormal();
	}
	if (!bTopDown)
	{
		// Up / Down held: up high ahead, or down ahead.
		if (const UOnyxMovementComponent* Movement = Character->GetOnyxMovement())
		{
			if (Movement->IsUpHeld())
			{
				return FVector2D(Ahead.X * 0.5f, 0.866f);
			}
			if (Movement->IsDownHeld() && !Movement->IsMovingOnGround())
			{
				return FVector2D(Ahead.X * 0.5f, -0.866f);
			}
		}
	}
	FVector Mouse;
	if (MousePoint(Mouse))
	{
		const FVector From = ShoulderLocation(Shoot);
		const FVector2D Toward(Mouse.X - From.X, Mouse.Z - From.Z);
		if (Toward.SizeSquared() > 1.f)
		{
			return Toward.GetSafeNormal();
		}
	}
	return Ahead;
}

float UOnyxRangedComponent::AimDegrees(const FOnyxRangedMove& Move) const
{
	const FVector2D Aim = GetAimDirection();
	float Degrees = FMath::RadiansToDegrees(FMath::Atan2(Aim.Y, FMath::Abs(Aim.X)));
	if (!Move.AimAngles.IsEmpty())
	{
		float Low = Move.AimAngles[0], High = Move.AimAngles[0];
		for (const float Angle : Move.AimAngles)
		{
			Low = FMath::Min(Low, Angle);
			High = FMath::Max(High, Angle);
		}
		Degrees = FMath::Clamp(Degrees, Low, High);
	}
	return Degrees;
}

float UOnyxRangedComponent::NearestDrawn(const FOnyxRangedMove& Move, float Degrees)
{
	float Best = 0.f, BestGap = TNumericLimits<float>::Max();
	for (const float Angle : Move.AimAngles)
	{
		if (FMath::Abs(Angle - Degrees) < BestGap)
		{
			BestGap = FMath::Abs(Angle - Degrees);
			Best = Angle;
		}
	}
	return Best;
}

const TCHAR* UOnyxRangedComponent::StateSuffix() const
{
	const AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	const UOnyxMovementComponent* Movement = Character ? Character->GetOnyxMovement() : nullptr;
	if (!Movement || UOnyx2DLibrary::IsTopDown(this))
	{
		return TEXT("");
	}
	if (Movement->IsSwimmingInWater())
	{
		return TEXT("_Swim");
	}
	if (Movement->IsFalling())
	{
		// Rising (the jump's legs) or coming down (the fall's), like the clips it plays when it isn't shooting.
		return Movement->Velocity.Z > 0.f ? TEXT("_Air") : TEXT("_Fall");
	}
	return FMath::Abs(Movement->Velocity.X) > 40.f ? TEXT("_Run") : TEXT("");
}

FName UOnyxRangedComponent::ClipAt(FName Clip, float Angle) const
{
	if (Clip.IsNone())
	{
		return Clip;
	}
	const AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	const UOnyxMovesetComponent* Moveset = Character ? Character->MovesetComponent.Get() : nullptr;
	const int32 Whole = FMath::RoundToInt(Angle);
	const FString Aimed = Whole == 0 ? FString() : FString::Printf(TEXT("_%s%d"), Whole > 0 ? TEXT("Up") : TEXT("Down"), FMath::Abs(Whole));
	// An upper body is one drawing whatever the legs do.
	const FString Doing = UsesUpperBody(MoveOf(Active)) ? FString() : FString(StateSuffix());
	// The most fitting drawing there is: what it is doing at the aim, what it is doing (coming down: else as rising), standing at
	// the aim, standing level.
	const FString Rising = Doing == TEXT("_Fall") ? FString(TEXT("_Air")) : Doing;
	for (const FString& Suffix : { Doing + Aimed, Doing, Rising + Aimed, Rising, Aimed })
	{
		const FName Drawn(*(Clip.ToString() + Suffix));
		if (!Suffix.IsEmpty() && Moveset && Moveset->HasClip(Drawn))
		{
			return Drawn;
		}
	}
	return Clip;
}

float UOnyxRangedComponent::PlayClip(FName Clip, bool bFromStart)
{
	const AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	UOnyxMovesetComponent* Moveset = Character ? Character->MovesetComponent.Get() : nullptr;
	if (!Moveset || Clip.IsNone())
	{
		return 0.f;
	}
	if (!UsesUpperBody(MoveOf(Active)))
	{
		return Moveset->HasClip(Clip) ? Moveset->PlayOneShot(Clip) : 0.f;
	}
	// Over the legs: on its own layer, the body playing on as it moves.
	const UOnyxClip* Found = Moveset->Moveset ? Moveset->Moveset->FindClip(Clip) : nullptr;
	UPaperFlipbookComponent* Layer = Found && Found->Flipbook ? GetUpperLayer() : nullptr;
	if (!Layer)
	{
		return 0.f;
	}
	const int32 Frame = Layer->GetPlaybackPositionInFrames();
	Layer->SetFlipbook(Found->Flipbook);
	Layer->SetPlayRate(Found->PlayRate);
	Layer->SetLooping(false);
	if (bFromStart)
	{
		Layer->PlayFromStart();
	}
	else
	{
		// The aim moved on: the same moment of the pose, drawn at the new angle.
		Layer->SetPlaybackPositionInFrames(FMath::Clamp(Frame, 0, FMath::Max(Layer->GetFlipbookLengthInFrames() - 1, 0)), false);
		Layer->Play();
	}
	UpperClip = Clip;
	UpdateUpper();
	return Found->GetLength() / FMath::Max(Found->PlayRate, 0.01f);
}

void UOnyxRangedComponent::TurnToAim()
{
	AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	const FVector2D Aim = GetAimDirection();
	if (UOnyx2DLibrary::IsTopDown(this))
	{
		Character->SetHeading(Aim);
		return;
	}
	// It turns once the aim is clearly the other way: aiming almost straight up or down (the cursor under its feet), a hair
	// either side of upright doesn't flip it back and forth.
	const float Ahead = Character->GetFacing() == EOnyxFacing::Left ? -1.f : 1.f;
	if (Aim.X * Ahead < -0.25f)
	{
		Character->SetFacing(Aim.X < 0.f ? EOnyxFacing::Left : EOnyxFacing::Right);
	}
	Character->HoldFacing(0.1f); // the way it aims, not the way it was going
}

float UOnyxRangedComponent::GetCharge() const
{
	if (State != EState::Starting && State != EState::Holding)
	{
		return 0.f;
	}
	const FOnyxRangedMove& Move = MoveOf(Active);
	return Move.ChargeTime > 0.f ? FMath::Clamp(float(Now() - PressedAt) / Move.ChargeTime, 0.f, 1.f) : 1.f;
}

bool UOnyxRangedComponent::Press(EOnyxRangedSlot Slot)
{
	const FOnyxRangedMove& Move = MoveOf(Slot);
	AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	if (!Character || !Move.IsSet() || State != EState::Idle || Now() < ReadyAt[int32(Slot)])
	{
		return false;
	}
	if (const UOnyxCombatComponent* Combat = Character->FindComponentByClass<UOnyxCombatComponent>(); Combat && Combat->IsAttacking())
	{
		return false;
	}
	if (const UOnyxMovementComponent* Movement = Character->GetOnyxMovement(); Movement && (Movement->IsOnLedge() || Movement->IsUnderWater()))
	{
		return false; // the hands are on the edge, or it is under the water (at the surface, the head out, it can)
	}
	const UOnyxEnergyComponent* Energy = Character->FindComponentByClass<UOnyxEnergyComponent>();
	if (Move.EnergyCost > 0.f && (!Energy || Energy->GetEnergy() < Move.EnergyCost))
	{
		UE_LOG(LogOnyxRanged, Log, TEXT("%s: not enough energy to %s"), *GetOwner()->GetName(), Slot == EOnyxRangedSlot::Shoot ? TEXT("shoot") : TEXT("throw"));
		return false;
	}
	Active = Slot;
	bHeld = true;
	bFired = false;
	bCharged = false;
	FireAt = -1.0;
	PressedAt = Now();
	if (UsesUpperBody(Move) && Character->MovesetComponent)
	{
		// The legs go on with what it is doing, drawn alone; the upper body is laid over them.
		Character->MovesetComponent->SetClipVariant(OnyxRanged::LegsVariant);
	}
	if (UOnyxMovementComponent* Movement = Character->GetOnyxMovement())
	{
		if (Move.MoveScale <= 0.f)
		{
			Movement->SetMovementLocked(true);
			bLocked = true;
		}
		else
		{
			Movement->SetSpeedScale(Move.MoveScale);
			AppliedScale = Move.MoveScale;
			bSlowed = true;
		}
	}
	Character->SetFacingLocked(true);
	TurnToAim();
	ShownAngle = NearestDrawn(Move, AimDegrees(Move));
	ShownState = StateSuffix();
	const float Length = PlayClip(ClipAt(Move.StartClip, ShownAngle));
	if (Move.IsWholeMove())
	{
		// A quick throw: the whole move at once, the shot leaving partway through.
		State = EState::LettingGo;
		FirePower = 1.f;
		FireAt = Now() + Move.FireAfter;
		PhaseEnds = Now() + FMath::Max(Length, Move.FireAfter);
	}
	else if (Length > 0.f)
	{
		State = EState::Starting;
		PhaseEnds = Now() + Length;
	}
	else
	{
		LetGo();
	}
	UE_LOG(LogOnyxRanged, Log, TEXT("%s: %s starts, aiming %.0f deg"), *GetOwner()->GetName(), Slot == EOnyxRangedSlot::Shoot ? TEXT("shoot") : TEXT("throw"), ShownAngle);
	return true;
}

void UOnyxRangedComponent::Release(EOnyxRangedSlot Slot)
{
	if (State == EState::Idle || Slot != Active)
	{
		return;
	}
	bHeld = false;
	if (State == EState::Holding)
	{
		LetGo();
	}
}

void UOnyxRangedComponent::LetGo()
{
	const FOnyxRangedMove& Move = MoveOf(Active);
	FirePower = Move.ChargeTime > 0.f ? FMath::Lerp(Move.MinPower, 1.f, FMath::Clamp(float(Now() - PressedAt) / Move.ChargeTime, 0.f, 1.f)) : 1.f;
	TurnToAim();
	ShownAngle = NearestDrawn(Move, AimDegrees(Move));
	ShownState = StateSuffix();
	State = EState::LettingGo;
	const float Length = PlayClip(ClipAt(Move.ReleaseClip, ShownAngle));
	FireAt = Now() + Move.FireAfter;
	PhaseEnds = Now() + FMath::Max(Length, Move.FireAfter);
	if (Move.FireAfter <= 0.f)
	{
		FireNow();
	}
}

void UOnyxRangedComponent::FireNow()
{
	if (bFired)
	{
		return;
	}
	bFired = true;
	const FOnyxRangedMove& Move = MoveOf(Active);
	AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	if (Move.EnergyCost > 0.f)
	{
		UOnyxEnergyComponent* Energy = Character->FindComponentByClass<UOnyxEnergyComponent>();
		if (!Energy || !Energy->SpendEnergy(Move.EnergyCost))
		{
			return;
		}
	}
	ReadyAt[int32(Active)] = Now() + Move.Cooldown;
	// Along the aim (kept within what is drawn), lifted for a lob, from the shoulder out along it.
	const bool bTopDown = UOnyx2DLibrary::IsTopDown(this);
	FVector Direction, Along;
	if (bTopDown)
	{
		const FVector2D Aim = GetAimDirection();
		Direction = Along = FVector(Aim.X, 0.f, Aim.Y);
	}
	else
	{
		const float Sign = Character->GetFacing() == EOnyxFacing::Left ? -1.f : 1.f;
		const float Aimed = FMath::DegreesToRadians(AimDegrees(Move));
		const float Up = FMath::DegreesToRadians(FMath::Clamp(AimDegrees(Move) + Move.AimAngle, -89.f, 89.f));
		Direction = FVector(Sign * FMath::Cos(Up), 0.f, FMath::Sin(Up));
		Along = FVector(Sign * FMath::Cos(Aimed), 0.f, FMath::Sin(Aimed));
	}
	FVector From = ShoulderLocation(Move) + Along * Move.Muzzle.X;
	DrawnShotPoint(Move, From); // where the arrow or the bomb is drawn, when it is
	FOnyxProjectileSpec Spec = Move.Projectile;
	Spec.Speed *= FirePower;
	if (FirePower >= 0.999f)
	{
		Spec.Damage *= Move.ChargedDamageScale;
	}
	AOnyxProjectile* Shot = AOnyxProjectile::Fire(this, Spec, From, Direction, Character);
	UE_LOG(LogOnyxRanged, Log, TEXT("%s: %s at %.0f deg, power %.2f, speed %.0f, damage %.1f"), *Character->GetName(),
		Active == EOnyxRangedSlot::Shoot ? TEXT("shoots") : TEXT("throws"), FMath::RadiansToDegrees(FMath::Atan2(Direction.Z, FMath::Abs(Direction.X))),
		FirePower, Spec.Speed, Spec.Damage);
	OnFired.Broadcast(Active, Shot);
}

void UOnyxRangedComponent::Finish()
{
	State = EState::Idle;
	bHeld = false;
	FireAt = -1.0;
	HideUpper();
	bBackingAway = false;
	if (AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner()))
	{
		if (Character->MovesetComponent)
		{
			Character->MovesetComponent->SetClipVariant(NAME_None); // the whole body again
			Character->MovesetComponent->SetPlayBackwards(false);
		}
		Character->SetFacingLocked(false);
		if (UOnyxMovementComponent* Movement = Character->GetOnyxMovement())
		{
			if (bLocked)
			{
				Movement->SetMovementLocked(false);
			}
			if (bSlowed)
			{
				Movement->SetSpeedScale(1.f);
			}
		}
	}
	bLocked = false;
	bSlowed = false;
}

void UOnyxRangedComponent::Cancel()
{
	if (State != EState::Idle)
	{
		UE_LOG(LogOnyxRanged, Log, TEXT("%s: broken off"), *GetOwner()->GetName());
		Finish();
	}
}

void UOnyxRangedComponent::HandleDamaged(const FOnyxDamageInfo& Damage)
{
	Cancel();
}

void UOnyxRangedComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ShowCrosshair();
	if (State == EState::Idle)
	{
		return;
	}
	const FOnyxRangedMove& Move = MoveOf(Active);
	const double Time = Now();
	// Gone under the water while drawing or holding: the shot is lost.
	if (const AOnyxCharacter* Owner = Cast<AOnyxCharacter>(GetOwner()); Owner && Owner->GetOnyxMovement() && Owner->GetOnyxMovement()->IsUnderWater()
		&& (State == EState::Starting || State == EState::Holding))
	{
		Cancel();
		return;
	}
	// Going one way while aiming the other: slower, and over the legs they walk backwards. From a crouch it may kneel still.
	if (AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner()); Character && bSlowed && !UOnyx2DLibrary::IsTopDown(this))
	{
		if (UOnyxMovementComponent* Movement = Character->GetOnyxMovement())
		{
			const float Ahead = Character->GetFacing() == EOnyxFacing::Left ? -1.f : 1.f;
			const bool bKneel = Move.bKneelWhenCrouched && Movement->IsCrouchingDown();
			const bool bBack = !bKneel && Movement->IsMovingOnGround() && Movement->Velocity.X * Ahead < -20.f;
			const float Scale = bKneel ? 0.f : bBack && Move.BackwardMoveScale > 0.f ? Move.BackwardMoveScale : Move.MoveScale;
			if (!FMath::IsNearlyEqual(Scale, AppliedScale))
			{
				AppliedScale = Scale;
				Movement->SetSpeedScale(Scale);
			}
			if (bBack != bBackingAway)
			{
				bBackingAway = bBack;
				if (Character->MovesetComponent && UsesUpperBody(Move))
				{
					Character->MovesetComponent->SetPlayBackwards(bBack);
				}
			}
		}
	}
	UpdateUpper();
	if (State == EState::Starting || State == EState::Holding)
	{
		TurnToAim();
		// Full power: a glint at the hands, once.
		if (!bCharged && Move.ChargeTime > 0.f && Time - PressedAt >= Move.ChargeTime)
		{
			bCharged = true;
			FVector At = ShoulderLocation(Move) + FVector(GetAimDirection().X, 0.f, GetAimDirection().Y) * Move.Muzzle.X;
			DrawnShotPoint(Move, At);
			AOnyxFlipbookEffect::Spawn(this, Move.ChargedEffect, At);
			UOnyxAudioSubsystem::PlaySoundAt(this, Move.ChargedSound, At, EOnyxSoundCategory::Effects);
		}
	}
	switch (State)
	{
	case EState::Starting:
		if (Time >= PhaseEnds)
		{
			if (bHeld && !Move.HoldClip.IsNone())
			{
				// Drawn: held there, aiming and charging, for as long as the button is.
				State = EState::Holding;
				ShownAngle = NearestDrawn(Move, AimDegrees(Move));
				ShownState = StateSuffix();
				PhaseEnds = Time + FMath::Max(PlayClip(ClipAt(Move.HoldClip, ShownAngle)), 0.1f);
			}
			else
			{
				LetGo();
			}
		}
		break;
	case EState::Holding:
	{
		// The aim moved to another drawn angle, it started or stopped running, left the ground or the water, or the held pose ran
		// out: show it again, as it is now.
		const float Angle = NearestDrawn(Move, AimDegrees(Move));
		const TCHAR* Doing = StateSuffix();
		if (UsesUpperBody(Move))
		{
			// Over the legs it is one drawing whatever they do: it only follows the aim, at the same moment of the pose.
			if (Time >= PhaseEnds)
			{
				ShownAngle = Angle;
				PhaseEnds = Time + FMath::Max(PlayClip(ClipAt(Move.HoldClip, ShownAngle)), 0.1f);
			}
			else if (!FMath::IsNearlyEqual(Angle, ShownAngle))
			{
				ShownAngle = Angle;
				PlayClip(ClipAt(Move.HoldClip, ShownAngle), false);
			}
		}
		else if (Time >= PhaseEnds || !FMath::IsNearlyEqual(Angle, ShownAngle) || FCString::Strcmp(Doing, ShownState) != 0)
		{
			ShownAngle = Angle;
			ShownState = Doing;
			PhaseEnds = Time + FMath::Max(PlayClip(ClipAt(Move.HoldClip, ShownAngle)), 0.1f);
		}
		break;
	}
	case EState::LettingGo:
		if (!bFired && FireAt >= 0.0 && Time >= FireAt)
		{
			FireNow();
		}
		if (bFired && Time >= PhaseEnds)
		{
			Finish();
		}
		break;
	default:
		break;
	}
}
