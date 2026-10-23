// Copyright Onyx2D. All Rights Reserved.

#include "Character/OnyxCharacter.h"
#include "Sound/SoundBase.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Character/OnyxRespawnComponent.h"
#include "World/OnyxFlipbookEffect.h"
#include "Animation/OnyxMovesetComponent.h"
#include "Combat/OnyxCombatComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "Combat/OnyxHitboxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Core/OnyxWorldSubsystem.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Feedback/OnyxFeedbackSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Movement/OnyxMovementComponent.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PaperFlipbookComponent.h"
#include "UObject/ConstructorHelpers.h"

AOnyxCharacter::AOnyxCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UOnyxMovementComponent>(CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = false;
	GetCapsuleComponent()->InitCapsuleSize(30.f, 60.f);

	GetCharacterMovement()->bRunPhysicsWithNoController = true; // falls and lands even before anything possesses it
	Health = CreateDefaultSubobject<UOnyxHealthComponent>(TEXT("Health"));
	MovesetComponent = CreateDefaultSubobject<UOnyxMovesetComponent>(TEXT("Moveset"));
	Combat = CreateDefaultSubobject<UOnyxCombatComponent>(TEXT("Combat"));

	AttackHitbox = CreateDefaultSubobject<UOnyxHitboxComponent>(TEXT("AttackHitbox"));
	AttackHitbox->SetupAttachment(GetCapsuleComponent());
	AttackHitbox->SetBoxExtent(FVector(45.f, 60.f, 45.f));

	Placeholder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Placeholder"));
	Placeholder->SetupAttachment(GetCapsuleComponent());
	Placeholder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Placeholder->SetRelativeScale3D(FVector(0.6f, 0.3f, 1.2f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Placeholder->SetStaticMesh(Cube.Object);
}

UOnyxMovementComponent* AOnyxCharacter::GetOnyxMovement() const
{
	return Cast<UOnyxMovementComponent>(GetCharacterMovement());
}

void AOnyxCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AlignSpriteToCapsule();
	UpdatePlaceholder(); // in the editor too: the stand-in box must never cover real art
}

void AOnyxCharacter::UpdatePlaceholder()
{
	const bool bHasArt = GetSprite() && GetSprite()->GetFlipbook();
	Placeholder->SetVisibility(!bHasArt);
	if (!bHasArt)
	{
		UOnyx2DLibrary::ApplySolidColor(Placeholder, PlaceholderColor);
	}
}

void AOnyxCharacter::AlignSpriteToCapsule()
{
	UPaperFlipbookComponent* SpriteComponent = GetSprite();
	const UPaperFlipbook* Flipbook = SpriteComponent ? SpriteComponent->GetFlipbook() : nullptr;
	if (!bAlignSpriteToCapsule || !Flipbook || Flipbook->GetNumFrames() == 0)
	{
		return;
	}
	// Frame bounds in flipbook space (Z is up), scaled like the component.
	const FBoxSphereBounds Bounds = Flipbook->GetRenderBounds();
	const float Bottom = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * SpriteComponent->GetRelativeScale3D().Z;
	FVector& Location = GetSpriteBaseLocation();
	Location.Z = -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - Bottom - SpriteFootPadding;
	SpriteComponent->SetRelativeLocation(Location);
	AlignedFlipbook = Flipbook;
}

FVector& AOnyxCharacter::GetSpriteBaseLocation()
{
	if (!bHasSpriteBase && GetSprite())
	{
		SpriteBaseLocation = GetSprite()->GetRelativeLocation();
		bHasSpriteBase = true;
	}
	return SpriteBaseLocation;
}

void AOnyxCharacter::SnapSpriteToPixels()
{
	UPaperFlipbookComponent* SpriteComponent = GetSprite();
	const UPaperFlipbook* Flipbook = SpriteComponent ? SpriteComponent->GetFlipbook() : nullptr;
	const UPaperSprite* Frame = Flipbook && Flipbook->GetNumKeyFrames() > 0 ? Flipbook->GetSpriteAtFrame(0) : nullptr;
	// Standing on a slope the picture is lowered onto it (the capsule rests on the slope beside its middle).
	const UOnyxMovementComponent* Movement = GetOnyxMovement();
	const float Foot = Movement ? Movement->GetSlopeFootOffset() : 0.f;
	const FVector Base = GetSpriteBaseLocation() + FVector(0.f, 0.f, Foot);
	if (!bPixelSnapSprite || !Frame || Frame->GetPixelsPerUnrealUnit() <= KINDA_SMALL_NUMBER)
	{
		if (SpriteComponent && (Foot != 0.f || AppliedSlopeFoot != 0.f))
		{
			SpriteComponent->SetRelativeLocation(Base);
		}
		AppliedSlopeFoot = Foot;
		return;
	}
	// One art pixel in the world, scaled like the sprite.
	const float Pixel = SpriteComponent->GetComponentScale().X / Frame->GetPixelsPerUnrealUnit();
	const FVector World = GetActorLocation() + Base;
	const FVector Offset(FMath::GridSnap(World.X, Pixel) - World.X, 0.f, FMath::GridSnap(World.Z, Pixel) - World.Z);
	SpriteComponent->SetRelativeLocation(Base + Offset);
}

void AOnyxCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	Combat->SetHitbox(AttackHitbox);
	MovesetComponent->SetTargetFlipbook(GetSprite());
}

void AOnyxCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (bLitSprite)
	{
		UOnyx2DLibrary::ConfigureSpriteLighting(GetSprite(), bSpriteCastsShadow);
	}
	if (LightGuard > 0.f && GetSprite())
	{
		GetSprite()->SetCustomPrimitiveDataFloat(Onyx2D::LightGuardDataIndex, FMath::Clamp(LightGuard, 0.f, 1.f));
	}
	if (GlowPixels > 0.f)
	{
		UOnyx2DLibrary::SetSpriteGlow(GetSprite(), GlowPixels, bGlowFromMask ? -1.f : GlowThreshold);
	}

	UpdatePlaceholder();
	AlignSpriteToCapsule(); // after the movement took its game style: a top-down footprint is shorter
	MovesetComponent->OnClipApplied.AddUObject(this, &AOnyxCharacter::HandleClipApplied);
	if (IsTopDown())
	{
		Direction = EOnyxDirection::Side; // so the first heading always counts as a change
		SetHeading(Heading);
	}
	Health->OnDeath.AddUniqueDynamic(this, &AOnyxCharacter::HandleDeath);
	if (UOnyxMovementComponent* Movement = GetOnyxMovement())
	{
		Movement->OnJumped.AddUniqueDynamic(this, &AOnyxCharacter::HandleJumpSound);
		Movement->OnAirJumped.AddUniqueDynamic(this, &AOnyxCharacter::HandleJumpSound);
		Movement->OnWallJumped.AddUniqueDynamic(this, &AOnyxCharacter::HandleJumpSound);
		Movement->OnDashed.AddUniqueDynamic(this, &AOnyxCharacter::HandleDashSound);
		Movement->OnLandedEvent.AddUniqueDynamic(this, &AOnyxCharacter::HandleLandSound);
		Movement->OnDrowning.AddUniqueDynamic(this, &AOnyxCharacter::HandleDrowning);
	}
	if (Sounds.Loop)
	{
		LoopSound = UOnyxAudioSubsystem::PlaySoundAttached(Sounds.Loop, GetRootComponent(), EOnyxSoundCategory::Effects, Sounds.LoopVolume);
	}
	ApplyFacing();
}

void AOnyxCharacter::HandleDrowning()
{
	FOnyxDamageInfo Damage;
	Damage.Amount = GetOnyxMovement() ? GetOnyxMovement()->SwimSettings.DrownDamage : 1.f;
	Damage.DamageType = TEXT("Drowning");
	Damage.bIgnoreInvulnerability = true; // the hurt flash's invulnerability must not stop drowning
	if (Damage.Amount > 0.f)
	{
		Execute_ReceiveDamage(this, Damage);
	}
}

void AOnyxCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Animations can differ in frame size: keep the feet planted when the moveset swaps flipbooks.
	if (GetSprite() && GetSprite()->GetFlipbook() != AlignedFlipbook.Get())
	{
		AlignSpriteToCapsule();
	}
	KnockedBackTime = FMath::Max(KnockedBackTime - DeltaSeconds, 0.f);
	if (HurtStunLeft > 0.f)
	{
		HurtStunLeft -= DeltaSeconds;
		if (HurtStunLeft <= 0.f && !Health->IsDead())
		{
			if (UOnyxMovementComponent* Movement = GetOnyxMovement())
			{
				Movement->SetMovementLocked(false);
			}
		}
	}
	if (BlinkLeft > 0.f)
	{
		// Mostly there, with quick blinks out: readable as "can't be hurt" without losing sight of it.
		BlinkLeft = Health->IsDead() ? 0.f : BlinkLeft - DeltaSeconds;
		if (UPaperFlipbookComponent* SpriteComponent = GetSprite())
		{
			SpriteComponent->SetVisibility(BlinkLeft <= 0.f || FMath::Fmod(BlinkLeft, 0.1f) > 0.03f);
		}
	}
	if (CorpseTimeLeft >= 0.f)
	{
		// The body lies still, then comes apart art pixel by art pixel and is gone.
		CorpseTimeLeft -= DeltaSeconds;
		const float Fade = FMath::Max(CorpseFadeTime, 0.01f);
		if (UPaperFlipbookComponent* SpriteComponent = GetSprite(); SpriteComponent && CorpseTimeLeft < Fade)
		{
			SpriteComponent->SetCustomPrimitiveDataFloat(Onyx2D::DissolveDataIndex, FMath::Clamp(1.f - CorpseTimeLeft / Fade, 0.f, 1.f));
		}
		if (CorpseTimeLeft <= 0.f)
		{
			Destroy();
			return;
		}
	}
	if (bFaceMovement && !Combat->IsAttacking() && !Health->IsDead())
	{
		UpdateFacing();
	}
	// Leaning into the swimming direction (only when the movement's Max Lean allows it).
	const float Lean = GetOnyxMovement() ? GetOnyxMovement()->GetSwimLean() : 0.f;
	if (!FMath::IsNearlyEqual(Lean, SpriteLean, 0.05f))
	{
		SpriteLean = Lean;
		ApplyFacing();
	}
	SnapSpriteToPixels();
	CheckFallDeath();
}

void AOnyxCharacter::CheckFallDeath()
{
	if (FallDeathDepth <= 0.f || Health->IsDead() || IsTopDown())
	{
		return;
	}
	const UOnyxWorldSubsystem* Registry = GetWorld()->GetSubsystem<UOnyxWorldSubsystem>();
	const FBox Level = Registry ? Registry->GetLevelBounds() : FBox(ForceInit);
	if (Level.IsValid && GetActorLocation().Z < Level.Min.Z - FallDeathDepth)
	{
		Health->Kill(nullptr);
	}
}

void AOnyxCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	if (!Health->IsDead())
	{
		Health->Kill(nullptr);
	}
}

void AOnyxCharacter::SetAttackShape(const FVector2D& Offset, const FVector2D& HalfSize)
{
	if (!bHasDefaultAttackExtent)
	{
		DefaultAttackExtent = AttackHitbox->GetUnscaledBoxExtent();
		bHasDefaultAttackExtent = true;
	}
	bAttackShape = true;
	AttackShapeOffset = Offset;
	AttackHitbox->SetBoxExtent(FVector(FMath::Max(HalfSize.X, 1.f), DefaultAttackExtent.Y, FMath::Max(HalfSize.Y, 1.f)));
	ApplyFacing();
}

void AOnyxCharacter::ClearAttackShape()
{
	if (!bAttackShape)
	{
		return;
	}
	bAttackShape = false;
	AttackHitbox->SetBoxExtent(DefaultAttackExtent);
	ApplyFacing();
}

void AOnyxCharacter::ResetAfterDeath()
{
	CorpseTimeLeft = -1.f;
	HurtStunLeft = 0.f;
	BlinkLeft = 0.f;
	if (UPaperFlipbookComponent* SpriteComponent = GetSprite())
	{
		SpriteComponent->SetCustomPrimitiveDataFloat(Onyx2D::DissolveDataIndex, 0.f);
		SpriteComponent->SetVisibility(true);
	}
	MovesetComponent->StopOneShot();
	MovesetComponent->bAutoLocomotion = true;
}

void AOnyxCharacter::Destroyed()
{
	if (UAudioComponent* Loop = LoopSound.Get())
	{
		Loop->Stop();
	}
	ClearAttackShape();
	Super::Destroyed();
}

void AOnyxCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	// The capsule got shorter and its centre went down: the sprite keeps its feet on the ground.
	if (bAlignSpriteToCapsule)
	{
		AlignSpriteToCapsule();
	}
	else if (GetSprite())
	{
		GetSpriteBaseLocation().Z += HalfHeightAdjust;
		GetSprite()->SetRelativeLocation(GetSpriteBaseLocation());
	}
}

void AOnyxCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	if (bAlignSpriteToCapsule)
	{
		AlignSpriteToCapsule();
	}
	else if (GetSprite())
	{
		GetSpriteBaseLocation().Z -= HalfHeightAdjust;
		GetSprite()->SetRelativeLocation(GetSpriteBaseLocation());
	}
}

void AOnyxCharacter::UpdateFacing()
{
	if (bFacingLocked)
	{
		return; // turned by whoever locked it (aiming)
	}
	const UOnyxMovementComponent* Movement = GetOnyxMovement();
	if (Movement && Movement->IsTopDown())
	{
		// Where it is steered; else, moving by itself (a dash), where it goes; thrown back by a hit, still at what hit it.
		const FVector2D Steering = Movement->GetTopDownSteering();
		const FVector2D Moving(GetVelocity().X, GetVelocity().Z);
		if (!Steering.IsNearlyZero(0.1f))
		{
			SetHeading(Steering);
		}
		else if (KnockedBackTime <= 0.f && Moving.Size() > 40.f)
		{
			SetHeading(Moving);
		}
		return;
	}
	if (Movement && Movement->IsOnLedge())
	{
		SetFacing(Movement->GetLedgeDirection() > 0.f ? EOnyxFacing::Right : EOnyxFacing::Left); // facing the wall it holds
		return;
	}
	const bool bSteered = Movement && !Movement->GetCurrentAcceleration().IsNearlyZero();
	if (!bSteered && KnockedBackTime > 0.f)
	{
		return; // flying back from a hit, still facing what hit it
	}
	const float Sideways = bSteered ? Movement->GetCurrentAcceleration().X : GetVelocity().X * 0.01f;
	SetFacing(UOnyx2DLibrary::FacingFromDirection(Sideways, Facing));
}

void AOnyxCharacter::FaceForAttack()
{
	const UOnyxMovementComponent* Movement = GetOnyxMovement();
	if (Movement && Movement->IsTopDown())
	{
		// This frame's steering if there is any yet (the stick or keys pressed with the attack), else the last frame's.
		const FVector Pending = GetPendingMovementInputVector();
		const float Held = (Movement->IsUpHeld() ? 1.f : 0.f) - (Movement->IsDownHeld() ? 1.f : 0.f);
		FVector2D Steering(Pending.X, Pending.Z + Held);
		if (Steering.IsNearlyZero(0.1f))
		{
			Steering = Movement->GetTopDownSteering();
		}
		if (!Steering.IsNearlyZero(0.1f))
		{
			SetHeading(Steering);
		}
		return;
	}
	if (Movement && Movement->IsWallSliding() && Movement->GetWallDirection() != 0.f)
	{
		SetFacing(Movement->GetWallDirection() > 0.f ? EOnyxFacing::Left : EOnyxFacing::Right);
		return;
	}
	// This frame's steering if there is any yet, else the last frame's.
	const FVector Pending = GetPendingMovementInputVector();
	const FVector Steering = FMath::Abs(Pending.X) > 0.1f ? Pending : GetLastMovementInputVector();
	if (FMath::Abs(Steering.X) > 0.1f)
	{
		SetFacing(Steering.X > 0.f ? EOnyxFacing::Right : EOnyxFacing::Left);
	}
}

void AOnyxCharacter::SetFacing(EOnyxFacing NewFacing)
{
	if (NewFacing != Facing)
	{
		if (IsTopDown())
		{
			// Turned to a side: looking that way along the ground.
			SetHeading(FVector2D(Onyx2D::FacingSign(NewFacing), 0.f));
			return;
		}
		Facing = NewFacing;
		ApplyFacing();
	}
}

bool AOnyxCharacter::IsTopDown() const
{
	const UOnyxMovementComponent* Movement = GetOnyxMovement();
	return Movement && Movement->IsTopDown();
}

FVector2D AOnyxCharacter::GetAimVector() const
{
	return Direction == EOnyxDirection::None ? FVector2D(Onyx2D::FacingSign(Facing), 0.f) : Onyx2D::HeadingFromDirection(Direction, Facing);
}

void AOnyxCharacter::SetHeading(FVector2D NewHeading)
{
	if (!IsTopDown() || NewHeading.IsNearlyZero(0.01f))
	{
		return;
	}
	Heading = NewHeading.GetSafeNormal();
	EOnyxDirection NewDirection = Onyx2D::DirectionFromHeading(Heading, Direction);
	if ((NewDirection == EOnyxDirection::DownSide || NewDirection == EOnyxDirection::UpSide) && !MovesetComponent->HasDiagonalClips())
	{
		NewDirection = EOnyxDirection::Side; // drawn in four directions: a diagonal shows the side
	}
	const EOnyxFacing NewFacing = FMath::Abs(Heading.X) > 0.2f ? (Heading.X > 0.f ? EOnyxFacing::Right : EOnyxFacing::Left) : Facing;
	if (NewDirection != Direction || NewFacing != Facing)
	{
		Direction = NewDirection;
		Facing = NewFacing;
		MovesetComponent->SetDirection(Direction);
		ApplyFacing();
	}
}

void AOnyxCharacter::HandleClipApplied()
{
	if (IsTopDown())
	{
		ApplyFacing();
	}
}

void AOnyxCharacter::ApplyFacing()
{
	const float Sign = Onyx2D::FacingSign(Facing);
	const bool bTopDown = IsTopDown();
	if (UPaperFlipbookComponent* SpriteComponent = GetSprite())
	{
		// Top down, a clip drawn looking up or down shows as drawn; side ones are turned the way it faces.
		const EOnyxDirection Shown = MovesetComponent->GetShownDirection();
		const bool bAsDrawn = bTopDown && (Shown == EOnyxDirection::Up || Shown == EOnyxDirection::Down);
		const bool bShowRight = bAsDrawn ? !bArtFacesLeft : (Facing == EOnyxFacing::Right) != bArtFacesLeft;
		SpriteComponent->SetRelativeRotation(FRotator(SpriteLean, bShowRight ? 0.f : 180.f, 0.f)); // pitch is relative to the facing
	}
	if (bTopDown)
	{
		// The blow lands ahead of the feet along the way it looks, its length along that way.
		const FVector2D Aim = GetAimVector();
		const float Reach = bAttackShape ? AttackShapeOffset.X : AttackReach;
		AttackHitbox->SetRelativeLocation(FVector(Aim.X * Reach, 0.f, Aim.Y * Reach));
		AttackHitbox->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(FMath::Atan2(Aim.Y, Aim.X)), 0.f, 0.f));
	}
	else
	{
		AttackHitbox->SetRelativeLocation(bAttackShape ? FVector(Sign * AttackShapeOffset.X, 0.f, AttackShapeOffset.Y) : FVector(Sign * AttackReach, 0.f, 0.f));
		AttackHitbox->SetRelativeRotation(FRotator::ZeroRotator);
	}
	Combat->FacingSign = Sign;
	MovesetComponent->SetFacingSign(Sign);
}

bool AOnyxCharacter::ReceiveDamage_Implementation(const FOnyxDamageInfo& Damage)
{
	if (!Health->ApplyDamage(Damage))
	{
		return false;
	}

	if (const FVector Thrown = Damage.Knockback * KnockbackScale; !Thrown.IsNearlyZero())
	{
		LaunchCharacter(Thrown, true, true);
		KnockedBackTime = 0.4f;
	}
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
	{
		Feedback->FlashActor(this);
	}
	if (!Health->IsDead())
	{
		Combat->CancelAttack();
		MovesetComponent->PlayOneShot(HurtClip);
		PlayCharacterSound(Sounds.Hurt);
		// Thrown back: no control for a moment (after the attack let go of the movement).
		if (HurtStunTime > 0.f)
		{
			HurtStunLeft = HurtStunTime;
			if (UOnyxMovementComponent* Movement = GetOnyxMovement())
			{
				Movement->SetMovementLocked(true);
			}
		}
		if (bBlinkWhileInvulnerable)
		{
			BlinkLeft = Health->InvulnerabilityTime;
		}
		const APlayerController* PlayerController = Cast<APlayerController>(GetController());
		if (HurtScreenFlash.A > 0.f && PlayerController && PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->StartCameraFade(HurtScreenFlash.A, 0.f, 0.3f, HurtScreenFlash, false, false);
		}
	}
	OnHurt(Damage);
	return true;
}

void AOnyxCharacter::HandleJumpSound()
{
	PlayCharacterSound(Sounds.Jump);
}

void AOnyxCharacter::HandleLandSound()
{
	PlayCharacterSound(Sounds.Land);
}

void AOnyxCharacter::HandleDashSound()
{
	PlayCharacterSound(Sounds.Dash);
}

void AOnyxCharacter::PlayCharacterSound(USoundBase* Sound) const
{
	if (Sound)
	{
		UOnyxAudioSubsystem::PlaySoundAt(this, Sound, GetActorLocation(), EOnyxSoundCategory::Effects, Sounds.Volume,
			1.f + FMath::FRandRange(-Sounds.PitchVariation, Sounds.PitchVariation));
	}
}

void AOnyxCharacter::HandleDeath(AActor* Killer)
{
	if (UAudioComponent* Loop = LoopSound.Get())
	{
		Loop->FadeOut(0.3f, 0.f);
	}
	Combat->CancelAttack();
	ClearAttackShape();
	// The death clip plays to its end and holds its last frame (the movement clips don't come back).
	const float DeathLength = MovesetComponent->PlayOneShot(DeathClip);
	MovesetComponent->bAutoLocomotion = false;
	PlayCharacterSound(Sounds.Death);
	if (UOnyxMovementComponent* Movement = GetOnyxMovement())
	{
		Movement->SetMovementLocked(true);
	}
	OnDied(Killer);

	// Characters that come back keep their body; the others leave (with their death effect).
	if (FindComponentByClass<UOnyxRespawnComponent>())
	{
		return;
	}
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	if (DeathEffect)
	{
		AOnyxFlipbookEffect::Spawn(this, DeathEffect, GetActorLocation(), GetSprite()->GetComponentScale().X);
		GetSprite()->SetVisibility(false);
		SetLifeSpan(0.1f);
	}
	else
	{
		// Lies there for the rest of Corpse Lifetime (at least until its death is over), then dissolves.
		CorpseTimeLeft = FMath::Max(CorpseLifetime, DeathLength + 0.25f) + CorpseFadeTime;
	}
}
