// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Core/Onyx2DTypes.h"
#include "OnyxMovementComponent.generated.h"


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

/** How a character moves in Onyx Water: Jump strokes upward (and jumps out at the surface), Down dives. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxSwimSettings
{
	GENERATED_BODY()

	/** Off: the character ignores water (walks along the bottom, like a heavy knight). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim")
	bool bCanSwim = true;

	/** Left / right speed in water, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim", meta = (ClampMin = "0"))
	float SwimSpeed = 320.f;

	/** How quickly it reaches that speed or stops, cm/s². */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim", meta = (ClampMin = "0"))
	float SwimAcceleration = 1600.f;

	/** Upward speed of a stroke (Jump under water). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim", meta = (ClampMin = "0"))
	float StrokeSpeed = 460.f;

	/** Pull down between strokes, as a part of normal gravity: below 0 floats up to the surface (default), 0 hovers, above 0 sinks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim", meta = (UIMin = "-0.5", UIMax = "1"))
	float SinkGravityScale = -0.08f;

	/** Fastest sinking speed, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim", meta = (ClampMin = "0"))
	float MaxSinkSpeed = 200.f;

	/** Sinking speed while holding Down, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim", meta = (ClampMin = "0"))
	float DiveSpeed = 380.f;

	/** Height of the jump out of the water (Jump at the surface), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim", meta = (ClampMin = "0"))
	float ExitJumpHeight = 200.f;

	/** Part of the falling speed kept when landing in water (the rest goes into the splash). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim", meta = (ClampMin = "0", ClampMax = "1"))
	float EntrySpeedKept = 0.35f;

	/** Bob at the surface with the head out instead of swimming up out of the water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim")
	bool bFloatAtSurface = true;
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

	/** Hold to crouch while on the ground (Down). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Movement")
	void SetCrouchHeld(bool bHeld) { bCrouchHeld = bHeld; }

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
};
