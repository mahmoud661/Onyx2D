// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/OnyxProjectile.h"
#include "Components/ActorComponent.h"
#include "OnyxBossComponent.generated.h"

class AOnyxCharacter;
class UOnyxMovementComponent;
class UPaperFlipbook;
class USoundBase;
struct FOnyxDamageInfo;

/** What a boss does during one step of a move. */
UENUM(BlueprintType)
enum class EOnyxBossAction : uint8
{
	/** Nothing but its clip: a tell, a pause, a recovery to punish. */
	None,
	/** Its attack hitbox is out (Hitbox Offset / Size). */
	Strike,
	/** Moves towards the player at Speed, stopping Distance from it. */
	Glide,
	/** Goes straight ahead at Speed with the hitbox out, until a wall or the end of the step. */
	Rush,
	/** Jumps at where the player is now (Height high, at most Speed across), the hitbox out. */
	Leap,
	/** Is somewhere else at once (Place). */
	Teleport,
	/** Fires Shots projectiles (Aim, Spread, Shot Interval). */
	Shoot,
	/** Brings Summon Count of Summon Class into the arena, away from the player. */
	Summon
};

/** Where a Teleport step puts the boss. */
UENUM(BlueprintType)
enum class EOnyxBossPlace : uint8
{
	/** Distance behind the player (the side it isn't facing), on the arena's floor (never on a ledge). */
	BehindPlayer,
	/** Distance in front of the player, on the arena's floor. */
	InFrontOfPlayer,
	/** The end of the arena farther from the player, on the floor. */
	FarFromPlayer,
	/** The middle of the arena, on the floor. */
	ArenaCentre,
	/** Over the middle of the arena, Distance above its floor (0: under its roof). Fly there: Flying on. */
	ArenaTop,
	/** Where it stood when the fight began. */
	Start
};

/** Where a Shoot step aims. */
UENUM(BlueprintType)
enum class EOnyxBossAim : uint8
{
	/** At the player, the shots fanned out by Spread. */
	AtPlayer,
	/** Straight down, fanned out by Spread. */
	Down,
	/** One after another at points across the arena floor, left to right or right to left (a sweep to run through). */
	AcrossFloor
};

/** One step of a boss move: a clip, a time, and what happens meanwhile. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxBossStep
{
	GENERATED_BODY()

	/** Moveset role played from the start of the step (none: the current animation goes on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
	FName Clip;

	/** How long the step lasts, seconds (shorter in later phases: see Phase Speed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step", meta = (ClampMin = "0.02", Units = "s"))
	float Time = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
	EOnyxBossAction Action = EOnyxBossAction::None;

	/** It flashes its Tell Color as the step starts: the warning that an attack is coming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
	bool bTell = false;

	/** It turns to the player as the step starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
	bool bFacePlayer = true;

	/** Can't be hurt during the step (blows glance off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
	bool bInvulnerable = false;

	/** Unseen and untouchable: vanished. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
	bool bHidden = false;

	/** No gravity: it hovers (with Teleport to Arena Top, it hangs over the arena). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
	bool bFlying = false;

	// --- Strike, Rush, Leap ---

	/** The hitbox: centre forward and up from the capsule's centre, cm (zero size: the character's own hitbox). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Hit")
	FVector2D HitboxOffset = FVector2D::ZeroVector;

	/** Its half size, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Hit")
	FVector2D HitboxSize = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Hit", meta = (ClampMin = "0"))
	float Damage = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Hit")
	FVector2D Knockback = FVector2D(500.f, 350.f);

	/** The player's blade can meet this one (a clash). Off for smoke, fire, a body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Hit")
	bool bClashable = false;

	// --- Glide, Rush, Leap, Teleport ---

	/** Glide and Rush: how fast, cm/s. Leap: at most this fast across. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Motion", meta = (ClampMin = "0"))
	float Speed = 600.f;

	/** Leap: how high, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Motion", meta = (ClampMin = "0"))
	float Height = 300.f;

	/** Glide: stops this far from the player. Teleport: this far from the player (Behind / In Front Of Player), or this high over the floor (Arena Top), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Motion", meta = (ClampMin = "0"))
	float Distance = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Motion")
	EOnyxBossPlace Place = EOnyxBossPlace::BehindPlayer;

	// --- Shoot ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Shoot")
	FOnyxProjectileSpec Projectile;

	/** Where the shots leave from, cm forward and up from its middle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Shoot")
	FVector2D Muzzle = FVector2D(40.f, 40.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Shoot", meta = (ClampMin = "1", UIMax = "24"))
	int32 Shots = 1;

	/** Degrees between fanned shots (At Player, Down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Shoot", meta = (ClampMin = "0", UIMax = "60"))
	float Spread = 15.f;

	/** Seconds between shots (0: all at once). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Shoot", meta = (ClampMin = "0", Units = "s"))
	float ShotInterval = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Shoot")
	EOnyxBossAim Aim = EOnyxBossAim::AtPlayer;

	// --- Summon ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Summon")
	TSubclassOf<AActor> SummonClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Summon", meta = (ClampMin = "1", UIMax = "6"))
	int32 SummonCount = 1;

	// --- Effects ---

	/** Played where it is as the step starts (after a teleport: where it went). Summons get it too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Effects")
	TObjectPtr<UPaperFlipbook> Effect;

	/** The effect's place, cm forward and up from its middle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Effects")
	FVector2D EffectOffset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Effects")
	TObjectPtr<USoundBase> Sound;

	/** Screen shake as the step starts (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Effects", meta = (ClampMin = "0", ClampMax = "1"))
	float Trauma = 0.f;

	/** Left behind it all through the step, every Trail Interval (smoke after a rush). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Effects")
	TObjectPtr<UPaperFlipbook> Trail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Effects", meta = (ClampMin = "0.02", Units = "s"))
	float TrailInterval = 0.08f;
};

/** One move of a boss: steps played in order, when it may be chosen, and what may follow it. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxBossMove
{
	GENERATED_BODY()

	/** For follow-ups, phases and the log. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (TitleProperty = "Action"))
	TArray<FOnyxBossStep> Steps;

	/** Chosen only with the player this far away sideways, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move|Choosing", meta = (ClampMin = "0"))
	float MinRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move|Choosing", meta = (ClampMin = "0"))
	float MaxRange = 5000.f;

	/** The phases it is used in (0 = the first). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move|Choosing", meta = (ClampMin = "0"))
	int32 FirstPhase = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move|Choosing", meta = (ClampMin = "0"))
	int32 LastPhase = 99;

	/** How likely it is picked against the others that fit. 0: only as a follow-up or a phase's opening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move|Choosing", meta = (ClampMin = "0"))
	float Weight = 1.f;

	/** Not chosen again for this long, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move|Choosing", meta = (ClampMin = "0", Units = "s"))
	float Cooldown = 0.f;

	/** Moves that may come straight after it (one at random, no rest between). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move|Follow-up")
	TArray<FName> FollowUps;

	/** How likely a follow-up is, 0 to 1, in each phase from the first (the last value goes on for later phases). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move|Follow-up")
	TArray<float> FollowUpChance = { 1.f };
};

/** A stage of the fight, from a share of its health down. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxBossPhase
{
	GENERATED_BODY()

	/** Begins when its health falls to this share of the most (1 = from the start). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase", meta = (ClampMin = "0", ClampMax = "1"))
	float HealthFraction = 1.f;

	/** Everything it does is this much faster (tells, attacks, its animations). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase", meta = (ClampMin = "0.5", ClampMax = "3"))
	float Speed = 1.f;

	/** The move it opens the phase with, unhurtable (a roar, a summons); none: it just goes on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase")
	FName OpeningMove;

	/** The pause between its moves, seconds (from X to Y, at random). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase")
	FVector2D Rest = FVector2D(0.5f, 1.f);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnyxBossSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxBossPhaseSignature, int32, Phase);

/**
 * A boss: a fight with an opening, stages, and moves that read. Each move is a list of steps (a tell, the attack, a
 * recovery to punish it; a vanish, the place it comes back, an ambush), chosen by how far away the player is, how
 * often they may come and what may follow them. At each phase's share of health it opens the next stage, faster and
 * with new moves. A run of blows knocks it off balance (Stagger Hits), open to punishment. Its death slows the world.
 *
 * It waits until the fight starts (an Onyx Boss Arena starts it), and comes back whole if the player dies.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Boss"))
class ONYX2DRUNTIME_API UOnyxBossComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxBossComponent();

	/** Shown as the fight starts and over its health bar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss")
	FText BossName;

	/** Shown over its name as the fight starts (a title: "Keeper of the Ruins"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss")
	FText BossTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss", meta = (TitleProperty = "Name"))
	TArray<FOnyxBossMove> Moves;

	/** From the first; each begins at its share of health. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss")
	TArray<FOnyxBossPhase> Phases;

	/** The move it opens the fight with, unhurtable (appearing, a roar). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss")
	FName IntroMove;

	/**
	 * Fighters pass through it and it through them (touching it still hurts: Contact Damage). Off: its body blocks, and
	 * a rush stops at the first one in its way.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss")
	bool bPassThroughFighters = true;

	/** Unseen until the fight starts (it appears in its Intro Move). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss")
	bool bHiddenUntilFight = false;

	/** At most this many moves one straight after another (a move and its follow-ups) before it pauses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss", meta = (ClampMin = "1"))
	int32 MaxChain = 3;

	/** The colour it flashes as an attack's tell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss")
	FLinearColor TellColor = FLinearColor(1.f, 0.45f, 0.3f);

	// --- Stagger ---

	/** This many blows without a stagger knock it off balance (0: never). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss|Stagger", meta = (ClampMin = "0"))
	int32 StaggerHits = 10;

	/** How long it stays off balance, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss|Stagger", meta = (ClampMin = "0", Units = "s"))
	float StaggerTime = 2.f;

	/** Moveset role it holds while off balance (looping). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss|Stagger")
	FName StaggerClip;

	/** Played as it is knocked off balance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss|Stagger")
	TObjectPtr<USoundBase> StaggerSound;

	/** The move it gets up with (getting away, a counter); none: it rests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss|Stagger")
	FName AfterStaggerMove;

	/** Back on its feet, blows don't count towards the next stagger for this long: it can't be knocked down again and again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss|Stagger", meta = (ClampMin = "0", Units = "s"))
	float StaggerGrace = 4.f;

	// --- Answering ---

	/**
	 * The move it answers a run of blows with between its attacks (a burst that throws the player off, a vanish), so
	 * standing at it and hacking away is punished. None: it takes them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss|Answer")
	FName AnswerMove;

	/** This many blows between two of its moves bring the answer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss|Answer", meta = (ClampMin = "1"))
	int32 AnswerHits = 3;

	// --- Defeat ---

	/** The world slows to this as it dies... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss|Defeat", meta = (ClampMin = "0.01", ClampMax = "1"))
	float DeathSlowMotion = 0.25f;

	/** ...for this long, real seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss|Defeat", meta = (ClampMin = "0", Units = "s"))
	float DeathSlowMotionTime = 1.2f;

	/** Played where it falls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Boss|Defeat")
	TObjectPtr<UPaperFlipbook> DeathEffect;

	// --- The fight ---

	/** Begins the fight in an arena (teleports and summons stay inside it). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Boss")
	void StartFight(const FBox& ArenaBounds);

	/** Back to before the fight, whole (the player died). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Boss")
	void ResetFight();

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Boss")
	bool IsFightActive() const { return bFightActive; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Boss")
	bool IsDefeated() const { return bDefeated; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Boss")
	int32 GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Boss")
	float GetHealthFraction() const;

	/** Real seconds since the fight started (the title card's clock); since its defeat once it is down. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Boss")
	float GetFightTime() const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Boss")
	bool IsStaggered() const { return State == EState::Staggered; }

	/** The name of the move under way (none while resting). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Boss")
	FName GetCurrentMove() const;

	/** Knocks it off balance now (testing). */
	void ForceStagger() { Stagger(); }

	/** Starts one of its moves by name now, whatever it was doing (testing). */
	void ForceMove(FName Name);

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Boss")
	FOnyxBossSignature OnFightStarted;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Boss")
	FOnyxBossPhaseSignature OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Boss")
	FOnyxBossSignature OnStaggered;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Boss")
	FOnyxBossSignature OnDefeated;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Boss")
	FOnyxBossSignature OnFightReset;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	enum class EState : uint8
	{
		Waiting,
		Resting,
		Moving,
		Staggered,
		Defeated
	};

	UFUNCTION()
	void HandleDamaged(const FOnyxDamageInfo& Damage);

	UFUNCTION()
	void HandleDeath(AActor* Killer);

	APawn* GetPlayer() const;
	int32 FindMove(FName Name) const;
	/** A move that fits now (range, phase, cooldown), weighted at random; INDEX_NONE when none does. */
	int32 PickMove() const;
	/** bForced: unhurtable throughout (an opening, a phase's opening). */
	void StartMove(int32 Index, bool bForced);
	void BeginStep();
	void TickStep(float DeltaTime);
	void EndStep();
	void FinishMove();
	/** Stops the move under way at once. */
	void Interrupt();
	void Rest();
	void Stagger();
	void EnterPhase(int32 NewPhase);
	void FacePlayer();
	void SetHidden(bool bHidden);
	void SetFlying(bool bFlying);
	void SetHitbox(const FOnyxBossStep& Step, bool bOpen);
	FVector FindPlace(EOnyxBossPlace Place, float Distance) const;
	/** Where the capsule stands on the first floor under (X, FromZ); the current height when there is none. */
	float FloorZ(float X, float FromZ) const;
	/** Just over the arena's floor (under its ledges): where floor searches start for the floor itself. */
	float LowZ() const;
	void FireShot(const FOnyxBossStep& Step, int32 Index);
	void SummonMinions(const FOnyxBossStep& Step);
	void KillSummons();
	float FacingSign() const;
	void LogState(const TCHAR* What) const;

	TWeakObjectPtr<AOnyxCharacter> Character;
	TWeakObjectPtr<UOnyxMovementComponent> Movement;
	TArray<TWeakObjectPtr<AActor>> Summoned;
	TArray<float> Cooldowns;
	FBox Arena = FBox(ForceInit);
	FVector StartLocation = FVector::ZeroVector;
	EState State = EState::Waiting;
	int32 Phase = 0;
	int32 CurrentMove = INDEX_NONE;
	int32 LastMove = INDEX_NONE;
	int32 StepIndex = INDEX_NONE;
	bool bForcedMove = false;
	/** Moves played one straight after another so far. */
	int32 ChainCount = 0;
	bool bFightActive = false;
	bool bDefeated = false;
	bool bHidden = false;
	bool bHitboxOpen = false;
	int32 PendingPhase = INDEX_NONE;
	int32 HitsSinceStagger = 0;
	/** Blows taken since it began its last move or rest (the answer comes at Answer Hits of them while it rests). */
	int32 HitsSinceMove = 0;
	/** Time left before blows count towards a stagger again. */
	float GraceLeft = 0.f;
	float StepTime = 0.f;
	float RestLeft = 0.f;
	float StaggerLeft = 0.f;
	float ShotTimer = 0.f;
	float TrailTimer = 0.f;
	/** Where it was when the step began (how far a rush or a glide took it). */
	FVector StepStart = FVector::ZeroVector;
	/** A rush that stopped going anywhere (something in its way): how long, and where it was last frame. */
	float StuckTime = 0.f;
	float LastX = 0.f;
	int32 ShotsFired = 0;
	float RushDirection = 1.f;
	/** The sweep direction of the last Across Floor volley (they alternate). */
	float SweepDirection = 1.f;
	double FightStartTime = 0.0;
	double DefeatTime = 0.0;
	float BaseRunSpeed = 0.f;
};
