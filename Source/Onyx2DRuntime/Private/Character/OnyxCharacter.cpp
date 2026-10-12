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
	if (!bPixelSnapSprite || !Frame || Frame->GetPixelsPerUnrealUnit() <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	// One art pixel in the world, scaled like the sprite.
	const float Pixel = SpriteComponent->GetComponentScale().X / Frame->GetPixelsPerUnrealUnit();
	const FVector Base = GetSpriteBaseLocation();
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

	UpdatePlaceholder();
	AlignSpriteToCapsule();
	Health->OnDeath.AddUniqueDynamic(this, &AOnyxCharacter::HandleDeath);
	if (UOnyxMovementComponent* Movement = GetOnyxMovement())
	{
		Movement->OnJumped.AddUniqueDynamic(this, &AOnyxCharacter::HandleJumpSound);
		Movement->OnAirJumped.AddUniqueDynamic(this, &AOnyxCharacter::HandleJumpSound);
		Movement->OnWallJumped.AddUniqueDynamic(this, &AOnyxCharacter::HandleJumpSound);
		Movement->OnDashed.AddUniqueDynamic(this, &AOnyxCharacter::HandleDashSound);
		Movement->OnLandedEvent.AddUniqueDynamic(this, &AOnyxCharacter::HandleLandSound);
	}
	ApplyFacing();
}

void AOnyxCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Animations can differ in frame size: keep the feet planted when the moveset swaps flipbooks.
	if (GetSprite() && GetSprite()->GetFlipbook() != AlignedFlipbook.Get())
	{
		AlignSpriteToCapsule();
	}
	if (!Combat->IsAttacking() && !Health->IsDead())
	{
		UpdateFacing();
	}
	SnapSpriteToPixels();
	CheckFallDeath();
}

void AOnyxCharacter::CheckFallDeath()
{
	if (FallDeathDepth <= 0.f || Health->IsDead())
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

void AOnyxCharacter::UpdateFacing()
{
	const UOnyxMovementComponent* Movement = GetOnyxMovement();
	const float Direction = Movement && !Movement->GetCurrentAcceleration().IsNearlyZero()
		? Movement->GetCurrentAcceleration().X
		: GetVelocity().X * 0.01f;
	SetFacing(UOnyx2DLibrary::FacingFromDirection(Direction, Facing));
}

void AOnyxCharacter::SetFacing(EOnyxFacing NewFacing)
{
	if (NewFacing != Facing)
	{
		Facing = NewFacing;
		ApplyFacing();
	}
}

void AOnyxCharacter::ApplyFacing()
{
	const float Sign = Onyx2D::FacingSign(Facing);
	if (UPaperFlipbookComponent* SpriteComponent = GetSprite())
	{
		const bool bShowRight = (Facing == EOnyxFacing::Right) != bArtFacesLeft;
		SpriteComponent->SetRelativeRotation(FRotator(0.f, bShowRight ? 0.f : 180.f, 0.f));
	}
	AttackHitbox->SetRelativeLocation(FVector(Sign * AttackReach, 0.f, 0.f));
	Combat->FacingSign = Sign;
	MovesetComponent->SetFacingSign(Sign);
}

bool AOnyxCharacter::ReceiveDamage_Implementation(const FOnyxDamageInfo& Damage)
{
	if (!Health->ApplyDamage(Damage))
	{
		return false;
	}

	if (!Damage.Knockback.IsNearlyZero())
	{
		LaunchCharacter(Damage.Knockback, true, true);
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
	Combat->CancelAttack();
	MovesetComponent->PlayOneShot(DeathClip);
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
	else if (CorpseLifetime > 0.f)
	{
		SetLifeSpan(CorpseLifetime);
	}
}
