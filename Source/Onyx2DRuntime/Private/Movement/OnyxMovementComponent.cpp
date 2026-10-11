// Copyright Onyx2D. All Rights Reserved.

#include "Movement/OnyxMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/OnyxWorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

namespace
{
	constexpr float OneWayPlatformRange = 400.f;
	constexpr float OneWayPlatformTolerance = 4.f;
	constexpr float DropThroughDuration = 0.3f;
	constexpr float WallNormalMaxZ = 0.35f;
	constexpr float WallJumpSteeringScale = 0.15f;
	constexpr float RunThreshold = 15.f;
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
