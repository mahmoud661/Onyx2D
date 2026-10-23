// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/OnyxCameraTypes.h"

/** What the camera follows this frame, in the gameplay plane (X right, Y up). */
struct FOnyxCameraTarget
{
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Velocity = FVector2D::ZeroVector;
	bool bGrounded = true;
	/** Seen from above: up and down are walking, not jumping and falling (followed and looked ahead like left and right). */
	bool bTopDown = false;
};

/**
 * Framing transposer: dead zone, soft zone, damping, screen offset, look-ahead, vertical follow modes.
 * Pure math on 2D points, no engine state: easy to reason about and to test.
 */
class ONYX2DRUNTIME_API FOnyxCameraFraming
{
public:
	/** Puts the camera exactly where the settings want it (no smoothing). */
	void Reset(const FOnyxCameraSettings& Settings, const FOnyxCameraTarget& Target, const FVector2D& ViewHalf);

	/** Advances the camera centre by DeltaTime. */
	FVector2D Update(const FOnyxCameraSettings& Settings, const FOnyxCameraTarget& Target, const FVector2D& ViewHalf, float DeltaTime);

	FVector2D GetCenter() const { return Center; }

private:
	FVector2D DesiredCenter(const FOnyxCameraSettings& Settings, const FOnyxCameraTarget& Target, const FVector2D& ViewHalf) const;
	void UpdateLead(const FOnyxCameraSettings& Settings, const FOnyxCameraTarget& Target, const FVector2D& ViewHalf, float DeltaTime);

	FVector2D Center = FVector2D::ZeroVector;
	FVector2D Lead = FVector2D::ZeroVector;
	float FallOffset = 0.f;
	/** Height the camera frames while the target is airborne (WhenGrounded mode). */
	float GroundY = 0.f;
};

/** Keeps the view rectangle inside an area; centres on axes where the area is smaller than the view. */
struct ONYX2DRUNTIME_API FOnyxCameraConfiner
{
	static FVector2D Clamp(const FVector2D& Center, const FVector2D& ViewHalf, const FBox2D& Area);
};

/** Trauma-based shake (offset grows with trauma squared, Perlin noise, decays over time). */
class ONYX2DRUNTIME_API FOnyxCameraShake
{
public:
	void AddTrauma(float Amount) { Trauma = FMath::Clamp(Trauma + Amount, 0.f, 1.f); }
	float GetTrauma() const { return Trauma; }

	/** Advances the shake and returns the offset (X, Y in the plane) and roll in degrees. */
	FVector Update(const FOnyxCameraSettings& Settings, float DeltaTime);

private:
	float Trauma = 0.f;
	float Time = 0.f;
};
