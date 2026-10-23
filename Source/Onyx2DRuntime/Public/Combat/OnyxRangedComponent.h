// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/OnyxProjectile.h"
#include "OnyxRangedComponent.generated.h"

class UPaperFlipbook;
class UPaperFlipbookComponent;
class USoundBase;
struct FOnyxDamageInfo;

/** A shot a character can take: a bow drawn and loosed, a bomb or a stone thrown. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxRangedMove
{
	GENERATED_BODY()

	/** Played when it starts: the bow drawn, the arm cocked back (or, with no Hold or Release Clip, the whole throw). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	FName StartClip;

	/** Played again and again while the button stays held after Start Clip (the bow held drawn, charging); none: it lets go then. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	FName HoldClip;

	/** Played as it lets go (the string loosed, the arm thrown); none: none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	FName ReleaseClip;

	/**
	 * The aims its clips are drawn at, degrees up from level (-60, -30, 0, 30, 60, 90): beside each level clip, <Clip>_Up30,
	 * <Clip>_Down30... The one nearest the aim plays, and the aim is kept between the lowest and the highest. Empty: the
	 * level clips, whatever the aim.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Aim")
	TArray<float> AimAngles;

	/**
	 * Its clips draw the upper body alone (head, torso, arms, what is held), laid over the legs of whatever the character is
	 * doing: the movement clips drawn as legs alone (<Role>_Legs: Idle_Legs, Run_Legs, Fall_Legs...), the clip's "UpperBody"
	 * socket on theirs. The legs stand, run, jump and swim by themselves while it aims, and with Aim Angles a few degrees apart
	 * from straight down to straight up (it turns to face the aim) it follows the aim all the way round. A "Shot" socket on its
	 * frames is where the shot leaves. Side view; top down the clips play whole.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Aim")
	bool bUpperBody = false;

	/** What flies (its Speed is the full-power speed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	FOnyxProjectileSpec Projectile;

	/** Seconds into letting go when the shot leaves the hand: into Release Clip, or into Start Clip when it is the whole move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (ClampMin = "0", Units = "s"))
	float FireAfter = 0.f;

	/** Where it leaves from: Y up from the feet (the shoulder), then X out along the aim, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Aim")
	FVector2D Muzzle = FVector2D(40.f, 70.f);

	/** The shot leaves this far above the aim (a lob), degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Aim", meta = (ClampMin = "-89", ClampMax = "89", Units = "deg"))
	float AimAngle = 0.f;

	/** Seconds of holding (from the press) for full power; 0: always full. Held longer, it flies faster and further. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Charge", meta = (ClampMin = "0", Units = "s"))
	float ChargeTime = 0.f;

	/** The share of its speed when let go at once; it grows to all of it over Charge Time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Charge", meta = (ClampMin = "0.05", ClampMax = "1"))
	float MinPower = 0.4f;

	/** Its damage times this when let go at full power. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Charge", meta = (ClampMin = "0"))
	float ChargedDamageScale = 1.f;

	/** Shown and played at the hands the moment it reaches full power. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Charge")
	TObjectPtr<UPaperFlipbook> ChargedEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Charge")
	TObjectPtr<USoundBase> ChargedSound;

	/** Seconds after a shot before the next one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (ClampMin = "0", Units = "s"))
	float Cooldown = 0.4f;

	/** Energy each shot takes (Onyx Energy); 0: free. Without enough, nothing happens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (ClampMin = "0"))
	float EnergyCost = 0.f;

	/**
	 * How fast it can move while it draws, aims and lets go, as a share of its speed (0: it stands its ground; a fall carries on
	 * either way). Moving, it plays the clips drawn for running (<Clip>_Run...); in the air, those drawn rising (_Air) or coming
	 * down (_Fall); in water, those for swimming (_Swim); each at the aim as well (<Clip>_Run_Up30...), else the plain ones.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (ClampMin = "0", ClampMax = "1"))
	float MoveScale = 0.f;

	/**
	 * How fast it backs away from its aim (going one way, aiming the other), as a share of its speed; 0: as Move Scale. With
	 * Upper Body the legs walk backwards (their looping clips played backwards), slow enough for the Walk clip.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (ClampMin = "0", ClampMax = "1"))
	float BackwardMoveScale = 0.f;

	/** Aiming from a crouch it kneels where it is (stand up to move): for a crouch with no legs drawn for walking while aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	bool bKneelWhenCrouched = false;

	bool IsSet() const { return !StartClip.IsNone() || !ReleaseClip.IsNone() || Projectile.Look != nullptr; }
	/** Played start to end with no button to hold (a quick throw), rather than drawn, held and let go (a bow). */
	bool IsWholeMove() const { return HoldClip.IsNone() && ReleaseClip.IsNone(); }
};

/** Which of a character's shots. */
UENUM(BlueprintType)
enum class EOnyxRangedSlot : uint8
{
	/** The Shoot input: a bow, a sling, a wand. */
	Shoot,
	/** The Throw input: a bomb, a stone, a knife. */
	Throw
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnyxRangedFiredSignature, EOnyxRangedSlot, Slot, AOnyxProjectile*, Projectile);

/**
 * Onyx Ranged: a character's shots, on the Shoot and Throw inputs. A move with a Hold Clip is drawn while its button is held and
 * let go when it is released (a bow: Start Clip, Hold Clip for as long as it is held, then Release Clip and the arrow); held
 * longer it charges (Charge Time) and flies faster and further. A move that is only a Start Clip plays through at once and its
 * shot leaves Fire After seconds in.
 *
 * It aims at the mouse cursor (shown as a crosshair), else along the right stick, else up or down with Up / Down, else straight
 * ahead; the character turns to the aim, and with Aim Angles the clips drawn nearest the aim play. Each move can cost energy
 * (Onyx Energy) and waits its Cooldown; a hit, a dash or death breaks it off with nothing fired.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Onyx Ranged"))
class ONYX2DRUNTIME_API UOnyxRangedComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxRangedComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Ranged")
	FOnyxRangedMove Shoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Ranged")
	FOnyxRangedMove Throw;

	/** Aims at the mouse cursor when a player with a mouse plays it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Ranged|Aim")
	bool bAimWithMouse = true;

	/** Shows the mouse cursor as a crosshair while it plays (with Aim With Mouse). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Ranged|Aim")
	bool bShowCrosshair = true;

	/** Its button went down: starts the move if it can (not busy, ready again, enough energy). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Ranged")
	bool Press(EOnyxRangedSlot Slot);

	/** Its button came up: a drawn shot is let go (one still being drawn goes as soon as it is drawn). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Ranged")
	void Release(EOnyxRangedSlot Slot);

	/** Breaks off whatever it is doing, nothing fired. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Ranged")
	void Cancel();

	/** The right stick (X right, Y up); small deflections are ignored. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Ranged|Aim")
	void SetStickAim(FVector2D Stick) { StickAim = Stick; }

	/** Aims this many degrees up from level, ahead, whatever the mouse or the stick say (tests, cutscenes); clear to stop. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Ranged|Aim")
	void SetAimOverride(float Degrees) { bAimOverride = true; AimOverride = Degrees; }

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Ranged|Aim")
	void ClearAimOverride() { bAimOverride = false; }

	/** Where it aims now: a unit vector (side view: X along the level, Y up; top down: X across, Y up the screen). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Ranged|Aim")
	FVector2D GetAimDirection() const;

	/** 0 to 1: how charged the move being held is (0 when nothing is held). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Ranged")
	float GetCharge() const;

	/** Drawing, aiming, throwing or letting go. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Ranged")
	bool IsBusy() const { return State != EState::Idle; }

	/** The upper body clip showing over the legs (Upper Body moves), or None. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Ranged")
	FName GetUpperClip() const;

	/** A shot left the hand. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Ranged")
	FOnyxRangedFiredSignature OnFired;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	enum class EState : uint8
	{
		Idle,
		Starting,
		Holding,
		LettingGo
	};

	const FOnyxRangedMove& MoveOf(EOnyxRangedSlot Slot) const { return Slot == EOnyxRangedSlot::Shoot ? Shoot : Throw; }
	/** Degrees up from level, ahead of the character, kept within the move's Aim Angles. */
	float AimDegrees(const FOnyxRangedMove& Move) const;
	/** The angle of Aim Angles nearest Degrees (0 without any). */
	static float NearestDrawn(const FOnyxRangedMove& Move, float Degrees);
	/** Clip drawn for what it is doing and at Angle (<Clip>_Run_Up30, <Clip>_Run, <Clip>_Up30...), or Clip itself when there is none. */
	FName ClipAt(FName Clip, float Angle) const;
	/** What it is doing, as clips name it: running (_Run), rising (_Air), coming down (_Fall), swimming (_Swim), or standing (none). */
	const TCHAR* StateSuffix() const;
	/** Plays Clip: on the body, or for an Upper Body move over the legs (from the start, or on from the frame showing). */
	float PlayClip(FName Clip, bool bFromStart = true);
	bool UsesUpperBody(const FOnyxRangedMove& Move) const;
	/** The layer the upper body clips play on, attached to the body's "UpperBody" socket (made the first time). */
	UPaperFlipbookComponent* GetUpperLayer();
	/** Keeps the upper body on the legs: shown only over legs, its anchor on theirs, lit and flashing as the body is. */
	void UpdateUpper();
	void HideUpper();
	/** Where the shot leaves, as drawn on the upper body (its "Shot" socket), in the world. */
	bool DrawnShotPoint(const FOnyxRangedMove& Move, FVector& OutPoint) const;
	void TurnToAim();
	void ShowCrosshair();
	void LetGo();
	void FireNow();
	void Finish();
	double Now() const;
	bool MousePoint(FVector& OutPoint) const;
	FVector ShoulderLocation(const FOnyxRangedMove& Move) const;

	UFUNCTION()
	void HandleDamaged(const FOnyxDamageInfo& Damage);

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbookComponent> UpperLayer;
	FName UpperClip;

	EState State = EState::Idle;
	EOnyxRangedSlot Active = EOnyxRangedSlot::Shoot;
	bool bHeld = false;
	bool bFired = false;
	bool bLocked = false;
	bool bCharged = false;
	bool bAimOverride = false;
	float AimOverride = 0.f;
	/** The drawn angle and the state (running, in the air...) the playing clip shows. */
	float ShownAngle = 0.f;
	const TCHAR* ShownState = TEXT("");
	bool bSlowed = false;
	bool bBackingAway = false;
	/** The speed share it moves at now (Move Scale, backing away, or kneeling). */
	float AppliedScale = 1.f;
	FVector2D StickAim = FVector2D::ZeroVector;
	double PressedAt = 0.0;
	double PhaseEnds = 0.0;
	double FireAt = -1.0;
	float FirePower = 1.f;
	double ReadyAt[2] = { 0.0, 0.0 };
};
