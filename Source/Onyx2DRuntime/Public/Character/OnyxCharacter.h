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

	/** Its own sound for as long as it is alive, going where it goes (a wasp's buzz, a machine's hum). Make it a looping sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sounds")
	TObjectPtr<USoundBase> Loop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sounds", meta = (ClampMin = "0", ClampMax = "2"))
	float LoopVolume = 0.5f;

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
	 * How much its sprite is kept from washing out under light, 0 to 1: its own glows (its light, its blade's flashes: Own
	 * Glow lights) turned away, other light taken a little less and rolled off before it turns paint white. A hero
	 * carrying light lights the world around them and stays readable (a dark cloak, a pale mask against the lit wall).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Rendering", meta = (ClampMin = "0", ClampMax = "1"))
	float LightGuard = 0.f;

	/** Glowing pixels: how strongly its brightest pixels (eyes, a blade, a flame) shine on their own in the dark (0 none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Rendering", meta = (ClampMin = "0", UIMax = "4"))
	float GlowPixels = 0.f;

	/** Which pixels glow: those brighter than this share of full brightness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Rendering", meta = (ClampMin = "0.05", ClampMax = "1", EditCondition = "GlowPixels > 0 && !bGlowFromMask"))
	float GlowThreshold = 0.75f;

	/** Only the pixels its glow mask names glow (its sprites' Additional Textures slot 1, made with a Pixel Light): just the blade, not every bright pixel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Rendering", meta = (EditCondition = "GlowPixels > 0"))
	bool bGlowFromMask = false;

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

	/** Then the body dissolves, art pixel by art pixel, over this long (with the Onyx2D lit sprite material). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Health", meta = (ClampMin = "0", Units = "s"))
	float CorpseFadeTime = 0.5f;

	/** How far blows throw it: 1 as the blow says, 0 unmoved (a boss, a heavy guard). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Health", meta = (ClampMin = "0", UIMax = "2"))
	float KnockbackScale = 1.f;

	/** After a hit it blinks for as long as it can't be hurt again (Health's Invulnerability Time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Health")
	bool bBlinkWhileInvulnerable = false;

	/** After a hit, no steering, jumping or attacking for this long: the blow throws it back (0 = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Health", meta = (ClampMin = "0", Units = "s"))
	float HurtStunTime = 0.f;

	/** The screen flashes this colour when the player's character is hurt; alpha is how strongly (0 = no flash). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Health")
	FLinearColor HurtScreenFlash = FLinearColor(1.f, 1.f, 1.f, 0.f);

	/** Keeps facing the way it faces for a while, whatever its speed (pushed back by its own blow, a clash, a hit). */
	void HoldFacing(float Seconds) { KnockedBackTime = FMath::Max(KnockedBackTime, Seconds); }

	/** Keeps it facing as it is set (aiming, a held stance) whichever way it moves; Set Facing / Set Heading still turn it. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Character")
	void SetFacingLocked(bool bLocked) { bFacingLocked = bLocked; }

	/**
	 * Turns the way an attack starting now should go: where the player steers (this frame's input too), and away from
	 * the wall it slides down (a blade swung off a wall, not into it).
	 */
	void FaceForAttack();

	/** Thrown back by a hit (Hurt Stun Time): it can't act yet. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Health")
	bool IsHurtStunned() const { return HurtStunLeft > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	UOnyxMovementComponent* GetOnyxMovement() const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	EOnyxFacing GetFacing() const { return Facing; }

	UFUNCTION(BlueprintCallable, Category = "Onyx2D")
	void SetFacing(EOnyxFacing NewFacing);

	/** Seen from above (its movement plays the Top Down game style). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	bool IsTopDown() const;

	/**
	 * Top down: which way it looks, as its clips are drawn (Down, Up, Side and, when its Moveset has them, the diagonals);
	 * the side ones point the way it faces. None in a side view.
	 */
	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	EOnyxDirection GetDirection() const { return Direction; }

	/** Top down: where it looks, a unit vector (X right, Y up the screen) along its Direction: where it strikes and dashes. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	FVector2D GetAimVector() const;

	/** Top down: turns to look along Heading (X right, Y up the screen), snapped to the directions its clips are drawn in. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D")
	void SetHeading(FVector2D NewHeading);

	/** Turns to face the way it moves. Off: something else decides (Enemy AI looking at the player while it backs away). */
	UPROPERTY(BlueprintReadWrite, Category = "Onyx2D")
	bool bFaceMovement = true;

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
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void Destroyed() override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
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
	/**
	 * Gives the attack hitbox the shape of the attack under way: its centre forward (along the facing) and up from the
	 * capsule's centre, and its half size, cm. Clear Attack Shape puts back the Attack Reach and the hitbox's own size.
	 */
	void SetAttackShape(const FVector2D& Offset, const FVector2D& HalfSize);
	void ClearAttackShape();

	/** Undoes dying: the body back to whole, the movement clips playing again (Respawn calls it). */
	void ResetAfterDeath();

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
	bool bFacingLocked = false;
	/** Top down: where it looks (unit, X right, Y up the screen), and that as one of its drawn directions. */
	FVector2D Heading = FVector2D(0.f, -1.f);
	EOnyxDirection Direction = EOnyxDirection::None;
	/** The Moveset started another clip: an Up or Down clip shows as drawn, a side one turned the way it faces. */
	void HandleClipApplied();
	bool bAttackShape = false;
	FVector2D AttackShapeOffset = FVector2D::ZeroVector;
	FVector DefaultAttackExtent = FVector::ZeroVector;
	bool bHasDefaultAttackExtent = false;
	/** Seconds of lying dead left (counting down), and whether the body has started to dissolve. */
	float CorpseTimeLeft = -1.f;
	/** Seconds left of being knocked back by a hit: it doesn't turn round because of it (only when steered). */
	float KnockedBackTime = 0.f;
	float HurtStunLeft = 0.f;
	float BlinkLeft = 0.f;
	TWeakObjectPtr<class UAudioComponent> LoopSound;
	/** Degrees the sprite leans while swimming (see the movement's Max Lean). */
	float SpriteLean = 0.f;
	FVector SpriteBaseLocation = FVector::ZeroVector;
	bool bHasSpriteBase = false;
	/** How far the picture was last lowered onto a slope (sprites that aren't pixel-snapped). */
	float AppliedSlopeFoot = 0.f;
	TWeakObjectPtr<const UPaperFlipbook> AlignedFlipbook;
};
