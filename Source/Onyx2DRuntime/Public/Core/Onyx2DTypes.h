// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Onyx2DTypes.generated.h"

class APlayerController;

/**
 * Onyx2D conventions
 *  - Gameplay happens on the XZ plane: +X is right, +Z is up, Y is depth.
 *  - The camera sits at +Y looking toward -Y; backgrounds live at negative Y.
 */

UENUM(BlueprintType)
enum class EOnyxFacing : uint8
{
	Right,
	Left
};

UENUM(BlueprintType)
enum class EOnyxLocomotion : uint8
{
	Idle,
	Run,
	Jump,
	Fall,
	WallSlide,
	Dash,
	Crouch,
	/** In Onyx Water. */
	Swim
};

namespace Onyx2D
{
	/**
	 * Depth (Y) of tile maps: just behind the gameplay plane (Y = 0), so characters, pickups and effects never
	 * share a depth with background tiles (that z-fights: sprites flicker in and out).
	 */
	constexpr float TileMapDepth = -20.f;
	/** Scenery painted behind the tiles (houses, trees): tile edges and grass overlap its base. */
	constexpr float BackgroundObjectDepth = -40.f;
	/** Decoration painted in front of everything on the gameplay plane (foreground bushes, vines). */
	constexpr float ForegroundObjectDepth = 10.f;

	inline float FacingSign(EOnyxFacing Facing)
	{
		return Facing == EOnyxFacing::Right ? 1.f : -1.f;
	}

	inline FVector FacingVector(EOnyxFacing Facing)
	{
		return FVector(FacingSign(Facing), 0.f, 0.f);
	}
}

/** Engine-wide gameplay notifications. Lets the UI react without the runtime depending on it. */
struct ONYX2DRUNTIME_API FOnyxGameDelegates
{
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPauseRequested, APlayerController* /*Player*/);

	/** Broadcast when a player presses Pause. The UI module opens or closes the pause menu. */
	static FOnPauseRequested OnPauseRequested;
};
