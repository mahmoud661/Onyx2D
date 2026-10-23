// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/OnyxProjectile.h"
#include "Components/ActorComponent.h"
#include "OnyxEnemyAIComponent.generated.h"

class AOnyxCharacter;
class UOnyxMovementComponent;
class UOnyxPatrolComponent;
class UPaperFlipbook;
class USoundBase;
struct FOnyxDamageInfo;

/** How an enemy attack moves. */
UENUM(BlueprintType)
enum class EOnyxEnemyMove : uint8
{
	/** A quick dash forward with the attack hitbox out: a lunge, a swipe. */
	Lunge,
	/** A jump at the player. */
	Pounce,
	/** Throws or casts a projectile at the player. */
	Shoot,
	/** Flyers: rises, then swoops down at where the player was. */
	Dive,
	/** Runs at the player far faster than it walks, until a wall, a ledge or the time is up. */
	Charge
};

/** One attack: what it does, when it may start, and its rhythm (wind-up to read it, strike, recovery to punish it). */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxEnemyAttack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	EOnyxEnemyMove Move = EOnyxEnemyMove::Lunge;

	/** Moveset role played from the wind-up on (none: keeps its current animation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	FName Clip;

	/** Starts only with the player this far away sideways, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0"))
	float MinRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0"))
	float MaxRange = 260.f;

	/** And no more than this far above or below, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0"))
	float MaxHeight = 160.f;

	/** The tell: it stops, faces the player and gets ready (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Rhythm", meta = (ClampMin = "0"))
	float WindUp = 0.45f;

	/** How long the strike lasts: the dash, the swoop, the charge, the rest of the cast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Rhythm", meta = (ClampMin = "0.05"))
	float StrikeTime = 0.3f;

	/** Open to a counter-attack afterwards (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Rhythm", meta = (ClampMin = "0"))
	float Recover = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Rhythm", meta = (ClampMin = "0"))
	float Cooldown = 1.4f;

	/** Lunge, Charge and Dive speed; Pounce: how fast it leaps across (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Motion", meta = (ClampMin = "0"))
	float Speed = 700.f;

	/** Pounce: how high it leaps. Dive: how high above the player it rises first (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Motion", meta = (ClampMin = "0"))
	float Height = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Damage", meta = (ClampMin = "0"))
	float Damage = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Damage")
	FVector2D Knockback = FVector2D(450.f, 300.f);

	/** The hitbox while it strikes: centre forward and up from the capsule's centre, cm (zero size: the character's own hitbox). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Damage")
	FVector2D HitboxOffset = FVector2D::ZeroVector;

	/** Its half size, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Damage")
	FVector2D HitboxSize = FVector2D::ZeroVector;

	/** Shoot: what it throws. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Shoot")
	FOnyxProjectileSpec Projectile;

	/** Shoot: where the projectile leaves from (cm forward and up from its middle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Shoot")
	FVector2D Muzzle = FVector2D(40.f, 20.f);

	/** Shoot: how many, fanned out by Spread degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Shoot", meta = (ClampMin = "1", UIMax = "7"))
	int32 Shots = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Shoot", meta = (ClampMin = "0", UIMax = "60"))
	float Spread = 15.f;

	/** Played as the strike starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects")
	TObjectPtr<USoundBase> Sound;

	/** Played as the wind-up starts: the warning, heard as well as seen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects")
	TObjectPtr<USoundBase> TellSound;
};

UENUM(BlueprintType)
enum class EOnyxEnemyState : uint8
{
	/** Asleep until the player comes near (a skeleton under the ground). */
	Dormant,
	/** Waking up (its wake clip playing). */
	Waking,
	/** Going about its business (its Patrol behaviour). */
	Patrol,
	/** Has just seen the player. */
	Alert,
	Chase,
	WindUp,
	Strike,
	Recover,
	/** Knocked off balance by a hit. */
	Stagger,
	Dead
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnyxEnemyStateSignature, EOnyxEnemyState, NewState, EOnyxEnemyState, OldState);

/**
 * Enemy AI: what turns a patrolling character into a foe. It patrols (with its Patrol behaviour) until it sees the player,
 * reacts, chases, and attacks with readable rhythm: a wind-up to warn, the strike, a recovery to punish. A hit staggers
 * it (except mid-strike); it gives up when the player is gone for a while, and scatters coins when it dies.
 *
 * In a Top Down level it does the same across the ground: it sees the player within Sight Range all round, walks round
 * walls to reach them, and strikes, charges and shoots in any direction (a pounce or a dive becomes a lunge).
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Enemy AI"))
class ONYX2DRUNTIME_API UOnyxEnemyAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxEnemyAIComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI", meta = (TitleProperty = "Move"))
	TArray<FOnyxEnemyAttack> Attacks;

	// --- Senses ---

	/** Sees the player this far away sideways (cm)... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Senses", meta = (ClampMin = "0"))
	float SightRange = 650.f;

	/** ...and this far above or below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Senses", meta = (ClampMin = "0"))
	float SightHeight = 280.f;

	/** Walls block its view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Senses")
	bool bNeedLineOfSight = true;

	/** Gives up after not seeing the player for this long (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Senses", meta = (ClampMin = "0"))
	float ForgetTime = 3.f;

	/** The pause after it first sees the player (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Senses", meta = (ClampMin = "0"))
	float AlertTime = 0.4f;

	/** Played when it notices the player (or wakes up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Senses")
	TObjectPtr<USoundBase> AlertSound;

	/** Walkers give a little start when they notice you. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Senses")
	bool bHopWhenAlerted = true;

	// --- Moving ---

	/** Walkers chase this much faster than they patrol. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Moving", meta = (ClampMin = "0.1"))
	float ChaseSpeedScale = 1.4f;

	/** Flyers' speed while chasing, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Moving", meta = (ClampMin = "0"))
	float FlySpeed = 260.f;

	/** Flyers keep this high above the player while chasing, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Moving")
	float HoverHeight = 180.f;

	/** Keeps at least this far from the player, backing away when it comes closer (casters). 0 = closes in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Moving", meta = (ClampMin = "0"))
	float KeepDistance = 0.f;

	// --- Hits ---

	/** Knocked off balance this long by a hit (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Hits", meta = (ClampMin = "0"))
	float StaggerTime = 0.3f;

	/** A hit stops even a strike under way (off: it commits to its strikes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Hits")
	bool bStaggerDuringStrike = false;

	/** The colour it flashes at the start of a wind-up (the tell). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Hits")
	FLinearColor TellColor = FLinearColor(1.f, 0.45f, 0.25f);

	// --- Waking up ---

	/** Lies dormant (DormantClip) until it sees the player, then plays WakeClip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Waking")
	bool bStartsDormant = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Waking", meta = (EditCondition = "bStartsDormant"))
	FName DormantClip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Waking", meta = (EditCondition = "bStartsDormant"))
	FName WakeClip;

	// --- Reward ---

	/** Coins scattered when it dies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Reward", meta = (ClampMin = "0", UIMax = "20"))
	int32 CoinDrop = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Reward")
	TObjectPtr<UPaperFlipbook> CoinLook;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Reward")
	TObjectPtr<UPaperFlipbook> CoinCollectEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Enemy AI|Reward")
	TObjectPtr<USoundBase> CoinSound;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Enemy AI")
	EOnyxEnemyState GetState() const { return State; }

	/** Knocks it off balance for Seconds whatever it is doing, a strike too (its blade met the player's). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Enemy AI")
	void Interrupt(float Seconds);

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Enemy AI")
	FOnyxEnemyStateSignature OnStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void HandleDamaged(const FOnyxDamageInfo& Damage);

	UFUNCTION()
	void HandleDeath(AActor* Killer);

	void Enter(EOnyxEnemyState NewState);
	/** How long the stagger under way lasts (0: Stagger Time). */
	float StaggerFor = 0.f;
	bool CanSee(const APawn* Player) const;
	void FacePlayer(const APawn* Player);
	void StopMoving();
	/** Walking that way is safe (no wall in the way, ground ahead). */
	bool CanWalk(float Direction) const;
	void TickChase(float DeltaTime, const APawn* Player);
	void TickStrike(float DeltaTime, const APawn* Player);
	void BeginStrike(const APawn* Player);
	void SetHitbox(bool bOpen);
	void SetRunSpeed(float Speed);
	void Fly(const FVector& Velocity, float DeltaTime);
	void DropCoins();
	const FOnyxEnemyAttack* GetAttack() const { return Attacks.IsValidIndex(CurrentAttack) ? &Attacks[CurrentAttack] : nullptr; }
	/** Seen from above (its movement plays the Top Down game style). */
	bool IsTopDown() const;
	/** A flyer seen from the side (from above, flyers get around like walkers). */
	bool IsFlyer() const { return bFlyer && !IsTopDown(); }
	/** Top down: walks towards Way (X right, Y up the screen), round walls in the way. */
	void WalkToward(const FVector2D& Way);
	/** Top down: nothing solid just ahead along Way. */
	bool CanWalkToward(const FVector2D& Way) const;
	/** Top down: the way it strikes, fixed as the wind-up starts. */
	FVector2D StrikeVector = FVector2D(1.f, 0.f);
	/** Top down: the side it turned last to get round a wall. */
	float WayBias = 1.f;

	TWeakObjectPtr<AOnyxCharacter> Character;
	TWeakObjectPtr<UOnyxPatrolComponent> Patrol;
	TWeakObjectPtr<UOnyxMovementComponent> Movement;
	EOnyxEnemyState State = EOnyxEnemyState::Patrol;
	float StateTime = 0.f;
	float SinceSeen = 0.f;
	float WakeLength = 0.f;
	float BaseRunSpeed = 0.f;
	bool bFlyer = false;
	bool bHitboxOpen = false;
	int32 CurrentAttack = INDEX_NONE;
	float StrikeDirection = 1.f;
	FVector DiveDirection = FVector::ZeroVector;
	TArray<float> Cooldowns;
};
