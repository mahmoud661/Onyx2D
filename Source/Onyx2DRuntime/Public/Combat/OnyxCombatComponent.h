// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OnyxCombatComponent.generated.h"

class UOnyxAnimatorComponent;
class UOnyxHitboxComponent;
class UOnyxMovementComponent;

/** One step of a combo. Times are in seconds from the start of the attack. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxAttack
{
	GENERATED_BODY()

	/** Animator animation to play (optional; Duration is used when missing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	FName Animation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.05"))
	float Duration = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0"))
	float HitWindowStart = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0"))
	float HitWindowEnd = 0.18f;

	/** From this time the next combo attack may start if it was requested. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0"))
	float ChainTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0"))
	float Damage = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	FVector2D Knockback = FVector2D(420.f, 240.f);

	/** Forward speed applied when the attack starts (a small lunge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float ForwardImpulse = 180.f;

	/** Freeze steering/jumping during the attack (jumps are buffered). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bLockMovement = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxAttackSignature, int32, ComboIndex);

/**
 * Melee combos with input buffering. Talks to the hitbox, animator and movement components
 * on the same actor if they exist; each is optional.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent))
class ONYX2DRUNTIME_API UOnyxCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxCombatComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat")
	TArray<FOnyxAttack> Combo;

	/** Attack presses made this long before the current attack can chain are kept. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat", meta = (ClampMin = "0"))
	float InputBufferTime = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat")
	bool bAllowAirAttacks = true;

	/** Direction attacks face: +1 right, -1 left. Usually set by the owning character. */
	UPROPERTY(BlueprintReadWrite, Category = "Onyx2D|Combat")
	float FacingSign = 1.f;

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Combat")
	void RequestAttack();

	/** Stops the current attack immediately (e.g. when hurt). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Combat")
	void CancelAttack();

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Combat")
	bool IsAttacking() const { return CurrentIndex != INDEX_NONE; }

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Combat")
	void SetHitbox(UOnyxHitboxComponent* InHitbox) { Hitbox = InHitbox; }

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Combat")
	FOnyxAttackSignature OnAttackStarted;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Combat")
	FOnyxAttackSignature OnAttackFinished;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool CanStartAttack() const;
	void StartAttack(int32 Index);
	void FinishAttack();
	void SetHitWindow(bool bOpen);

	UPROPERTY(Transient)
	TObjectPtr<UOnyxHitboxComponent> Hitbox;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxAnimatorComponent> Animator;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxMovementComponent> Movement;

	int32 CurrentIndex = INDEX_NONE;
	float Elapsed = 0.f;
	float CurrentDuration = 0.f;
	float BufferRemaining = 0.f;
	bool bHitWindowOpen = false;
};
