// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaperCharacter.h"
#include "Combat/OnyxCombatTypes.h"
#include "Core/Onyx2DTypes.h"
#include "OnyxCharacter.generated.h"

class UOnyxAnimatorComponent;
class UOnyxCombatComponent;
class UOnyxHealthComponent;
class UOnyxHitboxComponent;
class UOnyxMovementComponent;
class UPaperFlipbook;
class UStaticMeshComponent;

/**
 * Base 2D character: platformer movement, health, flipbook animator, melee combat and a team.
 * Each feature lives in its own component; this class only wires them together.
 */
UCLASS()
class ONYX2DRUNTIME_API AOnyxCharacter : public APaperCharacter, public IOnyxDamageable, public IOnyxTeamMember
{
	GENERATED_BODY()

public:
	AOnyxCharacter(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxAnimatorComponent> Animator;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Animation")
	FName HurtAnimation = TEXT("Hurt");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Animation")
	FName DeathAnimation = TEXT("Death");

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

	/** Called when health reaches zero. Base: stops combat, plays the death animation, fires OnDied. */
	UFUNCTION()
	virtual void HandleDeath(AActor* Killer);

	/** Chooses facing each frame (movement input / velocity). Skipped while attacking. */
	virtual void UpdateFacing();

public:
	/** Moves the flipbook so its bottom sits on the bottom of the capsule (see bAlignSpriteToCapsule). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Rendering")
	void AlignSpriteToCapsule();

private:
	void ApplyFacing();
	/** Shows the coloured stand-in body only while the character has no animation. */
	void UpdatePlaceholder();

	/** Puts the sprite back on whole art pixels this frame (see bPixelSnapSprite). */
	void SnapSpriteToPixels();
	/** Sprite position before pixel snapping (where AlignSpriteToCapsule / the Blueprint put it). */
	FVector& GetSpriteBaseLocation();

	EOnyxFacing Facing = EOnyxFacing::Right;
	FVector SpriteBaseLocation = FVector::ZeroVector;
	bool bHasSpriteBase = false;
	TWeakObjectPtr<const UPaperFlipbook> AlignedFlipbook;
};
