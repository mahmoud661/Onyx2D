// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaperCharacter.h"
#include "Combat/OnyxCombatTypes.h"
#include "Core/Onyx2DTypes.h"
#include "OnyxCharacter.generated.h"

class UOnyxMovesetComponent;
class UOnyxCombatComponent;
class UOnyxHealthComponent;
class UOnyxHitboxComponent;
class UOnyxMovementComponent;
class UPaperFlipbook;
class USoundBase;
class UStaticMeshComponent;

/** Sounds a character makes by itself (footsteps and attack swings go in its clips as Sound events). */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxCharacterSounds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sounds")
	TObjectPtr<USoundBase> Jump;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sounds")
	TObjectPtr<USoundBase> Land;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sounds")
	TObjectPtr<USoundBase> Dash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sounds")
	TObjectPtr<USoundBase> Hurt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sounds")
	TObjectPtr<USoundBase> Death;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sounds", meta = (ClampMin = "0", ClampMax = "2"))
	float Volume = 1.f;

	/** Random pitch change each time (+/-), so repeated sounds don't feel mechanical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sounds", meta = (ClampMin = "0", ClampMax = "0.5"))
	float PitchVariation = 0.06f;
};

/**
 * A 2D character: platformer movement, health, moveset, melee combat and a team. Each feature is its own component and
 * this class only wires them together. What drives it and how it behaves are components too: add Player Control
 * (+ Camera, Respawn) for the hero, Patrol + Contact Damage (+ Stompable) for a foe, or your own.
 */
UCLASS()
class ONYX2DRUNTIME_API AOnyxCharacter : public APaperCharacter, public IOnyxDamageable, public IOnyxTeamMember
{
	GENERATED_BODY()

public:
	AOnyxCharacter(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxHealthComponent> Health;

	/** Plays the character's Moveset (its clips by role) on the sprite. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxMovesetComponent> MovesetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxCombatComponent> Combat;

	/** Opened by the combat component during attacks. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxHitboxComponent> AttackHitbox;

	/** Shown while the flipbook component has no flipbook, so characters are visible before art exists. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UStaticMeshComponent> Placeholder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	uint8 TeamId = OnyxTeams::Enemy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	FLinearColor PlaceholderColor = FLinearColor(1.f, 0.45f, 0.15f);

	/** Distance of the attack hitbox in front of the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat")
	float AttackReach = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Rendering")
	bool bLitSprite = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Rendering")
	bool bSpriteCastsShadow = true;

	/**
	 * Keep the bottom of the flipbook on the bottom of the capsule (a "feet" pivot), so the art stands on the
	 * ground whatever its size or pivot. Turn off to place the sprite by hand.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Rendering")
	bool bAlignSpriteToCapsule = true;

	/**
	 * Draw the sprite on its own pixel grid (whole art pixels) while the character moves smoothly underneath:
	 * pixel art never shimmers or smears between pixels. Collision and movement are unaffected.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Rendering")
	bool bPixelSnapSprite = true;

	/** Tick when the sprite sheet is drawn looking left (Onyx flips it to face where the character goes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Rendering")
	bool bArtFacesLeft = false;

	/** Transparent space under the feet in the art, in world units: raise to sink the sprite onto the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Rendering", meta = (EditCondition = "bAlignSpriteToCapsule"))
	float SpriteFootPadding = 0.f;

	/** Falling this far below the bottom of the level (tile maps) kills the character. 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Health", meta = (ClampMin = "0"))
	float FallDeathDepth = 400.f;

	/** Moveset role played when hurt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Moveset")
	FName HurtClip = TEXT("Hurt");

	/** Moveset role played on death. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Moveset")
	FName DeathClip = TEXT("Death");

	/** Jump, land, dash, hurt and death sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Sounds")
	FOnyxCharacterSounds Sounds;

	/** Plays one of its sounds at the character (with a little pitch variation). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Sounds")
	void PlayCharacterSound(USoundBase* Sound) const;

	/** Played once where it dies (a puff, an explosion); the body disappears at once when set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Health")
	TObjectPtr<UPaperFlipbook> DeathEffect;

	/** Seconds the body stays after death (0 = forever). Ignored by characters that respawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Health", meta = (ClampMin = "0", Units = "s"))
	float CorpseLifetime = 1.f;

	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	UOnyxMovementComponent* GetOnyxMovement() const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	EOnyxFacing GetFacing() const { return Facing; }

	UFUNCTION(BlueprintCallable, Category = "Onyx2D")
	void SetFacing(EOnyxFacing NewFacing);

	// IOnyxDamageable / IOnyxTeamMember
	virtual bool ReceiveDamage_Implementation(const FOnyxDamageInfo& Damage) override;
	virtual uint8 GetTeamId_Implementation() const override { return TeamId; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Onyx2D")
	void OnHurt(const FOnyxDamageInfo& Damage);

	UFUNCTION(BlueprintImplementableEvent, Category = "Onyx2D")
	void OnDied(AActor* Killer);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	/** Unreal's world KillZ: treat it as a fatal fall (respawn) instead of silently destroying the actor. */
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;
	/** Kills the character when it falls below the playable area. */
	void CheckFallDeath();

	/** Called when health reaches zero. Base: stops combat, plays the Death clip, fires OnDied. */
	UFUNCTION()
	virtual void HandleDeath(AActor* Killer);

	UFUNCTION()
	void HandleJumpSound();

	UFUNCTION()
	void HandleLandSound();

	UFUNCTION()
	void HandleDashSound();

	/** Out of breath under water: takes the movement's Drown Damage. */
	UFUNCTION()
	void HandleDrowning();

	/** Chooses facing each frame (movement input / velocity). Skipped while attacking. */
	virtual void UpdateFacing();

public:
	/** Moves the flipbook so its bottom sits on the bottom of the capsule (see bAlignSpriteToCapsule). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Rendering")
	void AlignSpriteToCapsule();

private:
	void ApplyFacing();
	/** Shows the coloured stand-in body only while the character has no flipbook. */
	void UpdatePlaceholder();

	/** Puts the sprite back on whole art pixels this frame (see bPixelSnapSprite). */
	void SnapSpriteToPixels();
	/** Sprite position before pixel snapping (where AlignSpriteToCapsule / the Blueprint put it). */
	FVector& GetSpriteBaseLocation();

	EOnyxFacing Facing = EOnyxFacing::Right;
	/** Degrees the sprite leans while swimming (see the movement's Max Lean). */
	float SpriteLean = 0.f;
	FVector SpriteBaseLocation = FVector::ZeroVector;
	bool bHasSpriteBase = false;
	TWeakObjectPtr<const UPaperFlipbook> AlignedFlipbook;
};
