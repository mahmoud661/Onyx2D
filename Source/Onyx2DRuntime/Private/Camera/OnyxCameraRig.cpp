// Copyright Onyx2D. All Rights Reserved.

#include "Camera/OnyxCameraRig.h"

namespace
{
	constexpr float LeadMinSpeed = 40.f;

	/** Framerate-independent exponential approach: fraction of the gap closed in DeltaTime for a given "time to catch up". */
	float DampAlpha(float Seconds, float DeltaTime)
	{
		return Seconds <= KINDA_SMALL_NUMBER ? 1.f : 1.f - FMath::Exp(-DeltaTime * 4.f / Seconds);
	}

	/** One axis of the framing transposer. */
	float FollowAxis(float Current, float Desired, float DeadHalf, float SoftHalf, float Damping, float DeltaTime)
	{
		const float Delta = Desired - Current;
		if (FMath::Abs(Delta) <= DeadHalf)
		{
			return Current;
		}
		const float Excess = Delta - FMath::Sign(Delta) * DeadHalf;
		float Result = Current + Excess * DampAlpha(Damping, DeltaTime);
		// Hard limit: the target never leaves the soft zone.
		const float Remaining = Desired - Result;
		if (FMath::Abs(Remaining) > SoftHalf)
		{
			Result = Desired - FMath::Sign(Remaining) * SoftHalf;
		}
		return Result;
	}
}

FOnyxCameraSettings FOnyxCameraSettings::Blend(const FOnyxCameraSettings& A, const FOnyxCameraSettings& B, float Alpha)
{
	if (Alpha <= 0.f)
	{
		return A;
	}
	if (Alpha >= 1.f)
	{
		return B;
	}
	FOnyxCameraSettings Out = Alpha < 0.5f ? A : B; // discrete choices switch half way
	Out.ViewHeight = FMath::Lerp(A.ViewHeight, B.ViewHeight, Alpha);
	Out.FieldOfView = FMath::Lerp(A.FieldOfView, B.FieldOfView, Alpha);
	Out.TargetOffset = FMath::Lerp(A.TargetOffset, B.TargetOffset, Alpha);
	Out.ScreenOffset = FMath::Lerp(A.ScreenOffset, B.ScreenOffset, Alpha);
	Out.DeadZone = FMath::Lerp(A.DeadZone, B.DeadZone, Alpha);
	Out.SoftZone = FMath::Lerp(A.SoftZone, B.SoftZone, Alpha);
	Out.Damping = FMath::Lerp(A.Damping, B.Damping, Alpha);
	Out.LookAheadTime = FMath::Lerp(A.LookAheadTime, B.LookAheadTime, Alpha);
	Out.LookAheadMax = FMath::Lerp(A.LookAheadMax, B.LookAheadMax, Alpha);
	Out.LookAheadSmoothing = FMath::Lerp(A.LookAheadSmoothing, B.LookAheadSmoothing, Alpha);
	Out.FallLookDown = FMath::Lerp(A.FallLookDown, B.FallLookDown, Alpha);
	Out.FallSpeed = FMath::Lerp(A.FallSpeed, B.FallSpeed, Alpha);
	Out.ConfinePadding = FMath::Lerp(A.ConfinePadding, B.ConfinePadding, Alpha);
	Out.ArtPixelSize = FMath::Lerp(A.ArtPixelSize, B.ArtPixelSize, Alpha);
	Out.ShakeMaxOffset = FMath::Lerp(A.ShakeMaxOffset, B.ShakeMaxOffset, Alpha);
	Out.ShakeMaxRoll = FMath::Lerp(A.ShakeMaxRoll, B.ShakeMaxRoll, Alpha);
	return Out;
}

// --- Framing -----------------------------------------------------------------------------

FVector2D FOnyxCameraFraming::DesiredCenter(const FOnyxCameraSettings& Settings, const FOnyxCameraTarget& Target, const FVector2D& ViewHalf) const
{
	FVector2D Point = Target.Position + Settings.TargetOffset + Lead;
	if (Settings.VerticalFollow == EOnyxVerticalFollow::WhenGrounded && !Target.bGrounded)
	{
		Point.Y = GroundY + Settings.TargetOffset.Y + Lead.Y; // hold the last ground height while jumping
	}
	Point.Y -= FallOffset;
	// ScreenOffset > 0 places the target below the centre, so the centre moves up.
	return Point + FVector2D(-Settings.ScreenOffset.X * ViewHalf.X * 2.f, Settings.ScreenOffset.Y * ViewHalf.Y * 2.f);
}

void FOnyxCameraFraming::UpdateLead(const FOnyxCameraSettings& Settings, const FOnyxCameraTarget& Target, const FVector2D& ViewHalf, float DeltaTime)
{
	FVector2D WantedLead = FVector2D::ZeroVector;
	if (Settings.LookAheadTime > 0.f)
	{
		const float MaxX = Settings.LookAheadMax * ViewHalf.X;
		const float MaxY = Settings.LookAheadMax * ViewHalf.Y;
		WantedLead.X = FMath::Abs(Target.Velocity.X) > LeadMinSpeed ? FMath::Clamp(Target.Velocity.X * Settings.LookAheadTime, -MaxX, MaxX) : Lead.X;
		WantedLead.Y = Settings.bLookAheadVertical ? FMath::Clamp(Target.Velocity.Y * Settings.LookAheadTime, -MaxY, MaxY) : 0.f;
	}
	Lead = FMath::Lerp(Lead, WantedLead, DampAlpha(Settings.LookAheadSmoothing, DeltaTime));

	const float WantedFall = !Target.bGrounded && Target.Velocity.Y < -Settings.FallSpeed ? Settings.FallLookDown * ViewHalf.Y : 0.f;
	FallOffset = FMath::Lerp(FallOffset, WantedFall, DampAlpha(Settings.Damping.Y + 0.2f, DeltaTime));
}

void FOnyxCameraFraming::Reset(const FOnyxCameraSettings& Settings, const FOnyxCameraTarget& Target, const FVector2D& ViewHalf)
{
	Lead = FVector2D::ZeroVector;
	FallOffset = 0.f;
	GroundY = Target.Position.Y;
	Center = DesiredCenter(Settings, Target, ViewHalf);
}

FVector2D FOnyxCameraFraming::Update(const FOnyxCameraSettings& Settings, const FOnyxCameraTarget& Target, const FVector2D& ViewHalf, float DeltaTime)
{
	if (Target.bGrounded)
	{
		GroundY = Target.Position.Y;
	}
	UpdateLead(Settings, Target, ViewHalf, DeltaTime);

	const FVector2D Desired = DesiredCenter(Settings, Target, ViewHalf);
	const FVector2D DeadHalf = Settings.DeadZone * ViewHalf;
	const FVector2D SoftHalf(FMath::Max(Settings.SoftZone.X, Settings.DeadZone.X) * ViewHalf.X, FMath::Max(Settings.SoftZone.Y, Settings.DeadZone.Y) * ViewHalf.Y);

	Center.X = FollowAxis(Center.X, Desired.X, DeadHalf.X, SoftHalf.X, Settings.Damping.X, DeltaTime);
	if (Settings.VerticalFollow != EOnyxVerticalFollow::Locked)
	{
		// While airborne in WhenGrounded mode the desired height is the ground height, but the real
		// target must still stay inside the soft zone (long falls / tall jumps).
		Center.Y = FollowAxis(Center.Y, Desired.Y, DeadHalf.Y, SoftHalf.Y, Settings.Damping.Y, DeltaTime);
		if (Settings.VerticalFollow == EOnyxVerticalFollow::WhenGrounded && !Target.bGrounded)
		{
			const float TargetY = Target.Position.Y + Settings.TargetOffset.Y;
			const float Offset = TargetY - Center.Y;
			if (FMath::Abs(Offset) > SoftHalf.Y)
			{
				Center.Y = TargetY - FMath::Sign(Offset) * SoftHalf.Y;
			}
		}
	}
	return Center;
}

// --- Confiner ------------------------------------------------------------------------------

FVector2D FOnyxCameraConfiner::Clamp(const FVector2D& Center, const FVector2D& ViewHalf, const FBox2D& Area)
{
	if (!Area.bIsValid)
	{
		return Center;
	}
	auto ClampAxis = [](float Value, float Min, float Max, float Half)
	{
		return (Max - Min) <= Half * 2.f ? (Min + Max) * 0.5f : FMath::Clamp(Value, Min + Half, Max - Half);
	};
	return FVector2D(ClampAxis(Center.X, Area.Min.X, Area.Max.X, ViewHalf.X), ClampAxis(Center.Y, Area.Min.Y, Area.Max.Y, ViewHalf.Y));
}

// --- Shake --------------------------------------------------------------------------------

FVector FOnyxCameraShake::Update(const FOnyxCameraSettings& Settings, float DeltaTime)
{
	Trauma = FMath::Max(Trauma - Settings.TraumaDecay * DeltaTime, 0.f);
	Time += DeltaTime * Settings.ShakeFrequency;
	const float Strength = Trauma * Trauma;
	return FVector(
		Settings.ShakeMaxOffset * Strength * FMath::PerlinNoise1D(Time),
		Settings.ShakeMaxOffset * Strength * FMath::PerlinNoise1D(Time + 100.f),
		Settings.ShakeMaxRoll * Strength * FMath::PerlinNoise1D(Time + 200.f));
}
