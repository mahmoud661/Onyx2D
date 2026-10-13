// Copyright Onyx2D. All Rights Reserved.

#include "Movement/OnyxMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/OnyxWorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "World/OnyxWater.h"

namespace
{
	constexpr float OneWayPlatformRange = 400.f;
	constexpr float OneWayPlatformTolerance = 4.f;
	constexpr float DropThroughDuration = 0.3f;
	constexpr float WallNormalMaxZ = 0.35f;
	constexpr float WallJumpSteeringScale = 0.15f;
	constexpr float RunThreshold = 15.f;
	/** Swimming starts when the capsule centre is this far (x half height) under the surface... */
	constexpr float SwimStartDepth = 0.15f;
	/** ...and stops when it is this far above it. */
	constexpr float SwimStopHeight = 0.6f;
	/** At the surface the centre floats this far (x half height) under it: head out of the water. */
	constexpr float FloatDepth = 0.35f;
	constexpr float WaterExitGrace = 0.3f;
	/** Within this much (x half height) under the floating height counts as swimming at the surface. */
	constexpr float SurfaceBand = 0.35f;
	/** The head (breathing, breath bubbles) is this far (x half height) above the capsule centre. */
	constexpr float HeadHeight = 0.7f;
	/** Below this speed a swimmer doesn't lean. */
	constexpr float LeanMinSpeed = 60.f;
	constexpr float BreathBubbleInterval = 2.4f;
	/** Breath at which On Breath Low fires. */
	constexpr float LowBreath = 0.25f;
}

UOnyxMovementComponent::UOnyxMovementComponent()
{
	bConstrainToPlane = true;
	SetPlaneConstraintNormal(FVector(0.f, 1.f, 0.f));
	bOrientRotationToMovement = false;
	bUseFlatBaseForFloorChecks = true;
	ApplyTuning();
}

void UOnyxMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplyTuning();
}

void UOnyxMovementComponent::ApplyTuning()
{
	MaxWalkSpeed = RunSpeed;
	MaxAcceleration = GroundAcceleration;
	BrakingDecelerationWalking = GroundBraking;
	GroundFriction = 8.f;
	AirControl = AirSteering;
	AirControlBoostMultiplier = 1.f;
	BrakingDecelerationFalling = GroundBraking * 0.35f;
	FallingLateralFriction = 0.f;
	GravityScale = 1.f; // gravity comes from JumpSettings, see GetGravityZ()
	JumpZVelocity = GetFullJumpVelocity();
}

#if WITH_EDITOR
void UOnyxMovementComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyTuning();
}
#endif

// ---------------------------------------------------------------------------
// Gravity / speed
// ---------------------------------------------------------------------------

float UOnyxMovementComponent::GetBaseGravityZ() const
{
	const float Height = JumpSettings.JumpHeight;
	const float Time = FMath::Max(JumpSettings.TimeToApex, 0.05f);
	return -2.f * Height / (Time * Time);
}

float UOnyxMovementComponent::GetJumpVelocityForHeight(float Height) const
{
	return FMath::Sqrt(2.f * FMath::Abs(GetBaseGravityZ()) * FMath::Max(Height, 0.f));
}

float UOnyxMovementComponent::GetFullJumpVelocity() const
{
	return GetJumpVelocityForHeight(JumpSettings.JumpHeight);
}

float UOnyxMovementComponent::GetGravityZ() const
{
	const float Base = GetBaseGravityZ();
	if (!IsFalling())
	{
		return Base;
	}
	if (bJumpHeld && bJumpActive && FMath::Abs(Velocity.Z) < JumpSettings.ApexSpeedThreshold)
	{
		return Base * JumpSettings.ApexGravityMultiplier;
	}
	if (Velocity.Z < 0.f)
	{
		return Base * JumpSettings.FallGravityMultiplier;
	}
	return Base;
}

float UOnyxMovementComponent::GetMaxSpeed() const
{
	if (bDashing)
	{
		return DashSettings.Distance / FMath::Max(DashSettings.Duration, 0.01f);
	}
	if (IsSwimmingInWater())
	{
		return SwimSettings.SwimSpeed;
	}
	if (IsCrouchingDown())
	{
		return Super::GetMaxSpeed() * CrouchSpeedScale;
	}
	return Super::GetMaxSpeed();
}

FVector UOnyxMovementComponent::ConstrainInputAcceleration(const FVector& InputAcceleration) const
{
	if (bMovementLocked || bDashing)
	{
		return FVector::ZeroVector;
	}
	const FVector Result = Super::ConstrainInputAcceleration(InputAcceleration);
	return ControlLockTimer > 0.f ? Result * WallJumpSteeringScale : Result;
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void UOnyxMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	CoyoteTimer = FMath::Max(CoyoteTimer - DeltaTime, 0.f);
	DashCooldownTimer = FMath::Max(DashCooldownTimer - DeltaTime, 0.f);
	ControlLockTimer = FMath::Max(ControlLockTimer - DeltaTime, 0.f);
	DropThroughTimer = FMath::Max(DropThroughTimer - DeltaTime, 0.f);
	WaterExitTimer = FMath::Max(WaterExitTimer - DeltaTime, 0.f);
	StrokeTimer = FMath::Max(StrokeTimer - DeltaTime, 0.f);
	UpdateSwimming();
	UpdateBreath(DeltaTime);
	if (!IsSwimmingInWater())
	{
		SwimLean = FMath::FInterpTo(SwimLean, 0.f, DeltaTime, 10.f);
	}
	if (DropThroughTimer <= 0.f)
	{
		DropThroughSurface.Reset();
	}

	UpdateOneWayPlatforms();
	UpdateWallContact();
	TryConsumeJumpBuffer();
	JumpBufferTimer = FMath::Max(JumpBufferTimer - DeltaTime, 0.f);

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsFalling())
	{
		if (bWallSliding)
		{
			Velocity.Z = FMath::Max(Velocity.Z, -WallSettings.SlideSpeed);
		}
		Velocity.Z = FMath::Max(Velocity.Z, -JumpSettings.MaxFallSpeed);
	}
}

// ---------------------------------------------------------------------------
// Jump
// ---------------------------------------------------------------------------

void UOnyxMovementComponent::JumpPressed()
{
	bJumpHeld = true;
	JumpBufferTimer = JumpSettings.JumpBufferTime;
	TryConsumeJumpBuffer();
}

void UOnyxMovementComponent::JumpReleased()
{
	bJumpHeld = false;
	if (bJumpActive && IsFalling() && Velocity.Z > 0.f)
	{
		Velocity.Z *= JumpSettings.JumpCutMultiplier;
	}
	bJumpActive = false;
}

void UOnyxMovementComponent::TryConsumeJumpBuffer()
{
	if (JumpBufferTimer <= 0.f || bMovementLocked || bDashing || !CharacterOwner)
	{
		return;
	}
	if (IsSwimmingInWater())
	{
		SwimJump();
		return;
	}

	if (IsMovingOnGround() || (IsFalling() && CoyoteTimer > 0.f && JumpsUsed == 0))
	{
		PerformJump(JumpSettings.JumpHeight, false);
		return;
	}
	if (!IsFalling())
	{
		return;
	}
	if (WallSettings.bWallJump && WallDirection != 0)
	{
		PerformWallJump();
		return;
	}

	// Walking off a ledge without jumping uses up the ground jump.
	const int32 Used = FMath::Max(JumpsUsed, 1);
	if (Used < JumpSettings.MaxJumps)
	{
		JumpsUsed = Used;
		PerformJump(JumpSettings.JumpHeight * JumpSettings.AirJumpHeightScale, true);
	}
}

void UOnyxMovementComponent::PerformJump(float Height, bool bAirJump)
{
	JumpBufferTimer = 0.f;
	++JumpsUsed;
	bJumpActive = true;

	Velocity.Z = GetJumpVelocityForHeight(Height);
	if (!bJumpHeld)
	{
		Velocity.Z *= JumpSettings.JumpCutMultiplier; // buffered tap: short hop
		bJumpActive = false;
	}
	SetMovementMode(MOVE_Falling);
	CoyoteTimer = 0.f;

	(bAirJump ? OnAirJumped : OnJumped).Broadcast();
}

void UOnyxMovementComponent::PerformWallJump()
{
	JumpBufferTimer = 0.f;
	JumpsUsed = 1;
	bJumpActive = bJumpHeld;
	bWallSliding = false;

	Velocity = FVector(-WallDirection * WallSettings.WallJumpVelocity.X, 0.f, WallSettings.WallJumpVelocity.Y);
	SetMovementMode(MOVE_Falling);
	ControlLockTimer = WallSettings.ControlLockTime;
	WallDirection = 0;

	OnWallJumped.Broadcast();
}

void UOnyxMovementComponent::UpdateWallContact()
{
	WallDirection = 0;
	bWallSliding = false;
	if (!IsFalling() || !CharacterOwner || (!WallSettings.bWallSlide && !WallSettings.bWallJump))
	{
		return;
	}

	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		FMath::Max(Capsule->GetScaledCapsuleRadius() - 1.f, 1.f),
		Capsule->GetScaledCapsuleHalfHeight() * 0.8f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(OnyxWallProbe), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	InitCollisionParams(Params, ResponseParams);

	const float InputSign = FMath::Sign(Acceleration.X);
	const float Directions[2] = { InputSign != 0.f ? InputSign : 1.f, InputSign != 0.f ? -InputSign : -1.f };
	const FVector Start = UpdatedComponent->GetComponentLocation();

	for (const float Direction : Directions)
	{
		FHitResult Hit;
		const FVector End = Start + FVector(Direction * WallSettings.ProbeDistance, 0.f, 0.f);
		if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, UpdatedComponent->GetCollisionObjectType(), Shape, Params, ResponseParams)
			&& FMath::Abs(Hit.ImpactNormal.Z) < WallNormalMaxZ
			&& !IsOneWaySurface(Hit.GetComponent()))
		{
			WallDirection = int32(Direction);
			break;
		}
	}

	bWallSliding = WallSettings.bWallSlide && WallDirection != 0 && Velocity.Z < 0.f && InputSign == float(WallDirection);
}

// ---------------------------------------------------------------------------
// Dash
// ---------------------------------------------------------------------------

bool UOnyxMovementComponent::TryDash(float Direction)
{
	if (!DashSettings.bEnabled || bDashing || bMovementLocked || DashCooldownTimer > 0.f)
	{
		return false;
	}
	if (IsSwimmingInWater())
	{
		return SwimDash(Direction);
	}
	if (IsFalling())
	{
		if (AirDashesUsed >= DashSettings.AirDashes)
		{
			return false;
		}
		++AirDashesUsed;
	}

	DashDirection = Direction >= 0.f ? 1.f : -1.f;
	DashTimer = DashSettings.Duration;
	DashCooldownTimer = DashSettings.Duration + DashSettings.Cooldown;
	bDashing = true;
	bJumpActive = false;
	SetMovementMode(MOVE_Custom, CMOVE_OnyxDash);

	OnDashed.Broadcast();
	return true;
}

void UOnyxMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (CustomMovementMode == CMOVE_OnyxDash)
	{
		PhysDash(DeltaTime, Iterations);
		return;
	}
	if (CustomMovementMode == CMOVE_OnyxSwim)
	{
		PhysSwim(DeltaTime, Iterations);
		return;
	}
	Super::PhysCustom(DeltaTime, Iterations);
}

void UOnyxMovementComponent::PhysDash(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	Velocity = FVector(DashDirection * GetMaxSpeed(), 0.f, 0.f);
	const FVector Delta = Velocity * DeltaTime;

	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}

	DashTimer -= DeltaTime;
	if (DashTimer <= 0.f)
	{
		EndDash();
	}
}

void UOnyxMovementComponent::EndDash()
{
	bDashing = false;
	Velocity = FVector(DashDirection * RunSpeed, 0.f, 0.f);
	SetMovementMode(MOVE_Falling);
}

// ---------------------------------------------------------------------------
// Swimming
// ---------------------------------------------------------------------------

void UOnyxMovementComponent::SetWater(AOnyxWater* InWater)
{
	Water = InWater;
}

void UOnyxMovementComponent::ClearWater(const AOnyxWater* InWater)
{
	if (Water.Get() == InWater)
	{
		Water.Reset();
	}
}

float UOnyxMovementComponent::GetCapsuleHalfHeight() const
{
	return CharacterOwner ? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 50.f;
}

FVector2D UOnyxMovementComponent::GetSwimSteering() const
{
	if (!IsSwimmingInWater() || bMovementLocked)
	{
		return FVector2D::ZeroVector;
	}
	const float Side = MaxAcceleration > 0.f ? FMath::Clamp(Acceleration.X / MaxAcceleration, -1.f, 1.f) : 0.f;
	const float Up = FMath::Clamp(VerticalInput + (bJumpHeld ? 1.f : 0.f) - (bCrouchHeld ? 1.f : 0.f), -1.f, 1.f);
	const FVector2D Steering(Side, Up);
	return Steering.SizeSquared() > 1.f ? Steering.GetSafeNormal() : Steering;
}

float UOnyxMovementComponent::ApproachSwimSpeed(float Speed, float Target, float DeltaTime) const
{
	// Speeding up and turning answer at once; slowing down (letting go, the speed of a dive or a dash) drifts a little.
	const bool bSlowing = FMath::Abs(Target) < FMath::Abs(Speed) && Target * Speed >= 0.f;
	return FMath::FInterpConstantTo(Speed, Target, DeltaTime, bSlowing ? SwimSettings.SwimBraking : SwimSettings.SwimAcceleration);
}

void UOnyxMovementComponent::EmitBubbles(const FVector& Location, int32 Count, float Spread) const
{
	if (AOnyxWater* CurrentWater = Water.Get(); CurrentWater && SwimSettings.bSwimEffects)
	{
		CurrentWater->EmitBubbles(Location, Count, Spread);
	}
}

void UOnyxMovementComponent::EmitRing(const FVector& Location, float Radius, float Strength) const
{
	if (AOnyxWater* CurrentWater = Water.Get(); CurrentWater && SwimSettings.bSwimEffects)
	{
		CurrentWater->EmitRing(Location, Radius, Strength);
	}
}

void UOnyxMovementComponent::UpdateSwimming()
{
	if (!UpdatedComponent || bDashing)
	{
		return;
	}
	const AOnyxWater* CurrentWater = Water.Get();
	const float HalfHeight = GetCapsuleHalfHeight();
	const FVector Location = UpdatedComponent->GetComponentLocation();
	if (IsSwimmingInWater())
	{
		// Out of the water (it drained, moved, or the character climbed out).
		if (!CurrentWater || !SwimSettings.bCanSwim || Location.Z > CurrentWater->GetSurfaceZ() + HalfHeight * SwimStopHeight)
		{
			SetMovementMode(MOVE_Falling);
			OnStoppedSwimming.Broadcast();
		}
		return;
	}
	if (CurrentWater && SwimSettings.bCanSwim && WaterExitTimer <= 0.f && Location.Z < CurrentWater->GetSurfaceZ() - HalfHeight * SwimStartDepth)
	{
		// Diving in: the water takes part of the fall (less head first, holding Down), the rest carries the swimmer
		// down for a moment, so a dive from higher goes deeper. Jumps and air dashes come back.
		const float FallSpeed = FMath::Max(-Velocity.Z, 0.f);
		const float Kept = FMath::Min(SwimSettings.EntrySpeedKept + (bCrouchHeld ? SwimSettings.HeadFirstBonus : 0.f), 1.f);
		Velocity.Z *= Velocity.Z < 0.f ? Kept : SwimSettings.EntrySideSpeedKept;
		Velocity.X *= SwimSettings.EntrySideSpeedKept;
		DiveTimer = SwimSettings.DiveTime;
		JumpsUsed = 0;
		AirDashesUsed = 0;
		bJumpActive = false;
		SetMovementMode(MOVE_Custom, CMOVE_OnyxSwim);
		EmitBubbles(Location - FVector(0.f, 0.f, HalfHeight * 0.5f), FMath::Clamp(FMath::RoundToInt(FallSpeed / 70.f), 3, 18), HalfHeight * 0.6f);
		OnStartedSwimming.Broadcast();
	}
}

void UOnyxMovementComponent::LeaveWater(float UpSpeed, float SideSpeed)
{
	WaterExitTimer = WaterExitGrace;
	SwimDashTimer = 0.f;
	DiveTimer = 0.f;
	JumpsUsed = 1;
	Velocity = FVector(SideSpeed, 0.f, UpSpeed);
	SetMovementMode(MOVE_Falling);
	OnStoppedSwimming.Broadcast();
}

void UOnyxMovementComponent::SwimJump()
{
	JumpBufferTimer = 0.f;
	const AOnyxWater* CurrentWater = Water.Get();
	const float HalfHeight = GetCapsuleHalfHeight();
	const FVector Location = UpdatedComponent->GetComponentLocation();
	if (CurrentWater && Location.Z > CurrentWater->GetSurfaceHeightAt(Location.X) - HalfHeight)
	{
		// At the surface: jump out.
		LeaveWater(GetJumpVelocityForHeight(SwimSettings.ExitJumpHeight), Velocity.X);
		bJumpActive = bJumpHeld;
		OnJumped.Broadcast();
		return;
	}
	if (StrokeTimer > 0.f)
	{
		return;
	}
	// A stroke: a burst where the swimmer steers (Jump itself doesn't count as steering up), up when not steering.
	const float Side = MaxAcceleration > 0.f ? FMath::Clamp(Acceleration.X / MaxAcceleration, -1.f, 1.f) : 0.f;
	FVector2D Direction(Side, FMath::Clamp(VerticalInput - (bCrouchHeld ? 1.f : 0.f), -1.f, 1.f));
	Direction = Direction.IsNearlyZero(0.1f) ? FVector2D(0.f, 1.f) : Direction.GetSafeNormal();
	// Up to Stroke Speed in that direction, never slower than before.
	const float Along = Velocity.X * Direction.X + Velocity.Z * Direction.Y;
	const float Boost = FMath::Max(SwimSettings.StrokeSpeed - Along, 0.f);
	Velocity.X += Direction.X * Boost;
	Velocity.Z += Direction.Y * Boost;
	StrokeTimer = SwimSettings.StrokeInterval;
	DiveTimer = 0.f;
	const FVector Behind = Location - FVector(Direction.X, 0.f, Direction.Y) * HalfHeight * 0.6f;
	EmitBubbles(Behind, 3, 10.f);
	EmitRing(Behind, HalfHeight * 1.2f, 0.6f); // the kick
	OnSwimStroke.Broadcast();
}

bool UOnyxMovementComponent::SwimDash(float Fallback)
{
	// Any direction (Jump held counts as up here); straight ahead when not steering.
	const FVector2D Steering = GetSwimSteering();
	SwimDashDirection = Steering.IsNearlyZero(0.1f) ? FVector2D(Fallback >= 0.f ? 1.f : -1.f, 0.f) : Steering.GetSafeNormal();
	SwimDashTimer = SwimSettings.SwimDashTime;
	DashCooldownTimer = SwimSettings.SwimDashTime + DashSettings.Cooldown;
	DiveTimer = 0.f;
	EmitBubbles(UpdatedComponent->GetComponentLocation(), 6, 14.f);
	EmitRing(UpdatedComponent->GetComponentLocation(), GetCapsuleHalfHeight() * 2.f, 1.f);
	OnSwimDash.Broadcast();
	OnDashed.Broadcast();
	return true;
}

bool UOnyxMovementComponent::TryClimbOut(float WallSide)
{
	if (SwimSettings.ClimbOutHeight <= 0.f || !CharacterOwner)
	{
		return false;
	}
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	// A slightly slimmer capsule, so touching the bank's side doesn't count as blocked.
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius * 0.9f, HalfHeight * 0.95f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OnyxClimbOut), false, CharacterOwner);
	FCollisionResponseParams Response;
	InitCollisionParams(Params, Response);
	const ECollisionChannel Channel = UpdatedComponent->GetCollisionObjectType();
	const UWorld* World = GetWorld();

	// Room to rise by the climb height and to step over the edge...
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const FVector Raised = Location + FVector(0.f, 0.f, SwimSettings.ClimbOutHeight);
	const FVector Over = Raised + FVector(WallSide * Radius * 2.f, 0.f, 0.f);
	FHitResult Hit;
	if (World->SweepSingleByChannel(Hit, Location, Raised, FQuat::Identity, Channel, Shape, Params, Response)
		|| World->SweepSingleByChannel(Hit, Raised, Over, FQuat::Identity, Channel, Shape, Params, Response))
	{
		return false; // a ceiling, or the bank is higher than that
	}
	// ...and the top of the bank to land on.
	if (!World->SweepSingleByChannel(Hit, Over, Over - FVector(0.f, 0.f, SwimSettings.ClimbOutHeight + HalfHeight), FQuat::Identity, Channel, Shape, Params, Response)
		|| Hit.bStartPenetrating || Hit.ImpactNormal.Z < 0.7f)
	{
		return false;
	}
	const float Rise = FMath::Max(Hit.Location.Z - Location.Z, 0.f);
	LeaveWater(GetJumpVelocityForHeight(Rise + 30.f), WallSide * SwimSettings.SurfaceSwimSpeed * 0.8f);
	OnClimbedOut.Broadcast();
	return true;
}

void UOnyxMovementComponent::UpdateBreath(float DeltaTime)
{
	const AOnyxWater* CurrentWater = Water.Get();
	const FVector Head = UpdatedComponent ? UpdatedComponent->GetComponentLocation() + FVector(0.f, 0.f, GetCapsuleHalfHeight() * HeadHeight) : FVector::ZeroVector;
	if (!IsSwimmingInWater() || !CurrentWater || Head.Z >= CurrentWater->GetSurfaceHeightAt(Head.X))
	{
		Breath = SwimSettings.bLimitedBreath ? FMath::Min(Breath + DeltaTime / FMath::Max(SwimSettings.BreathRefillTime, 0.1f), 1.f) : 1.f;
		DrownTimer = 0.f;
		BreathBubbleTimer = FMath::FRandRange(0.6f, 1.2f); // the first breath out comes soon after going under
		return;
	}
	// Breathing out now and then (faster when short of breath).
	BreathBubbleTimer -= DeltaTime;
	if (BreathBubbleTimer <= 0.f)
	{
		BreathBubbleTimer = BreathBubbleInterval * FMath::FRandRange(0.7f, 1.3f) * (Breath < LowBreath ? 0.4f : 1.f);
		EmitBubbles(Head, FMath::RandRange(1, 3), 4.f);
	}
	if (!SwimSettings.bLimitedBreath)
	{
		return;
	}
	const float Before = Breath;
	Breath = FMath::Max(Breath - DeltaTime / FMath::Max(SwimSettings.BreathTime, 1.f), 0.f);
	if (Before > LowBreath && Breath <= LowBreath)
	{
		OnBreathLow.Broadcast();
	}
	if (Breath <= 0.f)
	{
		DrownTimer += DeltaTime;
		if (DrownTimer >= 1.f)
		{
			DrownTimer -= 1.f;
			EmitBubbles(Head, 4, 6.f);
			OnDrowning.Broadcast();
		}
	}
}

void UOnyxMovementComponent::PhysSwim(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}
	const AOnyxWater* CurrentWater = Water.Get();
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const float HalfHeight = GetCapsuleHalfHeight();
	const float FloatZ = CurrentWater ? CurrentWater->GetSurfaceHeightAt(Location.X) - HalfHeight * FloatDepth : Location.Z + HalfHeight; // rides the waves
	const bool bAtSurface = Location.Z > FloatZ - HalfHeight * SurfaceBand;
	const FVector2D Steering = GetSwimSteering();
	DiveTimer = FMath::Max(DiveTimer - DeltaTime, 0.f);

	if (SwimDashTimer > 0.f)
	{
		// The dash eases off towards its end, so it flows back into swimming; a trail of bubbles behind.
		const float Progress = 1.f - SwimDashTimer / FMath::Max(SwimSettings.SwimDashTime, 0.01f);
		const float Speed = SwimSettings.SwimDashSpeed * FMath::Lerp(1.f, 0.55f, Progress * Progress);
		Velocity = FVector(SwimDashDirection.X * Speed, 0.f, SwimDashDirection.Y * Speed);
		SwimDashTimer = FMath::Max(SwimDashTimer - DeltaTime, 0.f);
		if (FMath::FRand() < DeltaTime * 25.f)
		{
			EmitBubbles(Location, 1, 8.f);
		}
	}
	else
	{
		const float Cruise = bAtSurface ? SwimSettings.SurfaceSwimSpeed : SwimSettings.SwimSpeed;
		Velocity.X = ApproachSwimSpeed(Velocity.X, Steering.X * Cruise, DeltaTime);
		if (DiveTimer > 0.f && Velocity.Z < 0.f)
		{
			Velocity.Z *= FMath::Exp(-SwimSettings.DiveDrag * DeltaTime); // a dive carries on down, slowed by the water
			if (FMath::FRand() < DeltaTime * FMath::Min(-Velocity.Z / 25.f, 40.f))
			{
				EmitBubbles(Location + FVector(0.f, 0.f, HalfHeight * 0.4f), 1, HalfHeight * 0.4f); // the dive's trail
			}
		}
		else
		{
			// Steered up or down, else drifting with the buoyancy.
			const float TargetZ = FMath::Abs(Steering.Y) > 0.1f ? Steering.Y * Cruise : SwimSettings.Buoyancy;
			Velocity.Z = ApproachSwimSpeed(Velocity.Z, TargetZ, DeltaTime);
		}
	}
	Velocity.Y = 0.f;

	if (CurrentWater)
	{
		const FVector2D Current = CurrentWater->GetCurrentAt(Location);
		Velocity.X += Current.X * DeltaTime;
		Velocity.Z += Current.Y * DeltaTime;
		if (SwimSettings.bFloatAtSurface && Location.Z > FloatZ)
		{
			if (Velocity.Z >= SwimSettings.BreachSpeed)
			{
				// Too fast for the surface to hold: out of the water, faster now that it no longer holds back.
				LeaveWater(FMath::Min(Velocity.Z * SwimSettings.BreachBoost, GetJumpVelocityForHeight(JumpSettings.JumpHeight * 1.5f)), Velocity.X);
				bJumpActive = false;
				OnBreached.Broadcast();
				StartNewPhysics(DeltaTime, Iterations);
				return;
			}
			// Bob at the surface with the head out: rising eases off at the top.
			Velocity.Z = FMath::Min(Velocity.Z, (FloatZ - Location.Z) * 6.f);
		}
	}

	const FVector Delta = Velocity * DeltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Delta);
		const bool bWall = FMath::Abs(Hit.Normal.X) > 0.7f;
		// Swimming into a bank at the surface climbs out onto it.
		if (bWall && bAtSurface && Steering.X * -Hit.Normal.X > 0.5f && TryClimbOut(-FMath::Sign(Hit.Normal.X)))
		{
			return;
		}
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
		if (Hit.Normal.Z > 0.7f && Velocity.Z < 0.f)
		{
			// Landing on the bottom stirs up the sand.
			AOnyxWater* Bottom = Water.Get();
			if (Bottom && SwimSettings.bSwimEffects && Velocity.Z < -120.f)
			{
				Bottom->EmitSilt(Hit.ImpactPoint, FMath::Clamp(-Velocity.Z / 400.f, 0.3f, 1.5f));
			}
			Velocity.Z = 0.f; // resting on the bottom
			DiveTimer = 0.f;
		}
		if (bWall)
		{
			Velocity.X = 0.f;
			SwimDashTimer = 0.f; // a dash into a wall stops there
		}
	}

	// Lean into the swimming direction (head down in a dive).
	const float Speed = FVector2D(Velocity.X, Velocity.Z).Size();
	const float TargetLean = Speed > LeanMinSpeed
		? FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Velocity.Z, FMath::Abs(Velocity.X) + LeanMinSpeed)), -SwimSettings.MaxLean, SwimSettings.MaxLean)
		: 0.f;
	SwimLean = FMath::FInterpTo(SwimLean, TargetLean, DeltaTime, 8.f);
}

float UOnyxMovementComponent::GetDashCooldownFraction() const
{
	const float Total = DashSettings.Duration + DashSettings.Cooldown;
	return Total > 0.f ? FMath::Clamp(DashCooldownTimer / Total, 0.f, 1.f) : 0.f;
}

// ---------------------------------------------------------------------------
// One-way platforms
// ---------------------------------------------------------------------------

bool UOnyxMovementComponent::IsOneWaySurface(const UPrimitiveComponent* Component) const
{
	const UWorld* World = GetWorld();
	const UOnyxWorldSubsystem* Registry = World ? World->GetSubsystem<UOnyxWorldSubsystem>() : nullptr;
	return Registry && Registry->IsOneWaySurface(Component);
}

bool UOnyxMovementComponent::TryDropThroughPlatform()
{
	if (!IsMovingOnGround() || !UpdatedPrimitive)
	{
		return false;
	}
	UPrimitiveComponent* Surface = CurrentFloor.HitResult.GetComponent();
	if (!IsOneWaySurface(Surface))
	{
		return false;
	}

	DropThroughSurface = Surface;
	DropThroughTimer = DropThroughDuration;
	UpdatedPrimitive->IgnoreComponentWhenMoving(Surface, true);
	SetMovementMode(MOVE_Falling);
	Velocity.Z = -100.f;
	CoyoteTimer = 0.f;
	return true;
}

void UOnyxMovementComponent::UpdateOneWayPlatforms()
{
	const UWorld* World = GetWorld();
	const UOnyxWorldSubsystem* Registry = World ? World->GetSubsystem<UOnyxWorldSubsystem>() : nullptr;
	if (!Registry || !UpdatedPrimitive || !CharacterOwner)
	{
		return;
	}

	const FVector Location = UpdatedComponent->GetComponentLocation();
	const float FeetZ = Location.Z - CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	for (const TWeakObjectPtr<UPrimitiveComponent>& Weak : Registry->GetOneWaySurfaces())
	{
		UPrimitiveComponent* Surface = Weak.Get();
		if (!Surface)
		{
			continue;
		}
		const FBox Box = Surface->Bounds.GetBox();
		if (Location.X < Box.Min.X - OneWayPlatformRange || Location.X > Box.Max.X + OneWayPlatformRange)
		{
			continue;
		}
		const bool bDropping = DropThroughTimer > 0.f && DropThroughSurface.Get() == Surface;
		const bool bBelowTop = FeetZ < Box.Max.Z - OneWayPlatformTolerance;
		UpdatedPrimitive->IgnoreComponentWhenMoving(Surface, bDropping || bBelowTop);
	}
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

void UOnyxMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_OnyxDash && MovementMode != MOVE_Custom)
	{
		bDashing = false;
	}
	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_OnyxSwim && !IsSwimmingInWater())
	{
		SwimDashTimer = 0.f;
		DiveTimer = 0.f;
	}
	if (MovementMode == MOVE_Walking && PreviousMovementMode != MOVE_Walking)
	{
		HandleLanded();
	}
	if (MovementMode == MOVE_Falling && PreviousMovementMode == MOVE_Walking && !bJumpActive)
	{
		CoyoteTimer = JumpSettings.CoyoteTime;
	}
}

void UOnyxMovementComponent::HandleLanded()
{
	JumpsUsed = 0;
	AirDashesUsed = 0;
	bJumpActive = false;
	OnLandedEvent.Broadcast(); // a buffered jump fires on the next tick
}

EOnyxLocomotion UOnyxMovementComponent::GetLocomotionState() const
{
	if (bDashing)
	{
		return EOnyxLocomotion::Dash;
	}
	if (bWallSliding)
	{
		return EOnyxLocomotion::WallSlide;
	}
	if (IsSwimmingInWater())
	{
		return EOnyxLocomotion::Swim;
	}
	if (IsFalling())
	{
		return Velocity.Z > 0.f ? EOnyxLocomotion::Jump : EOnyxLocomotion::Fall;
	}
	if (IsCrouchingDown())
	{
		return EOnyxLocomotion::Crouch;
	}
	return FMath::Abs(Velocity.X) > RunThreshold ? EOnyxLocomotion::Run : EOnyxLocomotion::Idle;
}
