// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OnyxCameraTypes.generated.h"

UENUM(BlueprintType)
enum class EOnyxCameraProjection : uint8
{
	/** Narrow field of view from far away: looks flat but keeps depth for parallax and shadows. */
	Perspective,
	/** True 2D projection, no perspective at all. */
	Orthographic
};

UENUM(BlueprintType)
enum class EOnyxVerticalFollow : uint8
{
	/** Follow up and down like left and right. */
	Always,
	/** Classic platformer: ignore jumps; re-frame on landing or when the target leaves the soft zone. */
	WhenGrounded,
	/** Never move vertically (auto-runners, single-screen rooms). */
	Locked
};

UENUM(BlueprintType)
enum class EOnyxCameraConfine : uint8
{
	None,
	/** Stay inside the Camera Zone the target is in (zones with Confine on). */
	Zones,
	/** Stay inside the level (the area covered by tile maps with Onyx Tile Collision). */
	Level,
	/** Zone when inside one, otherwise the level. */
	ZonesThenLevel
};

/**
 * Everything that defines how the camera frames the target (Cinemachine-style: framing transposer,
 * confiner, impulse). Lives on the camera, in Camera Profile assets and in Camera Zones; blends smoothly.
 * Screen-relative values are fractions of the visible area, so they keep working when you zoom.
 */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxCameraSettings
{
	GENERATED_BODY()

	// --- Lens ----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	EOnyxCameraProjection Projection = EOnyxCameraProjection::Perspective;

	/** How much of the world is visible vertically, in cm. This is the zoom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "100", UIMin = "300", UIMax = "6000"))
	float ViewHeight = 1300.f;

	/** Horizontal field of view for the perspective projection (small = flatter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "5", ClampMax = "120", EditCondition = "Projection == EOnyxCameraProjection::Perspective"))
	float FieldOfView = 24.f;

	// --- Framing --------------------------------------------------------------------------

	/** Offset added to the target position, in cm (e.g. aim at the chest instead of the feet). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing")
	FVector2D TargetOffset = FVector2D(0.f, 60.f);

	/** Where the target sits on screen, as a fraction of the view from the centre (+Y = target below centre, more room above). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (UIMin = "-0.45", UIMax = "0.45"))
	FVector2D ScreenOffset = FVector2D(0.f, 0.12f);

	/** Area (fraction of the view) the target can move in without the camera moving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (UIMin = "0", UIMax = "1"))
	FVector2D DeadZone = FVector2D(0.08f, 0.12f);

	/** The camera catches up smoothly inside this area and hard-limits the target to it (fraction of the view). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (UIMin = "0", UIMax = "1"))
	FVector2D SoftZone = FVector2D(0.6f, 0.7f);

	/** Seconds to catch up horizontally / vertically (0 = instant). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (ClampMin = "0", UIMax = "2"))
	FVector2D Damping = FVector2D(0.25f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing")
	EOnyxVerticalFollow VerticalFollow = EOnyxVerticalFollow::WhenGrounded;

	// --- Look-ahead ------------------------------------------------------------------------

	/** Lead the target by where it will be this many seconds from now (0 = off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look Ahead", meta = (ClampMin = "0", UIMax = "1.5"))
	float LookAheadTime = 0.4f;

	/** Largest lead, as a fraction of the half view width. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look Ahead", meta = (ClampMin = "0", ClampMax = "1"))
	float LookAheadMax = 0.35f;

	/** Seconds to ease into / out of the lead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look Ahead", meta = (ClampMin = "0", UIMax = "3"))
	float LookAheadSmoothing = 0.6f;

	/** Also lead vertically (useful for vertical levels). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look Ahead")
	bool bLookAheadVertical = false;

	/** When falling faster than FallSpeed, look further down (fraction of the half view height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look Ahead", meta = (ClampMin = "0", ClampMax = "1"))
	float FallLookDown = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look Ahead", meta = (ClampMin = "0"))
	float FallSpeed = 900.f;

	// --- Confiner & pixels -------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Confiner")
	EOnyxCameraConfine Confine = EOnyxCameraConfine::ZonesThenLevel;

	/** Extra space allowed outside the confining area, in cm (e.g. to show a bit below the level). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Confiner")
	float ConfinePadding = 0.f;

	/** Snap the camera to the art's pixel grid so pixel art never shimmers (size of one texture pixel in cm; 0 = off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixels", meta = (ClampMin = "0"))
	float PixelSnap = 0.f;

	/**
	 * Pixel-perfect pixel art: every art pixel covers the same whole number of screen pixels, the camera moves one
	 * screen pixel at a time and the projection is orthographic. No uneven pixels, no shimmer, no lines between
	 * tiles. View Height becomes a target: the closest zoom that keeps pixels whole is used.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixels")
	bool bPixelPerfect = true;

	/** Size of one art pixel in the world, in cm (the "Centimeters Per Pixel" the art was imported with). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixels", meta = (ClampMin = "0.01", EditCondition = "bPixelPerfect"))
	float ArtPixelSize = 4.f;

	// --- Shake ---------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shake", meta = (ClampMin = "0"))
	float ShakeMaxOffset = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shake", meta = (ClampMin = "0"))
	float ShakeMaxRoll = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shake", meta = (ClampMin = "0.1"))
	float ShakeFrequency = 25.f;

	/** Trauma lost per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shake", meta = (ClampMin = "0"))
	float TraumaDecay = 1.6f;

	/** Blend between two settings (numbers interpolate, choices switch half way). */
	static FOnyxCameraSettings Blend(const FOnyxCameraSettings& A, const FOnyxCameraSettings& B, float Alpha);
};

/** Reusable camera setup (like a Cinemachine virtual camera): assign it to the camera or to Camera Zones. */
UCLASS(BlueprintType, meta = (DisplayName = "Onyx Camera Profile"))
class ONYX2DRUNTIME_API UOnyxCameraProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ShowOnlyInnerProperties))
	FOnyxCameraSettings Settings;
};
