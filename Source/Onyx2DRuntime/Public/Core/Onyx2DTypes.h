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

/** What a character is doing right now. The Moveset component plays the clip of the same name. */
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

/**
 * Where a sprite draws, back to front. Everything 2D lives on the gameplay plane (Y = 0); the draw order decides what is
 * in front, so two sprites never fight over the same pixels. Onyx2D picks the layer by itself (tile maps, props, items,
 * characters, the player); an Onyx Draw Order component overrides it.
 */
UENUM(BlueprintType)
enum class EOnyxDrawLayer : uint8
{
	/** Scenery behind the ground: houses, trees, hills (the Level Painter's Background layer). */
	BehindTiles UMETA(DisplayName = "Behind Tiles"),
	/** Tile maps. */
	Tiles,
	/** Things on the gameplay plane that don't move much: signs, switches, crates, decorations. */
	Props,
	/** Pickups and effects. */
	Items,
	/** Enemies and other characters. */
	Characters,
	/** The player's character: always in front of the rest of the gameplay. */
	Player,
	/** Decoration in front of everything (the Level Painter's Decor layer): foreground bushes, vines. */
	InFront UMETA(DisplayName = "In Front"),
};

namespace Onyx2D
{
	/** Depth (Y) of tile maps: the gameplay plane, like everything 2D. The draw order puts them behind the gameplay. */
	constexpr float TileMapDepth = 0.f;

	/**
	 * The draw order reaches the sprite materials through this custom primitive data slot: a depth bias in cm, applied
	 * along the view direction so the sprite never moves on screen (Onyx Draw Order subsystem).
	 */
	constexpr int32 DrawOrderDataIndex = 7;
	/** Depth between two draw layers, in cm. */
	constexpr float DrawLayerStep = 2.f;
	/** Actors in one layer get their own slot each: this many, this far apart (in cm). */
	constexpr int32 DrawOrderSlots = 48;
	constexpr float DrawOrderSlotStep = 0.03f;
	/** Sprites inside one actor are ordered by their Translucency Sort Priority (clamped), this far apart. */
	constexpr int32 DrawOrderMaxComponentPriority = 3;
	constexpr float DrawOrderComponentStep = 0.004f;
	/** The layer that sits at zero bias: sprites with materials that ignore the draw order draw as props. */
	constexpr EOnyxDrawLayer DrawOrderBaseLayer = EOnyxDrawLayer::Props;

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
