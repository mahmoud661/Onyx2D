// Copyright Onyx2D. All Rights Reserved.

#include "Character/OnyxCharacterEffectsComponent.h"
#include "Character/OnyxCharacter.h"
#include "Combat/OnyxHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Feedback/OnyxAfterimage.h"
#include "Movement/OnyxMovementComponent.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "World/OnyxFlipbookEffect.h"

UOnyxCharacterEffectsComponent::UOnyxCharacterEffectsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UOnyxCharacterEffectsComponent::BeginPlay()
{
	Super::BeginPlay();
	Character = Cast<AOnyxCharacter>(GetOwner());
	if (UOnyxMovementComponent* Movement = Character.IsValid() ? Character->GetOnyxMovement() : nullptr)
	{
		Movement->OnJumped.AddUniqueDynamic(this, &UOnyxCharacterEffectsComponent::HandleJumped);
		Movement->OnAirJumped.AddUniqueDynamic(this, &UOnyxCharacterEffectsComponent::HandleAirJumped);
		Movement->OnWallJumped.AddUniqueDynamic(this, &UOnyxCharacterEffectsComponent::HandleWallJumped);
		Movement->OnLandedEvent.AddUniqueDynamic(this, &UOnyxCharacterEffectsComponent::HandleLanded);
		Movement->OnDashed.AddUniqueDynamic(this, &UOnyxCharacterEffectsComponent::HandleDashed);
	}
}

void UOnyxCharacterEffectsComponent::SpawnDust(UPaperFlipbook* Dust) const
{
	const AOnyxCharacter* Self = Character.Get();
	if (!Self || !Dust)
	{
		return;
	}
	// At the feet, a hair in front of the character so it isn't hidden behind it.
	const FVector Feet = Self->GetActorLocation() - FVector(0.f, -1.f, Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 6.f);
	AOnyxFlipbookEffect::Spawn(const_cast<UOnyxCharacterEffectsComponent*>(this), Dust, Feet);
}

void UOnyxCharacterEffectsComponent::HandleJumped()
{
	// Off the ground: dust. Out of the water: the water splashes by itself.
	const UOnyxMovementComponent* Movement = Character.IsValid() ? Character->GetOnyxMovement() : nullptr;
	if (Movement && !Movement->HasJustLeftWater())
	{
		SpawnDust(JumpDust);
	}
}

void UOnyxCharacterEffectsComponent::HandleAirJumped()
{
	SpawnDust(AirJumpEffect);
}

void UOnyxCharacterEffectsComponent::HandleWallJumped()
{
	SpawnDust(JumpDust);
}

void UOnyxCharacterEffectsComponent::HandleLanded()
{
	SpawnDust(LandDust);
}

void UOnyxCharacterEffectsComponent::HandleDashed()
{
	// Only a roll along the ground raises dust; a dash in the air or under water has nothing to raise.
	const UOnyxMovementComponent* Movement = Character.IsValid() ? Character->GetOnyxMovement() : nullptr;
	if (Movement && Movement->IsRolling() && !Movement->IsSwimmingInWater())
	{
		SpawnDust(DashDust);
	}
	AfterimageTimer = 0.f;
}

void UOnyxCharacterEffectsComponent::LeaveAfterimage() const
{
	const AOnyxCharacter* Self = Character.Get();
	UPaperFlipbookComponent* Sprite = Self ? Self->GetSprite() : nullptr;
	const UPaperFlipbook* Flipbook = Sprite ? Sprite->GetFlipbook() : nullptr;
	if (!Flipbook)
	{
		return;
	}
	// The frame showing right now, where it is now (turned the way it faces), a little behind the character.
	FTransform Where = Sprite->GetComponentTransform();
	Where.AddToTranslation(FVector(0.f, -2.f, 0.f));
	AOnyxAfterimage::Spawn(const_cast<UOnyxCharacterEffectsComponent*>(this), Flipbook->GetSpriteAtTime(Sprite->GetPlaybackPosition()), Where, AfterimageColor,
		AfterimageLifetime);
}

void UOnyxCharacterEffectsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AOnyxCharacter* Self = Character.Get();
	if (!Self)
	{
		return;
	}
	const UOnyxMovementComponent* Movement = Self->GetOnyxMovement();
	if (bDashAfterimages && Movement && Movement->IsDashing())
	{
		AfterimageTimer -= DeltaTime;
		if (AfterimageTimer <= 0.f)
		{
			AfterimageTimer = AfterimageInterval;
			LeaveAfterimage();
		}
	}
	// Blink while it can't be hurt for a moment, after a hit (and isn't dead: a dying character leaves the stage its own way). Not
	// while it was made unhurtable outright (a boss before its fight, the God test command): it would blink the whole time.
	const bool bBlink = bBlinkWhileInvulnerable && Self->Health && Self->Health->IsBrieflyInvulnerable() && !Self->Health->IsDead();
	BlinkTime = bBlink ? BlinkTime + DeltaTime : 0.f;
	const bool bHide = bBlink && FMath::Frac(BlinkTime * BlinkRate) >= 0.5f;
	if (bHide != bBlinkHidden && Self->GetSprite())
	{
		bBlinkHidden = bHide;
		Self->GetSprite()->SetHiddenInGame(bHide);
	}
}
