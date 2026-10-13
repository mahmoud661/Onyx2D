// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Core/Onyx2DTypes.h"
#include "OnyxMovementComponent.generated.h"


/** How a character jumps (Onyx Movement > Jump). */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxJumpSettings
{
	GENERATED_BODY()

	/** Peak height of a full (held) jump, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "10"))
	float JumpHeight = 260.f;

	/** Seconds to reach the peak of a full jump. Lower is snappier. Gravity is derived from this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "0.1"))
	float TimeToApex = 0.38f;

	/** 1 = single jump, 2 = double jump, ... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "1"))
	int32 MaxJumps = 2;

	/** Height of air jumps relative to JumpHeight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "0.1"))
	float AirJumpHeightScale = 0.8f;

	/** Grace period after leaving a ledge during which a ground jump is still allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "0"))
	float CoyoteTime = 0.1f;

	/** A jump pressed this early (before landing / while locked) still happens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "0"))
	float JumpBufferTime = 0.12f;

	/** Upward speed is multiplied by this when jump is released early (variable jump height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "0", ClampMax = "1"))
	float JumpCutMultiplier = 0.45f;

	/** Gravity multiplier while falling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "0.1"))
	float FallGravityMultiplier = 1.7f;

	/** Gravity multiplier near the apex while jump is held (hang time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "0.1"))
	float ApexGravityMultiplier = 0.55f;

	/** |Vertical speed| under which the character counts as at the apex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "0"))
	float ApexSpeedThreshold = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "100"))
	float MaxFallSpeed = 1800.f;
};

/** The dash: a short burst of speed with a cooldown (Onyx Movement > Dash). */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxDashSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash", meta = (ClampMin = "0"))
	float Distance = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash", meta = (ClampMin = "0.01"))
	float Duration = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash", meta = (ClampMin = "0"))
	float Cooldown = 0.45f;

	/** Dashes allowed before touching the ground again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash", meta = (ClampMin = "0"))
	int32 AirDashes = 1;
};

/** Sliding down walls and jumping off them (Onyx Movement > Wall). */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxWallSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	bool bWallSlide = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0"))
	float SlideSpeed = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	bool bWallJump = true;

	/** X = push away from the wall, Y = upward speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	FVector2D WallJumpVelocity = FVector2D(620.f, 950.f);

	/** Seconds of reduced steering after a wall jump so it carries the player away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0"))
	float ControlLockTime = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "1"))
	float ProbeDistance = 12.f;
};

/**
 * How a character moves in Onyx Water. Steer in any direction (Move with Jump for up and Down for down, or the stick),
 * tap Jump for a stroke, Dash for a swim dash, and Jump at the surface to get out. Diving in from higher goes deeper.
 */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxSwimSettings
{
	GENERATED_BODY()

	/** Off: the character ignores water (walks along the bottom, like a heavy knight). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim")
	bool bCanSwim = true;

	// --- Moving ---------------------------------------------------------------------------------

	/** Cruising speed under water, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Moving", meta = (ClampMin = "0"))
	float SwimSpeed = 300.f;

	/** Cruising speed along the surface with the head out (faster: strokes in the air), cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Moving", meta = (ClampMin = "0"))
	float SurfaceSwimSpeed = 380.f;

	/** How quickly it speeds up and turns towards the steering, cm/s². High = responsive, never "swimmy". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Moving", meta = (ClampMin = "0"))
	float SwimAcceleration = 1500.f;

	/** How quickly it slows down after letting go, and how quickly the speed of dives and dashes fades, cm/s². */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Moving", meta = (ClampMin = "0"))
	float SwimBraking = 650.f;

	/** Drift without up / down steering, cm/s: above 0 floats up to the surface, 0 hovers, below 0 sinks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Moving")
	float Buoyancy = 110.f;

	// --- Strokes and swim dash --------------------------------------------------------------------

	/** Speed of a stroke (tap Jump under water), along the steering (up when not steering), cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Strokes", meta = (ClampMin = "0"))
	float StrokeSpeed = 560.f;

	/** Shortest time between two strokes, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Strokes", meta = (ClampMin = "0", Units = "s"))
	float StrokeInterval = 0.3f;

	/** Speed of the swim dash (Dash under water), in any direction, cm/s. Needs Dash enabled; uses its cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Strokes", meta = (ClampMin = "0"))
	float SwimDashSpeed = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Strokes", meta = (ClampMin = "0.01", Units = "s"))
	float SwimDashTime = 0.22f;

	// --- Diving in -----------------------------------------------------------------------------

	/** Part of the falling speed kept on hitting the water (the rest goes into the splash): from higher, deeper. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Diving In", meta = (ClampMin = "0", ClampMax = "1"))
	float EntrySpeedKept = 0.5f;

	/** Extra part kept when diving in holding Down (head first). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Diving In", meta = (ClampMin = "0", ClampMax = "1"))
	float HeadFirstBonus = 0.25f;

	/** Part of the sideways speed kept on hitting the water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Diving In", meta = (ClampMin = "0", ClampMax = "1"))
	float EntrySideSpeedKept = 0.6f;

	/** How quickly the water uses up the speed of a dive, per second (higher = shallower dives). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Diving In", meta = (ClampMin = "0"))
	float DiveDrag = 4.f;

	/** Seconds a dive carries on before floating and up / down steering take over. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Diving In", meta = (ClampMin = "0", Units = "s"))
	float DiveTime = 0.35f;

	// --- Surface and getting out -------------------------------------------------------------------

	/** Bob at the surface with the head out instead of swimming up out of the water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Surface")
	bool bFloatAtSurface = true;

	/** Height of the jump out of the water (Jump at the surface), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Surface", meta = (ClampMin = "0"))
	float ExitJumpHeight = 200.f;

	/** Rising at least this fast through the surface leaves the water (a breach); slower stays afloat, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Surface", meta = (ClampMin = "0"))
	float BreachSpeed = 480.f;

	/** The breach's speed is multiplied by this as the water lets go (swim dash up from deep: a dolphin leap). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Surface", meta = (ClampMin = "1", UIMax = "3"))
	float BreachBoost = 1.6f;

	/** Swimming into a bank at the surface climbs onto it when its top is at most this high, cm (0 = never). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Surface", meta = (ClampMin = "0"))
	float ClimbOutHeight = 150.f;

	// --- Breath --------------------------------------------------------------------------------

	/** Breath runs out with the head under water (refills at the surface); then the character takes damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Breath")
	bool bLimitedBreath = false;

	/** Seconds of breath under water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Breath", meta = (ClampMin = "1", Units = "s", EditCondition = "bLimitedBreath"))
	float BreathTime = 20.f;

	/** Seconds to fill up again from empty at the surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Breath", meta = (ClampMin = "0.1", Units = "s", EditCondition = "bLimitedBreath"))
	float BreathRefillTime = 1.2f;

	/** Damage every second once out of breath (gradual, so there is time to reach air). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Breath", meta = (ClampMin = "0", EditCondition = "bLimitedBreath"))
	float DrownDamage = 1.f;

	// --- Look ----------------------------------------------------------------------------------

	/** The sprite leans into the swimming direction, degrees (head down in a dive). 0 for pixel art: turned pixels look jagged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Look", meta = (ClampMin = "0", ClampMax = "80"))
	float MaxLean = 0.f;

	/**
	 * Effects from the character: bubbles when diving in and breathing, a pulse ring with each stroke and dash, sand stirred
	 * up from the bottom. (The water adds the splash, foam and the wake of anything moving through it.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim|Look")
	bool bSwimEffects = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnyxMovementSignature);

/**
 * Platformer movement on the XZ plane.
 * Commands (JumpPressed, TryDash, ...) are input-agnostic so players, AI and cutscenes can drive it.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent))
class ONYX2DRUNTIME_API UOnyxMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UOnyxMovementComponent();

	enum ECustomMode : uint8
	{
		CMOVE_OnyxDash = 0,
		CMOVE_OnyxSwim = 1
	};

	// ------------------------------------------------------------- Tuning
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Ground", meta = (ClampMin = "0"))
	float RunSpeed = 520.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Ground", meta = (ClampMin = "0"))
	float GroundAcceleration = 4200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Ground", meta = (ClampMin = "0"))
	float GroundBraking = 4600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Air", meta = (ClampMin = "0", ClampMax = "1"))
	float AirSteering = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Jump")
	FOnyxJumpSettings JumpSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Dash")
	FOnyxDashSettings DashSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Wall")
	FOnyxWallSettings WallSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Swim")
	FOnyxSwimSettings SwimSettings;

	// ------------------------------------------------------------- Commands
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void JumpPressed();

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void JumpReleased();

	/** Dashes horizontally (Direction sign only). Returns false when on cooldown or out of air dashes. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	bool TryDash(float Direction);

	/** Hold to crouch while on the ground (Down). Under water it steers down. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void SetCrouchHeld(bool bHeld) { bCrouchHeld = bHeld; }

	/** Up / down steering from -1 to 1 (the stick): the swimming direction. Keys steer through Jump (up) and Down. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void SetVerticalInput(float Value) { VerticalInput = FMath::Clamp(Value, -1.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsCrouchingDown() const { return bCrouchHeld && IsMovingOnGround() && !bDashing; }

	/** Speed while crouching, as a fraction of the run speed (0 = stand still). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch", meta = (ClampMin = "0", ClampMax = "1"))
	float CrouchSpeedScale = 0.f;

	/** Falls through the one-way platform currently stood on. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	bool TryDropThroughPlatform();

	/** While locked the character ignores steering and buffers jumps (used by attacks, dialogue...). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void SetMovementLocked(bool bLocked) { bMovementLocked = bLocked; }

	// ------------------------------------------------------------- Queries
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	EOnyxLocomotion GetLocomotionState() const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsDashing() const { return bDashing; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsWallSliding() const { return bWallSliding; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsMovementLocked() const { return bMovementLocked; }

	/** True while swimming in Onyx Water. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsSwimmingInWater() const { return MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_OnyxSwim; }

	/** The water the character is in (null on land). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	class AOnyxWater* GetWater() const { return Water.Get(); }

	/** Swimming steering: X right, Y up, each from -1 to 1 (zero when not swimming). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Swim")
	FVector2D GetSwimSteering() const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Swim")
	bool IsSwimDashing() const { return SwimDashTimer > 0.f; }

	/** Breath left, 1 = full (always full without Limited Breath). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Swim")
	float GetBreath() const { return Breath; }

	/** How far the sprite leans into the swimming direction now, degrees (see Max Lean). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Swim")
	float GetSwimLean() const { return SwimLean; }

	/** Called by Onyx Water while the character overlaps it / when it leaves. */
	void SetWater(class AOnyxWater* InWater);
	void ClearWater(const class AOnyxWater* InWater);

	/** 0 = ready, 1 = just used. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	float GetDashCooldownFraction() const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	float GetFullJumpVelocity() const;

	// ------------------------------------------------------------- Events
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnJumped;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnAirJumped;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnWallJumped;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnDashed;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnLandedEvent;

	/** Started swimming (fell or walked into deep enough water). */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnStartedSwimming;

	/** Left the water (jumped or climbed out). */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnStoppedSwimming;

	/** A swim stroke (Jump under water). */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnSwimStroke;

	/** A swim dash (Dash under water). */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnSwimDash;

	/** Shot out of the water by rising fast through the surface. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnBreached;

	/** Climbed out onto a bank. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnClimbedOut;

	/** Breath dropped to a quarter (Limited Breath): time to find air. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnBreathLow;

	/** Out of breath: every second until it reaches air (Onyx characters take Drown Damage). */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnDrowning;

	/** Re-applies RunSpeed/Acceleration/... to the underlying CharacterMovement values. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void ApplyTuning();

	// UCharacterMovementComponent
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual float GetGravityZ() const override;
	virtual float GetMaxSpeed() const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const override;

private:
	float GetBaseGravityZ() const;
	float GetJumpVelocityForHeight(float Height) const;
	void TryConsumeJumpBuffer();
	void PerformJump(float Height, bool bAirJump);
	void PerformWallJump();
	void UpdateWallContact();
	void UpdateOneWayPlatforms();
	bool IsOneWaySurface(const UPrimitiveComponent* Component) const;
	void PhysDash(float DeltaTime, int32 Iterations);
	void PhysSwim(float DeltaTime, int32 Iterations);
	/** Starts / stops swimming as the capsule goes under / above the surface. */
	void UpdateSwimming();
	/** Jump while swimming: a stroke, or a jump out at the surface. */
	void SwimJump();
	/** Dash while swimming: a burst in the steering direction (Fallback: the side to go when not steering). */
	bool SwimDash(float Fallback);
	/** Leaves the water upwards at Speed (jumping out, breaching, climbing out). */
	void LeaveWater(float UpSpeed, float SideSpeed);
	/** Climbs onto the bank on side WallSide (+1 right) when its top is within reach. */
	bool TryClimbOut(float WallSide);
	/** Breath under water and the bubbles of breathing. */
	void UpdateBreath(float DeltaTime);
	void EmitBubbles(const FVector& Location, int32 Count, float Spread) const;
	/** A pulse ring through the water at Location (strokes, dashes). */
	void EmitRing(const FVector& Location, float Radius, float Strength) const;
	/** The speed of one axis eased towards Target: speeding up / turning at Swim Acceleration, slowing at Swim Braking. */
	float ApproachSwimSpeed(float Speed, float Target, float DeltaTime) const;
	float GetCapsuleHalfHeight() const;
	void EndDash();
	void HandleLanded();

	float JumpBufferTimer = 0.f;
	float CoyoteTimer = 0.f;
	float DashTimer = 0.f;
	float DashCooldownTimer = 0.f;
	float ControlLockTimer = 0.f;
	float DropThroughTimer = 0.f;
	float DashDirection = 1.f;
	int32 JumpsUsed = 0;
	int32 AirDashesUsed = 0;
	int32 WallDirection = 0;
	bool bJumpHeld = false;
	bool bJumpActive = false;
	bool bDashing = false;
	bool bCrouchHeld = false;
	bool bWallSliding = false;
	bool bMovementLocked = false;

	TWeakObjectPtr<UPrimitiveComponent> DropThroughSurface;
	TWeakObjectPtr<class AOnyxWater> Water;
	/** After jumping out of the water: don't start swimming again straight away. */
	float WaterExitTimer = 0.f;
	float VerticalInput = 0.f;
	/** Time left of a dive's carry, of a swim dash, before the next stroke. */
	float DiveTimer = 0.f;
	float SwimDashTimer = 0.f;
	float StrokeTimer = 0.f;
	FVector2D SwimDashDirection = FVector2D(1.f, 0.f);
	float Breath = 1.f;
	float DrownTimer = 0.f;
	float BreathBubbleTimer = 0.f;
	float SwimLean = 0.f;
};
