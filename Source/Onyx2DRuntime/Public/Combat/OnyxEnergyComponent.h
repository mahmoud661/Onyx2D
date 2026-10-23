// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OnyxEnergyComponent.generated.h"

class UPaperFlipbook;
class USoundBase;
struct FOnyxDamageInfo;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnyxEnergyChangedSignature, float, NewEnergy, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnyxMendSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxMendStoppedSignature, bool, bInterrupted);

/**
 * Energy: gathered by landing blows, spent on mending wounds. Each hit the character's attacks land fills it a little;
 * holding Mend (standing still, kneeling) drains one Mend Cost over Mend Time and heals Mend Heal. Getting hit while
 * mending breaks it off and the energy already drained is lost, so a fight has to be won a moment of calm before
 * healing in it. Holding on keeps mending while there is energy and a wound to mend.
 *
 * Other uses (a special attack, a spell) can spend it with Spend Energy.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Energy"))
class ONYX2DRUNTIME_API UOnyxEnergyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxEnergyComponent();

	// --- Gathering ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy", meta = (ClampMin = "1"))
	float MaxEnergy = 99.f;

	/** Gathered from each foe the character's attacks hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy", meta = (ClampMin = "0"))
	float EnergyPerHit = 11.f;

	/** Energy at the start (and after a respawn when it is less). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy", meta = (ClampMin = "0"))
	float StartEnergy = 0.f;

	// --- Mending ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy|Mend")
	bool bCanMend = true;

	/** Energy one mend uses up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy|Mend", meta = (ClampMin = "1", EditCondition = "bCanMend"))
	float MendCost = 33.f;

	/** Health one mend gives back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy|Mend", meta = (ClampMin = "0", EditCondition = "bCanMend"))
	float MendHeal = 1.f;

	/** Settling down before the energy starts to flow (only before the first mend of a hold). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy|Mend", meta = (ClampMin = "0", Units = "s", EditCondition = "bCanMend"))
	float MendWindUp = 0.25f;

	/** How long one mend drains energy before it heals. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy|Mend", meta = (ClampMin = "0.05", Units = "s", EditCondition = "bCanMend"))
	float MendTime = 0.9f;

	/** Holding on mends again (without the wind-up) while there is energy and a wound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy|Mend", meta = (EditCondition = "bCanMend"))
	bool bKeepMending = true;

	/** Mending in the air too (off: only standing on the ground). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy|Mend", meta = (EditCondition = "bCanMend"))
	bool bMendInAir = false;

	/** Moveset role played (looping) while mending; none: the character just stands still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy|Mend", meta = (EditCondition = "bCanMend"))
	FName MendClip = TEXT("Mend");

	/** Played on the character as each mend starts to drain (light gathering in). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy|Mend", meta = (EditCondition = "bCanMend"))
	TObjectPtr<UPaperFlipbook> GatherEffect;

	/** Played on the character when a mend heals. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy|Mend", meta = (EditCondition = "bCanMend"))
	TObjectPtr<UPaperFlipbook> MendedEffect;

	/** The character flashes this colour when a mend heals (alpha 0: no flash, the Mended Effect alone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy|Mend", meta = (EditCondition = "bCanMend"))
	FLinearColor MendedFlash = FLinearColor(0.75f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy|Mend", meta = (EditCondition = "bCanMend"))
	TObjectPtr<USoundBase> GatherSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Energy|Mend", meta = (EditCondition = "bCanMend"))
	TObjectPtr<USoundBase> MendedSound;

	// --- Commands ---

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Energy")
	void AddEnergy(float Amount);

	/** Uses up Amount when there is that much; false (and nothing used) when there isn't. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Energy")
	bool SpendEnergy(float Amount);

	/** Starts mending (Mend pressed). False when it can't now. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Energy")
	bool StartMend();

	/** Stops mending (Mend released): energy drained for a mend that hasn't healed yet is lost. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Energy")
	void StopMend() { EndMend(false); }

	// --- Queries ---

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Energy")
	float GetEnergy() const { return Energy; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Energy")
	float GetEnergyFraction() const { return MaxEnergy > 0.f ? Energy / MaxEnergy : 0.f; }

	/** Enough energy, a wound to mend, and nothing in the way (falling, dead, mid-dash). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Energy")
	bool CanMend() const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Energy")
	bool IsMending() const { return bMending; }

	/** How far the mend under way is, 0 to 1 (0 while settling down). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Energy")
	float GetMendProgress() const;

	// --- Events ---

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Energy")
	FOnyxEnergyChangedSignature OnEnergyChanged;

	/** There is enough energy for a mend again. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Energy")
	FOnyxMendSignature OnMendReady;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Energy")
	FOnyxMendSignature OnMendStarted;

	/** A mend healed. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Energy")
	FOnyxMendSignature OnMended;

	/** Mending stopped: let go, out of energy or wounds (false), or broken off by a hit or a fall (true). */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Energy")
	FOnyxMendStoppedSignature OnMendStopped;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void HandleAttackHit(AActor* Victim);

	UFUNCTION()
	void HandleDamaged(const FOnyxDamageInfo& Damage);

	UFUNCTION()
	void HandleDeath(AActor* Killer);

	void SetEnergy(float NewEnergy);
	/** Begins draining for one mend (after the wind-up, or straight after the last mend while held). */
	void BeginDrain();
	void EndMend(bool bInterrupted);
	bool IsOnGround() const;
	bool IsWounded() const;

	UPROPERTY(VisibleInstanceOnly, Category = "Onyx2D|Energy")
	float Energy = 0.f;

	bool bMending = false;
	bool bDraining = false;
	float MendTimer = 0.f;
	/** Energy drained so far for the mend under way. */
	float Drained = 0.f;
};
