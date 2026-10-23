// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/OnyxCombatTypes.h"
#include "OnyxBreakable.generated.h"

class UBoxComponent;
class UPaperFlipbook;
class UPaperSprite;
class UPaperSpriteComponent;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxBrokenSignature, AActor*, Breaker);

/**
 * Something to smash: a pot, an urn, a lamp, a barrel. The player's blows (and what they throw) knock it about and break
 * it; it bursts with an effect and a sound and can spill coins. Nothing walks into it: it never blocks the way.
 * Stands on its sprite's pivot (the feet of a prop sprite); until it has a sprite it is invisible in the game.
 */
UCLASS(meta = (DisplayName = "Onyx Breakable"))
class ONYX2DRUNTIME_API AOnyxBreakable : public AActor, public IOnyxDamageable
{
	GENERATED_BODY()

public:
	AOnyxBreakable();

	/** What blows land on: the sprite's size. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperSpriteComponent> SpriteComponent;

	/** Solid breakables: what stands in the way (the body's size). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Blocker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Breakable")
	TObjectPtr<UPaperSprite> Sprite;

	/** Blows it takes before it breaks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Breakable", meta = (ClampMin = "1", UIMax = "10"))
	int32 Hits = 1;

	/** Only the player's side can break it (foes and their shots pass by). Off: anything that deals damage does. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Breakable")
	bool bOnlyPlayerBreaks = true;

	/**
	 * It blocks the way until it breaks (a dam holding back water, a cracked wall): characters and objects can't pass its
	 * body. Off: it stands in no one's way.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Breakable")
	bool bSolid = false;

	/** Breaking it fills the player's energy like hitting a foe (off: only foes feed it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Breakable")
	bool bFeedsEnergy = false;

	/** Played where it bursts, standing on its feet (shards flying, dust). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Breakable|Look")
	TObjectPtr<UPaperFlipbook> BreakEffect;

	/** How far it rocks when struck and doesn't break, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Breakable|Look", meta = (ClampMin = "0", UIMax = "30"))
	float RockAngle = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Breakable|Sound")
	TObjectPtr<USoundBase> HitSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Breakable|Sound")
	TObjectPtr<USoundBase> BreakSound;

	/** Coins spilled when it breaks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Breakable|Coins", meta = (ClampMin = "0", UIMax = "20"))
	int32 Coins = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Breakable|Coins")
	TObjectPtr<UPaperFlipbook> CoinLook;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Breakable|Coins")
	TObjectPtr<UPaperFlipbook> CoinCollectEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Breakable|Coins")
	TObjectPtr<USoundBase> CoinSound;

	/** Called as it breaks, with what broke it. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Breakable")
	FOnyxBrokenSignature OnBroken;

	/** Breaks it now, as if Breaker had struck the last blow (spills its coins, plays its effect). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Breakable")
	void Break(AActor* Breaker);

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Breakable")
	bool IsBroken() const { return bBroken; }

	virtual bool ReceiveDamage_Implementation(const FOnyxDamageInfo& Damage) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	int32 HitsTaken = 0;
	bool bBroken = false;
	/** Rocking after a blow: time left, and which way it was struck. */
	float RockTime = 0.f;
	float RockSide = 1.f;
};
