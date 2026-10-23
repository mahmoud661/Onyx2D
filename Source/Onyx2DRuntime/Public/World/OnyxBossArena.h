// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxBossArena.generated.h"

class AOnyxCharacter;
class AOnyxGate;
class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnyxArenaSignature);

/**
 * Onyx Boss Arena: where a boss fight happens. The player stepping into its Start Trigger shuts its gates and starts the
 * boss (its Boss component) inside Area. If the player falls, the arena resets while the screen is dark (the boss
 * whole and back in its place, the gates as they were); once the boss is beaten its gates open.
 */
UCLASS(meta = (DisplayName = "Onyx Boss Arena"))
class ONYX2DRUNTIME_API AOnyxBossArena : public AActor
{
	GENERATED_BODY()

public:
	AOnyxBossArena();

	/** The arena: the boss teleports and summons inside it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Area;

	/** The fight starts when the player steps in here. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> StartTrigger;

	/** The boss: a character with a Boss component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Arena")
	TObjectPtr<AOnyxCharacter> Boss;

	/** Shut as the fight starts, opened when it is won (and put back as they started if the player falls). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Arena")
	TArray<TObjectPtr<AOnyxGate>> Gates;

	/** Seconds after the boss falls before the gates open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Arena", meta = (ClampMin = "0", Units = "s"))
	float OpenDelay = 2.5f;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Arena")
	bool IsFighting() const { return bFighting; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Arena")
	bool IsWon() const { return bWon; }

	/** Starts the fight now (as if the player had stepped in). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Arena")
	void StartFight();

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Arena")
	FOnyxArenaSignature OnFightStarted;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Arena")
	FOnyxArenaSignature OnFightWon;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Arena")
	FOnyxArenaSignature OnFightReset;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandlePlayerDeath(AActor* Killer);

	UFUNCTION()
	void HandleBossDefeated();

	void ResetArena();
	void OpenGates();
	class UOnyxBossComponent* GetBossBrain() const;

	bool bFighting = false;
	bool bWon = false;
	FTimerHandle Timer;
};
