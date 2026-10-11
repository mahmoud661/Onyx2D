// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/OnyxCharacter.h"
#include "OnyxEnemyCharacter.generated.h"

class UPaperFlipbook;
class UOnyxHitboxComponent;
class UOnyxPatrolComponent;

/** Basic enemy: patrols, hurts the player on contact, disappears after dying. */
UCLASS()
class ONYX2DRUNTIME_API AOnyxEnemyCharacter : public AOnyxCharacter
{
	GENERATED_BODY()

public:
	AOnyxEnemyCharacter(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxPatrolComponent> Patrol;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxHitboxComponent> ContactHitbox;

	/** Jumping on its head kills it and bounces the player (Mario style) instead of hurting the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Stomp")
	bool bStompable = true;

	/** Upward speed given to the player after a stomp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Stomp", meta = (ClampMin = "0", EditCondition = "bStompable"))
	float StompBounce = 700.f;

	/** Played once where it dies (a puff, an explosion); the body disappears at once when set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperFlipbook> DeathEffect;

	/** Seconds the body stays after death (0 = forever). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (ClampMin = "0"))
	float CorpseLifetime = 1.f;

protected:
	virtual void PostInitializeComponents() override;
	virtual void HandleDeath(AActor* Killer) override;

private:
	/** True (and handles it) when Victim landed on top of this enemy. */
	bool TryStomp(AActor* Victim);
};
