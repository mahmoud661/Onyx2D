// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OnyxCombatComponent.generated.h"

class AOnyxCharacter;
class UOnyxMovesetComponent;
class UOnyxHitboxComponent;
class UOnyxMovementComponent;
class UPaperFlipbook;
class USoundBase;

/** One step of a combo. Times are in seconds from the start of the attack. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxAttack
{
	GENERATED_BODY()

	/** Moveset role to play (optional; Duration is used when missing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	FName Clip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.05"))
	float Duration = 0.32f;

	/** When the hitbox opens and closes. Ignored when the clip has a Hitbox window (the timeline decides then). */
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

	/** Pushed back this fast (cm/s) away from what it hits: keeps a fight at arm's length (0 = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0"))
	float Recoil = 0.f;

	/** The hitbox during this attack: centre forward and up from the capsule's centre, cm (zero size: the hitbox as set). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Hitbox")
	FVector2D HitboxOffset = FVector2D::ZeroVector;

	/** Its half size, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Hitbox")
	FVector2D HitboxSize = FVector2D::ZeroVector;

	/** Hitting something with it bounces the attacker up at this speed and gives back its air moves (a down-slash). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Hitbox", meta = (ClampMin = "0"))
	float Bounce = 0.f;

	/** Freeze steering/jumping during the attack (jumps are buffered). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bLockMovement = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxAttackSignature, int32, ComboIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxClashSignature, AActor*, Other);

/**
 * Melee combos with input buffering. Talks to the hitbox, moveset and movement components
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

	/** Attacks started in the air use this combo instead (empty: the same combo as on the ground). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat")
	TArray<FOnyxAttack> AirCombo;

	/** ...and with Down held, this one (a down-slash that bounces off foes; empty: the air combo). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat")
	TArray<FOnyxAttack> DownAirCombo;

	/** With Up held, on the ground or in the air: an up-slash (empty: Up changes nothing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat")
	TArray<FOnyxAttack> UpCombo;

	/** Attacking out of a dash or a roll: a strike that carries its speed (empty: no attacking while dashing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat")
	TArray<FOnyxAttack> DashCombo;

	/**
	 * Attacks started while running on the ground use this combo instead: cuts drawn over running legs, so the run carries
	 * on under them (empty: the ground combo).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat")
	TArray<FOnyxAttack> RunCombo;

	/** After the last attack of a combo the next one starts it again (slashes that alternate for as long as you press). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat")
	bool bLoopCombo = false;

	/** Attack presses made this long before the current attack can chain are kept. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat", meta = (ClampMin = "0"))
	float InputBufferTime = 0.3f;

	// --- Clashing ---

	/**
	 * When this attack's hitbox meets a foe's open attack hitbox the blades clash: neither is hurt, both are pushed
	 * apart, the foe is knocked off balance (its strike is over) and this character can't be hurt for a moment.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat|Clash")
	bool bClash = true;

	/** Both are pushed apart this fast, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat|Clash", meta = (ClampMin = "0", EditCondition = "bClash"))
	float ClashPush = 420.f;

	/** This character can't be hurt for this long after a clash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat|Clash", meta = (ClampMin = "0", Units = "s", EditCondition = "bClash"))
	float ClashInvulnerability = 0.25f;

	/** The foe is off balance this long (Enemy AI). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat|Clash", meta = (ClampMin = "0", Units = "s", EditCondition = "bClash"))
	float ClashStagger = 0.6f;

	/** Real-time freeze on a clash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat|Clash", meta = (ClampMin = "0", Units = "s", EditCondition = "bClash"))
	float ClashHitStop = 0.1f;

	/** Played where the blades meet (none: the hitbox's Hit Effect). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat|Clash", meta = (EditCondition = "bClash"))
	TObjectPtr<UPaperFlipbook> ClashEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat|Clash", meta = (EditCondition = "bClash"))
	TObjectPtr<USoundBase> ClashSound;

	/** Attacks cut hostile projectiles out of the air (they burst harmlessly). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Combat|Clash")
	bool bCutProjectiles = true;

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
	void SetHitbox(UOnyxHitboxComponent* InHitbox);

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Combat")
	FOnyxAttackSignature OnAttackStarted;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Combat")
	FOnyxAttackSignature OnAttackFinished;

	/** The blades met (see Clash). */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Combat")
	FOnyxClashSignature OnClash;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void HandleHit(AActor* Victim);

	bool CanStartAttack() const;
	void StartAttack(int32 Index);
	void FinishAttack();
	void SetHitWindow(bool bOpen);
	/** While the hitbox is open: blades it meets, projectiles it cuts. */
	void CheckClashes();
	void Clash(AOnyxCharacter& Other, const FVector& Where);

	UPROPERTY(Transient)
	TObjectPtr<UOnyxHitboxComponent> Hitbox;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxMovesetComponent> MovesetComponent;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxMovementComponent> Movement;

	int32 CurrentIndex = INDEX_NONE;
	/** The combo the attack under way is from: 0 ground, 1 air, 2 down in the air, 3 up, 4 out of a dash, 5 running. */
	uint8 ActiveCombo = 0;
	const TArray<FOnyxAttack>& GetActiveCombo() const;
	/** Foes whose blades this attack already clashed with. */
	TSet<TWeakObjectPtr<AActor>> ClashedThisAttack;
	/** The combo an attack starting now would come from. */
	uint8 PickCombo() const;
	/** For the log: facing right or left, or top down the way it looks. */
	FString LookingWay() const;
	float Elapsed = 0.f;
	float CurrentDuration = 0.f;
	float BufferRemaining = 0.f;
	bool bHitWindowOpen = false;
	/** The playing attack clip opens the hitbox itself (it has a Hitbox window). */
	bool bClipOwnsHitWindow = false;
};
