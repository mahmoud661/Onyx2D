// Copyright Onyx2D. All Rights Reserved.

#include "Movement/OnyxMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/OnyxLevelData.h"
#include "Core/OnyxWorldSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "World/OnyxWater.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxMovement, Log, All);

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
	/** A ledge's wall must be within this much of the capsule's side, cm. */
	constexpr float LedgeWallReach = 14.f;
	/** After letting go of a ledge: no grabbing for this long. */
	constexpr float LedgeRegrabTime = 0.3f;
	/** Pulling up onto a ledge rises for this part of the climb, then steps over the edge. */
	constexpr float LedgeRisePart = 0.65f;
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
	if (CrouchHeightScale < 0.999f && CharacterOwner)
	{
		// Crouching and sliding use the engine's crouch: the capsule shrinks and only stands up again where there is room.
		NavAgentProps.bCanCrouch = true;
		bCanWalkOffLedgesWhenCrouching = true;
		SetCrouchedHalfHeight(CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * CrouchHeightScale);
	}
	ApplyGameStyle(GetActiveGameStyle());
}

// ---------------------------------------------------------------------------
// Game style
// ---------------------------------------------------------------------------

EOnyxGameStyle UOnyxMovementComponent::GetActiveGameStyle() const
{
	return bUseLevelGameStyle ? UOnyxLevelData::GetGameStyle(GetWorld()) : GameStyle;
}

void UOnyxMovementComponent::SetGameStyle(EOnyxGameStyle Style)
{
	bUseLevelGameStyle = false;
	GameStyle = Style;
	ApplyGameStyle(Style);
}

void UOnyxMovementComponent::ApplyGameStyle(EOnyxGameStyle Style)
{
	const bool bWantTopDown = Style == EOnyxGameStyle::TopDown;
	if (!CharacterOwner || !UpdatedComponent || bWantTopDown == bTopDown)
	{
		return; // already moving this way (side view from the start: nothing to change)
	}
	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float OldHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	if (bWantTopDown)
	{
		// From above, the body that meets walls and blows is the ground the feet cover: a circle there.
		SideCapsule = FVector2D(Capsule->GetUnscaledCapsuleRadius(), Capsule->GetUnscaledCapsuleHalfHeight());
		const float Radius = FootprintRadius > 0.f ? FootprintRadius : SideCapsule.X;
		Capsule->SetCapsuleSize(Radius, Radius, false);
	}
	else if (!SideCapsule.IsZero())
	{
		Capsule->SetCapsuleSize(SideCapsule.X, SideCapsule.Y, false);
	}
	// The feet stay where they were.
	const float Drop = OldHalfHeight - Capsule->GetScaledCapsuleHalfHeight();
	UpdatedComponent->SetWorldLocation(UpdatedComponent->GetComponentLocation() - FVector(0.f, 0.f, Drop), false, nullptr, ETeleportType::TeleportPhysics);
	bTopDown = bWantTopDown;
	Velocity = FVector::ZeroVector;
	if (bTopDown)
	{
		bSliding = false;
		bLedgeHanging = false;
		bLedgeClimbing = false;
		bWantsToCrouch = false;
		SetMovementMode(MOVE_Custom, CMOVE_OnyxTopDown);
	}
	else
	{
		SetMovementMode(MOVE_Falling);
	}
	UE_LOG(LogOnyxMovement, Verbose, TEXT("%s: moves %s"), *GetNameSafe(GetOwner()), bTopDown ? TEXT("top down") : TEXT("side view"));
}

void UOnyxMovementComponent::SetDefaultMovementMode()
{
	if (bTopDown)
	{
		SetMovementMode(MOVE_Custom, CMOVE_OnyxTopDown);
		return;
	}
	Super::SetDefaultMovementMode();
}

bool UOnyxMovementComponent::HandlePendingLaunch()
{
	if (!bTopDown || PendingLaunchVelocity.IsZero() || !HasValidData())
	{
		return Super::HandlePendingLaunch();
	}
	// Thrown across the ground (a blow): it keeps walking the ground, the speed wearing off at the braking rate.
	Velocity = FVector(PendingLaunchVelocity.X, 0.f, PendingLaunchVelocity.Z);
	PendingLaunchVelocity = FVector::ZeroVector;
	bDashing = false;
	if (MovementMode != MOVE_Custom || CustomMovementMode != CMOVE_OnyxTopDown)
	{
		SetMovementMode(MOVE_Custom, CMOVE_OnyxTopDown);
	}
	return true;
}

FVector2D UOnyxMovementComponent::GetStandingCapsule() const
{
	if (!SideCapsule.IsZero())
	{
		return SideCapsule;
	}
	const UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	return Capsule ? FVector2D(Capsule->GetUnscaledCapsuleRadius(), Capsule->GetUnscaledCapsuleHalfHeight()) : FVector2D(20.f, 40.f);
}

FVector2D UOnyxMovementComponent::GetTopDownSteering() const
{
	if (!bTopDown || bMovementLocked || bDashing)
	{
		return FVector2D::ZeroVector;
	}
	// Moves added along X and Z (players: X; AI: both), plus up and down held (keys, the stick).
	const float MaxInput = FMath::Max(GetMaxAcceleration(), 1.f);
	const float Up = Acceleration.Z / MaxInput + VerticalInput + (bUpHeld ? 1.f : 0.f) - (bCrouchHeld ? 1.f : 0.f);
	const FVector2D Steering(FMath::Clamp(Acceleration.X / MaxInput, -1.f, 1.f), FMath::Clamp(Up, -1.f, 1.f));
	return Steering.SizeSquared() > 1.f ? Steering.GetSafeNormal() : Steering;
}

void UOnyxMovementComponent::MoveAndSlide(const FVector& Delta, float DeltaTime)
{
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Delta);
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
		// No speed into the wall: what is left runs along it.
		const FVector Normal = FVector(Hit.Normal.X, 0.f, Hit.Normal.Z).GetSafeNormal();
		const float Into = FVector::DotProduct(Velocity, Normal);
		if (Into < 0.f)
		{
			Velocity -= Normal * Into;
		}
	}
}

void UOnyxMovementComponent::PhysTopDown(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}
	// In Onyx Water: wading through the shallows (slower), swimming beyond them (its swim speed, the water's slower turns).
	const AOnyxWater* InWater = Water.Get();
	const FVector Feet = UpdatedComponent->GetComponentLocation();
	const float WaterDepth = InWater ? InWater->GetDepthFromAbove(Feet) : -1.f;
	const bool bWasSwimming = bSwimmingFromAbove;
	bSwimmingFromAbove = InWater && SwimSettings.bCanSwim && WaterDepth > InWater->ShoreWidth;
	bWading = InWater && WaterDepth > 0.f && !bSwimmingFromAbove;
	if (bSwimmingFromAbove != bWasSwimming)
	{
		(bSwimmingFromAbove ? OnStartedSwimming : OnStoppedSwimming).Broadcast();
	}
	const FVector2D Steering = GetTopDownSteering();
	const float MaxSpeed = bSwimmingFromAbove ? SwimSettings.SwimSpeed : bWading ? GetMaxSpeed() * SwimSettings.WadeSpeedScale : GetMaxSpeed();
	const FVector2D Current(Velocity.X, Velocity.Z);
	const FVector2D Target = Steering * MaxSpeed;
	// Speeding up and turning answer at once; let go, or faster than it walks (thrown by a blow, the end of a dash), the
	// speed wears off at the braking rate and steering only bends it.
	const bool bBraking = Steering.IsNearlyZero(0.05f) || Current.Size() > MaxSpeed * 1.05f;
	const float Rate = bSwimmingFromAbove ? (bBraking ? SwimSettings.SwimBraking : SwimSettings.SwimAcceleration) : (bBraking ? GroundBraking : GroundAcceleration);
	const FVector2D ToTarget = Target - Current;
	const FVector2D Next = ToTarget.Size() <= Rate * DeltaTime ? Target : Current + ToTarget.GetSafeNormal() * Rate * DeltaTime;
	Velocity = FVector(Next.X, 0.f, Next.Y);
	if (InWater && bSwimmingFromAbove)
	{
		const FVector2D Flow = InWater->GetCurrentAt(Feet);
		Velocity += FVector(Flow.X, 0.f, Flow.Y) * DeltaTime; // carried by the current
	}
	// A character that can't swim wades in no further than the shallows.
	if (InWater && !SwimSettings.bCanSwim && InWater->GetDepthFromAbove(Feet + Velocity * DeltaTime) > InWater->ShoreWidth
		&& InWater->GetDepthFromAbove(Feet + Velocity * DeltaTime) > WaterDepth)
	{
		Velocity = FVector::ZeroVector;
		return;
	}
	MoveAndSlide(Velocity * DeltaTime, DeltaTime);
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
	SetWalkableFloorAngle(SlopeSettings.MaxWalkAngle);
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
	if (bTopDown)
	{
		return MaxWalkSpeed * SpeedScale;
	}
	if (IsOnLedge())
	{
		return 0.f;
	}
	if (bSlopeSliding)
	{
		return SlopeSettings.MaxSlideSpeed;
	}
	if (bSliding)
	{
		// Slowing down from the start speed to the end speed over the slide.
		const float Elapsed = 1.f - SlideTimer / FMath::Max(SlideSettings.Duration, 0.05f);
		return FMath::Lerp(SlideStartSpeed, RunSpeed * SlideSettings.EndSpeedFraction, FMath::Clamp(Elapsed, 0.f, 1.f));
	}
	if (IsSwimmingInWater())
	{
		return SwimSettings.SwimSpeed * SpeedScale;
	}
	if (IsCrouchingDown())
	{
		return MaxWalkSpeed * CrouchSpeedScale * SpeedScale;
	}
	return Super::GetMaxSpeed() * SpeedScale;
}

float UOnyxMovementComponent::GetMaxAcceleration() const
{
	return bSlopeSliding ? Super::GetMaxAcceleration() * SlopeSettings.SlideSteering : Super::GetMaxAcceleration();
}

float UOnyxMovementComponent::GetMaxBrakingDeceleration() const
{
	// Sliding down a wet slope nothing holds it back but the little steering it has.
	return bSlopeSliding && MovementMode == MOVE_Walking ? 0.f : Super::GetMaxBrakingDeceleration();
}

void UOnyxMovementComponent::ApplySurfaceFlow(FVector2D Push, bool bWet)
{
	// Several streams at once add up within a frame; a call after the flow ran out starts afresh.
	if (SurfaceFlowTimer <= 0.f)
	{
		SurfacePush = FVector2D::ZeroVector;
		bSurfaceWet = false;
	}
	SurfacePush += Push;
	bSurfaceWet |= bWet;
	SurfaceFlowTimer = 0.12f;
}

void UOnyxMovementComponent::UpdateSlopes(float DeltaTime)
{
	SurfaceFlowTimer -= DeltaTime;
	const bool bFlowing = SurfaceFlowTimer > 0.f;
	if (!bFlowing && SurfaceFlowTimer < -1.f)
	{
		SurfacePush = FVector2D::ZeroVector;
		bSurfaceWet = false;
	}
	const bool bOnGround = IsMovingOnGround() && CurrentFloor.IsWalkableFloor();
	const FVector Normal = bOnGround ? CurrentFloor.HitResult.ImpactNormal : FVector::UpVector;
	GroundSlope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Normal.Z, -1.f, 1.f)));
	const bool bSloped = bOnGround && GroundSlope > 5.f;
	const bool bWasSliding = bSlopeSliding;
	bSlopeSliding = bSloped && !bDashing && !bMovementLocked && !IsOnLedge()
		&& ((bFlowing && bSurfaceWet) || (SlopeSettings.DrySlideAngle > 0.f && GroundSlope >= SlopeSettings.DrySlideAngle));
	GroundFriction = bSlopeSliding ? 0.f : 8.f;
	if (bSlopeSliding)
	{
		// Down the slope, sideways (walking keeps the speed level: the ground under it turns it into the slope).
		const float Downhill = Normal.X >= 0.f ? 1.f : -1.f;
		Velocity.X = FMath::Clamp(Velocity.X + Downhill * SlopeSettings.WetSlideAcceleration * DeltaTime, -SlopeSettings.MaxSlideSpeed, SlopeSettings.MaxSlideSpeed);
	}
	else if (bWasSliding && bOnGround)
	{
		// Off the bottom of a wet slope it keeps some of its speed (the braking takes the rest).
		Velocity.X = FMath::Clamp(Velocity.X, -RunSpeed * 1.2f, RunSpeed * 1.2f);
	}
	if (bFlowing && !SurfacePush.IsNearlyZero() && !IsSwimmingInWater() && !bDashing && !IsOnLedge())
	{
		Velocity.X += SurfacePush.X * DeltaTime;
		if (!bOnGround)
		{
			Velocity.Z += SurfacePush.Y * DeltaTime;
		}
	}

	// The feet on the slope right under the middle: the capsule rests on it beside the middle.
	float Foot = 0.f;
	if (SlopeSettings.bFeetOnSlope && bSloped && CharacterOwner)
	{
		const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		const float Radius = Capsule->GetScaledCapsuleRadius();
		const FVector Bottom = UpdatedComponent->GetComponentLocation() - FVector(0.f, 0.f, HalfHeight);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(OnyxSlopeFoot), false, CharacterOwner);
		FCollisionResponseParams Response;
		InitCollisionParams(Params, Response);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Bottom + FVector(0.f, 0.f, 2.f), Bottom - FVector(0.f, 0.f, Radius * 2.f), UpdatedComponent->GetCollisionObjectType(), Params, Response))
		{
			// On flat ground the capsule hovers Floor Dist over it and the picture already allows for that.
			Foot = FMath::Clamp(Hit.ImpactPoint.Z - Bottom.Z + CurrentFloor.FloorDist, -Radius * 1.5f, 0.f);
		}
	}
	SlopeFootOffset = Foot;
}

FVector UOnyxMovementComponent::ConstrainInputAcceleration(const FVector& InputAcceleration) const
{
	if (bMovementLocked || bDashing || IsOnLedge())
	{
		return FVector::ZeroVector;
	}
	if (bSliding)
	{
		return FVector(SlideDirection, 0.f, 0.f); // carries on by itself, slowing down
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
	LedgeRegrabTimer = FMath::Max(LedgeRegrabTimer - DeltaTime, 0.f);
	if (bTopDown)
	{
		// From above there is no jumping, crouching, walls to slide down, ledges or swimming: just walking and dashing.
		JumpBufferTimer = 0.f;
		bWantsToCrouch = false;
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
		return;
	}
	if (IsFalling())
	{
		FallingSpeed = FMath::Max(FallingSpeed, -Velocity.Z);
	}

	// Down pressed while running: a slide.
	const bool bCrouchPressed = bCrouchHeld && !bCrouchHeldBefore;
	bCrouchHeldBefore = bCrouchHeld;
	if (bCrouchPressed && SlideSettings.bEnabled && !bSliding && !bDashing && !bMovementLocked && IsMovingOnGround()
		&& FMath::Abs(Velocity.X) >= RunSpeed * SlideSettings.MinSpeedFraction)
	{
		StartSlide();
	}
	if (bSliding)
	{
		SlideTimer -= DeltaTime;
		if (SlideTimer <= 0.f || !IsMovingOnGround())
		{
			EndSlide();
		}
	}

	// Ledges: Jump or Up climbs, Down or away lets go; falling past one while holding towards it catches it.
	if (bLedgeHanging)
	{
		const float Steering = FMath::Sign(Acceleration.X);
		if (JumpBufferTimer > 0.f || IsUpHeld())
		{
			JumpBufferTimer = 0.f;
			StartLedgeClimb();
		}
		else if (bCrouchHeld || VerticalInput < -0.5f || (Steering != 0.f && Steering != LedgeDirection))
		{
			ReleaseLedge();
		}
	}
	else if (LedgeSettings.bEnabled && !bLedgeClimbing && IsFalling() && Velocity.Z <= 0.f && !bDashing && !bMovementLocked && LedgeRegrabTimer <= 0.f)
	{
		const float Steering = FMath::Sign(Acceleration.X);
		if (Steering != 0.f)
		{
			TryGrabLedge(Steering);
		}
	}
	UpdateCrouch();
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
	UpdateSlopes(DeltaTime);
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
	if (JumpBufferTimer <= 0.f || bMovementLocked || bDashing || IsOnLedge() || !CharacterOwner || bTopDown)
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
	if (bSliding)
	{
		EndSlide();
	}

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
	bJumpActive = bJumpHeld && !WallSettings.bFullHeightWallJump; // not active: letting go doesn't cut it
	bWallSliding = false;

	UE_LOG(LogOnyxMovement, Verbose, TEXT("%s: wall jump off the %s wall at %s"), *GetNameSafe(GetOwner()), WallDirection < 0 ? TEXT("left") : TEXT("right"),
		*UpdatedComponent->GetComponentLocation().ToCompactString());
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
	if (!DashSettings.bEnabled || bDashing || bMovementLocked || IsOnLedge() || DashCooldownTimer > 0.f)
	{
		return false;
	}
	if (bTopDown)
	{
		// Where it is steered, else straight to the side Direction says.
		const FVector2D Steering = GetTopDownSteering();
		return TryDashToward(Steering.IsNearlyZero(0.1f) ? FVector2D(Direction >= 0.f ? 1.f : -1.f, 0.f) : Steering);
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

	if (bSliding)
	{
		EndSlide();
	}
	bDashFromGround = IsMovingOnGround();
	DashDirection = Direction >= 0.f ? 1.f : -1.f;
	DashTimer = DashSettings.Duration;
	DashCooldownTimer = DashSettings.Duration + DashSettings.Cooldown;
	bDashing = true;
	bJumpActive = false;
	SetMovementMode(MOVE_Custom, CMOVE_OnyxDash);

	OnDashed.Broadcast();
	return true;
}

bool UOnyxMovementComponent::TryDashToward(FVector2D Direction)
{
	if (!bTopDown)
	{
		return TryDash(Direction.X);
	}
	if (!DashSettings.bEnabled || bDashing || bMovementLocked || DashCooldownTimer > 0.f)
	{
		return false;
	}
	DashVector = Direction.IsNearlyZero(0.01f) ? FVector2D(1.f, 0.f) : Direction.GetSafeNormal();
	DashDirection = DashVector.X >= 0.f ? 1.f : -1.f;
	bDashFromGround = true; // a roll along the ground
	DashTimer = DashSettings.Duration;
	DashCooldownTimer = DashSettings.Duration + DashSettings.Cooldown;
	bDashing = true;
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
	if (CustomMovementMode == CMOVE_OnyxLedge)
	{
		PhysLedge(DeltaTime, Iterations);
		return;
	}
	if (CustomMovementMode == CMOVE_OnyxTopDown)
	{
		PhysTopDown(DeltaTime, Iterations);
		return;
	}
	Super::PhysCustom(DeltaTime, Iterations);
}

// ---------------------------------------------------------------------------
// Crouch and slide
// ---------------------------------------------------------------------------

void UOnyxMovementComponent::UpdateCrouch()
{
	if (CrouchHeightScale >= 0.999f)
	{
		bWantsToCrouch = false;
		return;
	}
	// The engine crouches before the next move and stands up again only where there is room overhead.
	bWantsToCrouch = IsMovingOnGround() && !bDashing && (bCrouchHeld || bSliding);
}

void UOnyxMovementComponent::StartSlide()
{
	bSliding = true;
	SlideDirection = Velocity.X >= 0.f ? 1.f : -1.f;
	SlideStartSpeed = FMath::Max(FMath::Abs(Velocity.X), RunSpeed * SlideSettings.StartSpeedFraction);
	SlideTimer = SlideSettings.Duration;
	Velocity.X = SlideDirection * SlideStartSpeed;
	OnSlideStarted.Broadcast();
}

void UOnyxMovementComponent::EndSlide()
{
	bSliding = false;
	SlideTimer = 0.f;
}

// ---------------------------------------------------------------------------
// Ledges
// ---------------------------------------------------------------------------

bool UOnyxMovementComponent::TryGrabLedge(float Direction)
{
	if (!CharacterOwner)
	{
		return false;
	}
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Here = UpdatedComponent->GetComponentLocation();
	const float HeadTop = Here.Z + HalfHeight;
	const ECollisionChannel Channel = UpdatedComponent->GetCollisionObjectType();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OnyxLedgeProbe), false, CharacterOwner);
	FCollisionResponseParams Response;
	InitCollisionParams(Params, Response);
	UWorld* World = GetWorld();

	// Only the world has ledges: not another character (an enemy, an ally) it bumps into in the air.
	auto IsCharacter = [](const FHitResult& Hit) { return Cast<APawn>(Hit.GetActor()) != nullptr; };

	// A wall right in front at chest height...
	FHitResult Wall;
	const FVector Chest(Here.X, Here.Y, Here.Z + HalfHeight * 0.3f);
	if (!World->LineTraceSingleByChannel(Wall, Chest, Chest + FVector(Direction * (Radius + LedgeWallReach), 0.f, 0.f), Channel, Params, Response)
		|| FMath::Abs(Wall.ImpactNormal.Z) >= WallNormalMaxZ || IsOneWaySurface(Wall.GetComponent()) || IsCharacter(Wall))
	{
		return false;
	}
	// ...open air above it...
	const FVector Above(Here.X, Here.Y, HeadTop + LedgeSettings.GrabReach);
	FHitResult Blocked;
	if (World->LineTraceSingleByChannel(Blocked, Above, Above + FVector(Direction * (Radius + LedgeWallReach + 8.f), 0.f, 0.f), Channel, Params, Response))
	{
		return false;
	}
	// ...and its top edge within reach of the hands.
	FHitResult Top;
	const float EdgeX = Wall.ImpactPoint.X + Direction * 6.f;
	const FVector Down(EdgeX, Here.Y, HeadTop + LedgeSettings.GrabReach);
	if (!World->LineTraceSingleByChannel(Top, Down, FVector(EdgeX, Here.Y, HeadTop - LedgeSettings.GrabReach), Channel, Params, Response)
		|| Top.ImpactNormal.Z < 0.7f || IsCharacter(Top))
	{
		return false;
	}
	const float EdgeZ = Top.ImpactPoint.Z;
	const float HangX = Wall.ImpactPoint.X - Direction * (Radius + 0.5f);
	const FVector Hang(HangX, Here.Y, EdgeZ - LedgeSettings.HangDepth - HalfHeight);
	const FVector Stand(Wall.ImpactPoint.X + Direction * (Radius + 2.f), Here.Y, EdgeZ + HalfHeight + 2.f);
	// Room to pull up: straight up beside the wall, then over the edge.
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	if (World->OverlapBlockingTestByChannel(FVector(HangX, Here.Y, Stand.Z), FQuat::Identity, Channel, Shape, Params, Response)
		|| World->OverlapBlockingTestByChannel(Stand, FQuat::Identity, Channel, Shape, Params, Response))
	{
		return false;
	}

	bLedgeHanging = true;
	bLedgeClimbing = false;
	LedgeDirection = Direction;
	LedgeHangLocation = Hang;
	LedgeStandLocation = Stand;
	JumpBufferTimer = 0.f;
	bJumpActive = false;
	JumpsUsed = 0;
	AirDashesUsed = 0;
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Custom, CMOVE_OnyxLedge);
	UpdatedComponent->SetWorldLocation(Hang, false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogOnyxMovement, Verbose, TEXT("%s: caught a ledge at %s (edge %.0f)"), *GetNameSafe(GetOwner()), *Hang.ToCompactString(), EdgeZ);
	OnLedgeGrabbed.Broadcast();
	return true;
}

void UOnyxMovementComponent::StartLedgeClimb()
{
	UE_LOG(LogOnyxMovement, Verbose, TEXT("%s: climbing up to %s"), *GetNameSafe(GetOwner()), *LedgeStandLocation.ToCompactString());
	bLedgeHanging = false;
	bLedgeClimbing = true;
	LedgeTimer = LedgeSettings.ClimbTime;
}

void UOnyxMovementComponent::ReleaseLedge()
{
	UE_LOG(LogOnyxMovement, Verbose, TEXT("%s: let go of the ledge"), *GetNameSafe(GetOwner()));
	bLedgeHanging = false;
	bLedgeClimbing = false;
	LedgeRegrabTimer = LedgeRegrabTime;
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Falling);
}

void UOnyxMovementComponent::PhysLedge(float DeltaTime, int32 Iterations)
{
	Velocity = FVector::ZeroVector;
	if (!bLedgeClimbing)
	{
		return; // hanging still
	}
	LedgeTimer -= DeltaTime;
	const float Progress = FMath::Clamp(1.f - LedgeTimer / FMath::Max(LedgeSettings.ClimbTime, 0.05f), 0.f, 1.f);
	FVector Location = LedgeHangLocation;
	if (Progress < LedgeRisePart)
	{
		Location.Z = FMath::InterpEaseOut(LedgeHangLocation.Z, LedgeStandLocation.Z, Progress / LedgeRisePart, 2.f);
	}
	else
	{
		Location.Z = LedgeStandLocation.Z;
		Location.X = FMath::Lerp(LedgeHangLocation.X, LedgeStandLocation.X, (Progress - LedgeRisePart) / (1.f - LedgeRisePart));
	}
	UpdatedComponent->SetWorldLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	if (Progress >= 1.f)
	{
		bLedgeClimbing = false;
		SetMovementMode(MOVE_Walking);
		UE_LOG(LogOnyxMovement, Verbose, TEXT("%s: up on the ledge at %s"), *GetNameSafe(GetOwner()), *UpdatedComponent->GetComponentLocation().ToCompactString());
		OnLedgeClimbed.Broadcast();
	}
}

void UOnyxMovementComponent::PhysDash(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	Velocity = bTopDown ? FVector(DashVector.X, 0.f, DashVector.Y) * GetMaxSpeed() : FVector(DashDirection * GetMaxSpeed(), 0.f, 0.f);
	MoveAndSlide(Velocity * DeltaTime, DeltaTime);

	DashTimer -= DeltaTime;
	if (DashTimer <= 0.f)
	{
		EndDash();
	}
}

void UOnyxMovementComponent::CancelDash()
{
	if (bDashing)
	{
		EndDash();
	}
}

void UOnyxMovementComponent::EndDash()
{
	bDashing = false;
	if (bTopDown)
	{
		Velocity = FVector(DashVector.X, 0.f, DashVector.Y) * RunSpeed;
		SetMovementMode(MOVE_Custom, CMOVE_OnyxTopDown);
		return;
	}
	Velocity = FVector(DashDirection * RunSpeed, 0.f, 0.f);
	SetMovementMode(MOVE_Falling);
}

// ---------------------------------------------------------------------------
// Swimming
// ---------------------------------------------------------------------------

void UOnyxMovementComponent::SetWater(AOnyxWater* InWater)
{
	// From above, several Onyx Waters can make one pond (rectangles side by side or overlapping, a pond of any shape): it
	// keeps to the one it stands deepest in, so it swims wherever the pond is deep, not only well inside one rectangle.
	const AOnyxWater* Current = Water.Get();
	if (Current && InWater && Current != InWater && UpdatedComponent && IsTopDown())
	{
		const FVector Feet = UpdatedComponent->GetComponentLocation();
		if (Current->GetDepthFromAbove(Feet) >= InWater->GetDepthFromAbove(Feet))
		{
			return;
		}
	}
	Water = InWater;
}

void UOnyxMovementComponent::ClearWater(const AOnyxWater* InWater)
{
	if (Water.Get() == InWater)
	{
		// Out of one rectangle of a pond but still in another: on into that one, without stopping and starting to swim.
		if (UpdatedComponent && IsTopDown())
		{
			const FVector Feet = UpdatedComponent->GetComponentLocation();
			AOnyxWater* Deepest = nullptr;
			float Depth = 0.f;
			for (TActorIterator<AOnyxWater> It(GetWorld()); It; ++It)
			{
				const float Here = *It != InWater ? It->GetDepthFromAbove(Feet) : 0.f;
				if (Here > Depth)
				{
					Depth = Here;
					Deepest = *It;
				}
			}
			if (Deepest)
			{
				Water = Deepest;
				return;
			}
		}
		Water.Reset();
		if (bSwimmingFromAbove)
		{
			bSwimmingFromAbove = false;
			OnStoppedSwimming.Broadcast();
		}
		bWading = false;
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

bool UOnyxMovementComponent::IsUnderWater() const
{
	const AOnyxWater* CurrentWater = Water.Get();
	if (!CurrentWater || !UpdatedComponent || bSwimmingFromAbove || !IsSwimmingInWater())
	{
		return false;
	}
	// Below the band the surface swimming keeps it in (where the head is out), as swimming itself tells them apart.
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const float HalfHeight = GetCapsuleHalfHeight();
	const float FloatZ = CurrentWater->GetSurfaceHeightAt(Location.X) - HalfHeight * FloatDepth;
	return Location.Z <= FloatZ - HalfHeight * SurfaceBand;
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
		const FVector2D Current = CurrentWater->GetCurrentAt(Location) + (SurfaceFlowTimer > 0.f ? SurfacePush : FVector2D::ZeroVector);
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

	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_OnyxDash && !(MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_OnyxDash))
	{
		bDashing = false;
	}
	if (bTopDown && MovementMode != MOVE_Custom && MovementMode != MOVE_None)
	{
		// From above nothing falls or walks a floor (a respawn, a teleport, the engine's defaults): back to the ground plane.
		SetMovementMode(MOVE_Custom, CMOVE_OnyxTopDown);
		return;
	}
	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_OnyxSwim && !IsSwimmingInWater())
	{
		SwimDashTimer = 0.f;
		DiveTimer = 0.f;
	}
	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_OnyxLedge && !(MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_OnyxLedge))
	{
		// Knocked off (a hit) or done climbing.
		if (bLedgeHanging || bLedgeClimbing)
		{
			UE_LOG(LogOnyxMovement, Verbose, TEXT("%s: knocked off the ledge (now %s)"), *GetNameSafe(GetOwner()), *GetMovementName());
			LedgeRegrabTimer = LedgeRegrabTime;
		}
		bLedgeHanging = false;
		bLedgeClimbing = false;
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
	LandingSpeed = FallingSpeed;
	FallingSpeed = 0.f;
	OnLandedEvent.Broadcast(); // a buffered jump fires on the next tick
}

EOnyxLocomotion UOnyxMovementComponent::GetLocomotionState() const
{
	if (bTopDown)
	{
		if (bDashing)
		{
			return EOnyxLocomotion::Roll;
		}
		if (bSwimmingFromAbove)
		{
			return EOnyxLocomotion::Swim;
		}
		const float GroundSpeed = FVector2D(Velocity.X, Velocity.Z).Size();
		if (GroundSpeed <= RunThreshold)
		{
			return EOnyxLocomotion::Idle;
		}
		return WalkSpeedFraction > 0.f && GroundSpeed < RunSpeed * WalkSpeedFraction ? EOnyxLocomotion::Walk : EOnyxLocomotion::Run;
	}
	if (bLedgeClimbing)
	{
		return EOnyxLocomotion::LedgeClimb;
	}
	if (bLedgeHanging)
	{
		return EOnyxLocomotion::LedgeHang;
	}
	if (bDashing)
	{
		return bDashFromGround ? EOnyxLocomotion::Roll : EOnyxLocomotion::Dash;
	}
	if (bSliding || bSlopeSliding)
	{
		return EOnyxLocomotion::Slide;
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
	const float Speed = FMath::Abs(Velocity.X);
	if (IsCrouchingDown())
	{
		return Speed > RunThreshold ? EOnyxLocomotion::CrouchWalk : EOnyxLocomotion::Crouch;
	}
	if (Speed <= RunThreshold)
	{
		return EOnyxLocomotion::Idle;
	}
	return WalkSpeedFraction > 0.f && Speed < RunSpeed * WalkSpeedFraction ? EOnyxLocomotion::Walk : EOnyxLocomotion::Run;
}
