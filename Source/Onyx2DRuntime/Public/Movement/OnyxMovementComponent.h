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

	/** A wall jump always rises its full height: letting go of Jump doesn't cut it short (climbing a shaft wall to wall never depends on how long the button is held). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	bool bFullHeightWallJump = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "1"))
	float ProbeDistance = 12.f;
};

/** Sliding: Down while running drops into a low slide that carries on and slows down (Onyx Movement > Slide). */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxSlideSettings
{
	GENERATED_BODY()

	/** Off by default. Needs Crouch Height Scale below 1 to fit under things. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide")
	bool bEnabled = false;

	/** Starts only when running at least this fast (fraction of the run speed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bEnabled"))
	float MinSpeedFraction = 0.6f;

	/** Speed at the start, as a fraction of the run speed... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0", EditCondition = "bEnabled"))
	float StartSpeedFraction = 1.35f;

	/** ...and at the end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0", EditCondition = "bEnabled"))
	float EndSpeedFraction = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0.05", Units = "s", EditCondition = "bEnabled"))
	float Duration = 0.5f;
};

/**
 * Slopes: ramps of slope tiles (Onyx Tile Collision) or any sloped ground. How steep it walks, sliding down wet slopes
 * (water running down them: whatever calls Apply Surface Flow), and its feet drawn on the slope (Onyx Movement > Slopes).
 */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxSlopeSettings
{
	GENERATED_BODY()

	/** The steepest ground it walks on, degrees (45 degree ramps need more than 45); steeper ground slides it off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slopes", meta = (ClampMin = "0", ClampMax = "89", Units = "deg"))
	float MaxWalkAngle = 50.f;

	/**
	 * Its feet drawn on the slope right under its middle. The capsule rests on a slope beside its middle, so without this
	 * the feet float a little above it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slopes")
	bool bFeetOnSlope = true;

	/** On a wet slope (Apply Surface Flow with Wet) it slides: how quickly it speeds up down the slope, cm/s². */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slopes", meta = (ClampMin = "0"))
	float WetSlideAcceleration = 2400.f;

	/** The fastest it slides, cm/s (sideways; along a 45 degree slope that is 1.4 times as far). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slopes", meta = (ClampMin = "0"))
	float MaxSlideSpeed = 900.f;

	/** How much it can steer while sliding (0: not at all, 1: as on dry ground). Below about 0.5 it can't climb back up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slopes", meta = (ClampMin = "0", ClampMax = "1"))
	float SlideSteering = 0.25f;

	/** Dry ground at least this steep slides it too, degrees (0: only wet slopes do). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slopes", meta = (ClampMin = "0", ClampMax = "89", Units = "deg"))
	float DrySlideAngle = 0.f;
};

/**
 * Ledges: falling past the top of a wall while holding towards it, the character catches the edge and hangs. Jump or Up
 * climbs onto it, Down or away lets go (Onyx Movement > Ledge).
 */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxLedgeSettings
{
	GENERATED_BODY()

	/** Off by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge")
	bool bEnabled = false;

	/** How far above or below the top of the head an edge can be and still be caught, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge", meta = (ClampMin = "1", EditCondition = "bEnabled"))
	float GrabReach = 28.f;

	/** While hanging, the top of the head is this far under the edge, cm (hands over it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge", meta = (EditCondition = "bEnabled"))
	float HangDepth = 8.f;

	/** How long pulling up takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge", meta = (ClampMin = "0.05", Units = "s", EditCondition = "bEnabled"))
	float ClimbTime = 0.4f;
};

/**
 * How a character moves in Onyx Water. Steer in any direction (Move with Jump for up and Down for down, or the stick),
 * tap Jump for a stroke, Dash for a swim dash, and Jump at the surface to get out. Diving in from higher goes deeper.
 */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxSwimSettings
{
	GENERATED_BODY()

	/** Off: the character ignores water (walks along the bottom, like a heavy knight). From above it wades but stops where it gets deep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim")
	bool bCanSwim = true;

	/** From above (Top Down levels): its speed wading through the shallows, part of its run speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim", meta = (ClampMin = "0.05", ClampMax = "1"))
	float WadeSpeedScale = 0.6f;

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
 * 2D movement on the XZ plane: a platformer seen from the side (gravity, jumps, walls, ledges, swimming), or walking in
 * any direction seen from above (Top Down game style: up the screen is north, no gravity).
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
		CMOVE_OnyxSwim = 1,
		CMOVE_OnyxLedge = 2,
		/** Walking over the ground seen from above. */
		CMOVE_OnyxTopDown = 3
	};

	// ------------------------------------------------------------- Game style
	/** Moves the way its level is played (side view or top down). Off: the Game Style below, whatever the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Game Style")
	bool bUseLevelGameStyle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Game Style", meta = (EditCondition = "!bUseLevelGameStyle"))
	EOnyxGameStyle GameStyle = EOnyxGameStyle::SideView;

	/**
	 * Top down: how much ground the character stands on, a circle at its feet this many cm across the middle (0: as wide as
	 * its capsule). Walls stop the feet and blows land on them; the picture above them can overlap a wall behind.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Game Style", meta = (ClampMin = "0", Units = "cm"))
	float FootprintRadius = 0.f;

	/** Seen from above: walking in any direction, no gravity or jumping (the Top Down game style). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsTopDown() const { return bTopDown; }

	/** The style it moves in: its level's, or its own Game Style. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	EOnyxGameStyle GetActiveGameStyle() const;

	/** Moves in this style from now on (top down: its capsule becomes a footprint at its feet; side view: back). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void SetGameStyle(EOnyxGameStyle Style);

	/** Top down: where it is steered, X right and Y up the screen (north), at most 1 long (zero while it can't move). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	FVector2D GetTopDownSteering() const;

	/** The capsule standing up (radius, half height): from above, the one it had before it became a footprint at the feet. */
	FVector2D GetStandingCapsule() const;

	/** Top down: dashes in a direction (X right, Y up the screen). Returns false when on cooldown. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	bool TryDashToward(FVector2D Direction);

	// ------------------------------------------------------------- Tuning
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Ground", meta = (ClampMin = "0"))
	float RunSpeed = 520.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Ground", meta = (ClampMin = "0"))
	float GroundAcceleration = 4200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Ground", meta = (ClampMin = "0"))
	float GroundBraking = 4600.f;

	/** Below this fraction of the run speed it walks (the Walk clip) instead of running. 0 = always runs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Ground", meta = (ClampMin = "0", ClampMax = "1"))
	float WalkSpeedFraction = 0.f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Slide")
	FOnyxSlideSettings SlideSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Ledge")
	FOnyxLedgeSettings LedgeSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Slopes")
	FOnyxSlopeSettings SlopeSettings;

	// ------------------------------------------------------------- Commands
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void JumpPressed();

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void JumpReleased();

	/** Dashes horizontally (Direction sign only; top down: where it is steered, else that way). Returns false when on cooldown or out of air dashes. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	bool TryDash(float Direction);

	/** Hold to crouch while on the ground (Down). Under water it steers down. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void SetCrouchHeld(bool bHeld) { bCrouchHeld = bHeld; }

	/** Hold to aim up (up-slashes, climbing onto a ledge). The stick pushed up counts too. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void SetUpHeld(bool bHeld) { bUpHeld = bHeld; }

	/** Up / down steering from -1 to 1 (the stick): the swimming direction. Keys steer through Jump (up) and Down. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void SetVerticalInput(float Value) { VerticalInput = FMath::Clamp(Value, -1.f, 1.f); }

	/** Crouching: Down held on the ground, or kept low by a ceiling after a slide. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsCrouchingDown() const { return ((bCrouchHeld && IsMovingOnGround()) || (IsCrouching() && IsMovingOnGround())) && !bDashing && !bSliding; }

	/** Speed while crouching, as a fraction of the run speed (0 = stand still). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch", meta = (ClampMin = "0", ClampMax = "1"))
	float CrouchSpeedScale = 0.f;

	/** Crouching and sliding make the capsule this much of its height (fit under low ceilings). 1 = it stays tall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch", meta = (ClampMin = "0.2", ClampMax = "1"))
	float CrouchHeightScale = 1.f;

	/** Falls through the one-way platform currently stood on. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	bool TryDropThroughPlatform();

	/** While locked the character ignores steering and buffers jumps (used by attacks, dialogue...). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void SetMovementLocked(bool bLocked) { bMovementLocked = bLocked; }

	/** Its speed times this (aiming a bow, carrying something heavy): walking, running, swimming, crouching. 1: as tuned. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void SetSpeedScale(float Scale) { SpeedScale = FMath::Max(Scale, 0.f); }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	float GetSpeedScale() const { return SpeedScale; }

	// ------------------------------------------------------------- Queries
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	EOnyxLocomotion GetLocomotionState() const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsDashing() const { return bDashing; }

	/** A dash that started on the ground (a roll). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsRolling() const { return bDashing && bDashFromGround; }

	/** It came out of the water a moment ago (a jump or a breach out of it). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool HasJustLeftWater() const { return WaterExitTimer > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsSliding() const { return bSliding; }

	/** Sliding down a wet (or too steep) slope. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsSlopeSliding() const { return bSlopeSliding; }

	/** How steep the ground under it is, degrees (0 when flat or in the air). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	float GetGroundSlope() const { return GroundSlope; }

	/** How far its picture is lowered to stand on the slope under its middle, cm (0 or less; Slopes > Feet On Slope). */
	float GetSlopeFootOffset() const { return SlopeFootOffset; }

	/**
	 * Running water it stands or wades in (call this every frame it is in it): Push carries it along, cm/s² (X right,
	 * Y up); Wet makes slopes slide it. Lasts a moment after the last call.
	 */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void ApplySurfaceFlow(FVector2D Push, bool bWet);

	/** Hanging from a ledge or pulling up onto it. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsOnLedge() const { return bLedgeHanging || bLedgeClimbing; }

	/** The side of the ledge it holds: +1 right, -1 left. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	float GetLedgeDirection() const { return LedgeDirection; }

	/** Down is held (crouching on the ground, aiming down in the air), or the stick pushed down. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsDownHeld() const { return bCrouchHeld || VerticalInput < -0.5f; }

	/** Up is held (aiming up), or the stick pushed up. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsUpHeld() const { return bUpHeld || VerticalInput > 0.5f; }

	/** Ends a dash under way at once (an attack out of a dash takes over its speed). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void CancelDash();

	/** Bounced off something in the air (a down-slash on a foe): one jump and the air dashes are back. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void RefreshAirMoves() { JumpsUsed = FMath::Min(JumpsUsed, 1); AirDashesUsed = 0; }

	/** How fast it was falling when it last landed, cm/s. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	float GetLandingSpeed() const { return LandingSpeed; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsWallSliding() const { return bWallSliding; }

	/** The side of the wall it is against: +1 right, -1 left, 0 none. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	float GetWallDirection() const { return float(WallDirection); }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsMovementLocked() const { return bMovementLocked; }

	/** True while swimming in Onyx Water (from above: out beyond the shallows). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsSwimmingInWater() const { return (MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_OnyxSwim) || bSwimmingFromAbove; }

	/** Swimming under the water, the head below the surface (not bobbing at the surface with it out). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsUnderWater() const;

	/** From above: walking through the shallows of Onyx Water. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Movement")
	bool IsWading() const { return bWading; }

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

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnSlideStarted;

	/** Caught a ledge. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnLedgeGrabbed;

	/** Finished pulling up onto a ledge. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Movement")
	FOnyxMovementSignature OnLedgeClimbed;

	/** Re-applies RunSpeed/Acceleration/... to the underlying CharacterMovement values. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void ApplyTuning();

	// UCharacterMovementComponent
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual float GetGravityZ() const override;
	virtual float GetMaxSpeed() const override;
	virtual void SetDefaultMovementMode() override;
	virtual bool HandlePendingLaunch() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;

private:
	/** Slope steepness, sliding down wet slopes, running water's push, the feet's offset onto the slope. */
	void UpdateSlopes(float DeltaTime);
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
	/** Crouching shrinks the capsule (Crouch Height Scale), sliding too. */
	void UpdateCrouch();
	void StartSlide();
	void EndSlide();
	/** Catches a ledge on side Direction (+1 right) when one is in reach. */
	bool TryGrabLedge(float Direction);
	void StartLedgeClimb();
	void ReleaseLedge();
	void PhysLedge(float DeltaTime, int32 Iterations);
	/** Top down: speeding up towards the steering, braking when let go, sliding along walls. */
	void PhysTopDown(float DeltaTime, int32 Iterations);
	/** Moves by Delta, sliding along what it meets (dashes and top-down walking). */
	void MoveAndSlide(const FVector& Delta, float DeltaTime);
	/** Puts the movement in Style: the capsule shaped for it (feet kept where they are) and its movement mode. */
	void ApplyGameStyle(EOnyxGameStyle Style);

	bool bTopDown = false;
	/** From above: wading through the shallows, swimming beyond them. */
	bool bWading = false;
	bool bSwimmingFromAbove = false;
	/** The capsule as it was before it became a top-down footprint (radius, half height). */
	FVector2D SideCapsule = FVector2D::ZeroVector;
	/** Top down: the dash's direction. */
	FVector2D DashVector = FVector2D(1.f, 0.f);

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
	bool bUpHeld = false;
	float SpeedScale = 1.f;
	bool bWallSliding = false;
	bool bMovementLocked = false;
	bool bDashFromGround = false;
	bool bCrouchHeldBefore = false;

	bool bSliding = false;
	float SlideTimer = 0.f;
	float SlideDirection = 1.f;
	float SlideStartSpeed = 0.f;

	bool bLedgeHanging = false;
	bool bLedgeClimbing = false;
	float LedgeDirection = 1.f;
	float LedgeTimer = 0.f;
	/** After letting go: no grabbing for a moment. */
	float LedgeRegrabTimer = 0.f;
	FVector LedgeHangLocation = FVector::ZeroVector;
	FVector LedgeStandLocation = FVector::ZeroVector;

	float FallingSpeed = 0.f;
	float LandingSpeed = 0.f;

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

	bool bSlopeSliding = false;
	float GroundSlope = 0.f;
	float SlopeFootOffset = 0.f;
	FVector2D SurfacePush = FVector2D::ZeroVector;
	bool bSurfaceWet = false;
	float SurfaceFlowTimer = 0.f;
};
