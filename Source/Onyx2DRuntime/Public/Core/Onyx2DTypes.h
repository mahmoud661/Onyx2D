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

/**
 * How a level is played. A level picks its own (World Settings > Onyx Level), else the project's (Project Settings >
 * Onyx2D); characters follow their level's unless their movement says otherwise.
 */
UENUM(BlueprintType)
enum class EOnyxGameStyle : uint8
{
	/** Seen from the side: running and jumping with gravity, facing left or right (a platformer). */
	SideView UMETA(DisplayName = "Side View"),
	/**
	 * Seen from above: walking in any direction over the ground, no gravity, facing up, down and to the sides. Up the
	 * screen is north, and whoever stands lower on the screen draws in front.
	 */
	TopDown UMETA(DisplayName = "Top Down"),
};

/**
 * Which way a top-down character looks, as its clips are drawn: Down (towards the viewer), Up (away), Side, and the
 * diagonals between. Side ones are drawn looking right and flipped for the left (its Facing). None: a side-view character.
 */
UENUM(BlueprintType)
enum class EOnyxDirection : uint8
{
	None,
	Down,
	DownSide UMETA(DisplayName = "Down Side"),
	Side,
	UpSide UMETA(DisplayName = "Up Side"),
	Up,
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
	Swim,
	/** Moving on the ground slower than Walk Speed (Onyx Movement > Ground). */
	Walk,
	/** Moving while crouched (Crouch Speed Scale above 0). */
	CrouchWalk,
	/** A slide along the ground, low (Onyx Movement > Slide). */
	Slide,
	/** A dash started on the ground. */
	Roll,
	/** Hanging from a ledge by the hands (Onyx Movement > Ledge). */
	LedgeHang,
	/** Pulling up onto a ledge. */
	LedgeClimb
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
	/** The sprite materials' hit flash, through custom primitive data: the colour (4 values from here) and its amount. */
	constexpr int32 FlashColorDataIndex = 2;
	constexpr int32 FlashDataIndex = 6;
	/** Custom primitive data: how far a sprite has dissolved, art pixel by art pixel (0 whole, 1 gone). */
	constexpr int32 DissolveDataIndex = 8;
	/** Custom primitive data: how much a sprite is kept from washing out under light (Onyx Character's Light Guard). */
	constexpr int32 LightGuardDataIndex = 9;
	/**
	 * Custom primitive data: glowing pixels. How strongly its pixels brighter than the threshold shine on their own, lit
	 * or not (0 none), and the threshold (0 means the default, 0.75 of full brightness).
	 */
	constexpr int32 GlowPixelsDataIndex = 10;
	constexpr int32 GlowThresholdDataIndex = 11;
	/** Custom primitive data: how see-through a sprite is, its art pixels dropping out in an even pattern (0 none, 1 all). */
	constexpr int32 SeeThroughDataIndex = 12;
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
	/**
	 * Top down, props, items, characters and the player are drawn in the order of their feet: lower on the screen is in
	 * front. Each cm lower brings a sprite this many cm nearer, within the range either side of the middle; tiles stay
	 * behind them all and the In Front layer in front. The orthographic camera tells depths apart only about 0.0013 cm
	 * apart at the 2D plane (its planes run from 1 cm to 220 m away): two sprites nearer than that are one surface to it,
	 * and where they overlap one draws its clear pixels (black) over the other. So one art pixel of 4 cm is 0.12 cm here.
	 */
	constexpr float TopDownSortScale = 0.03f;
	constexpr float TopDownSortRange = 300.f;
	/** Top down, two at the same height keep their slots' order this far apart (two of the camera's depth steps). */
	constexpr float TopDownSlotStep = 0.0025f;
	/**
	 * Top down, sprites ordered by Translucency Sort Priority (inside one actor, or within a flat layer such as Background:
	 * water over the ground, a deck over the water) are this far apart: well clear of the camera's depth steps, so a flat
	 * layer's pictures never tie (a tie draws clear pixels black).
	 */
	constexpr float TopDownComponentStep = 0.05f;

	inline float FacingSign(EOnyxFacing Facing)
	{
		return Facing == EOnyxFacing::Right ? 1.f : -1.f;
	}

	inline FVector FacingVector(EOnyxFacing Facing)
	{
		return FVector(FacingSign(Facing), 0.f, 0.f);
	}

	/** The middle of a direction, degrees up from the side (Down -90, Side 0, Up 90). */
	inline float DirectionAngle(EOnyxDirection Direction)
	{
		switch (Direction)
		{
		case EOnyxDirection::Down:     return -90.f;
		case EOnyxDirection::DownSide: return -45.f;
		case EOnyxDirection::UpSide:   return 45.f;
		case EOnyxDirection::Up:       return 90.f;
		default:                       return 0.f;
		}
	}

	/**
	 * The direction a heading (screen X right, Y up) looks, of the eight. Near the line between two it keeps Current, so
	 * walking along a diagonal doesn't flick between them.
	 */
	inline EOnyxDirection DirectionFromHeading(const FVector2D& Heading, EOnyxDirection Current)
	{
		if (Heading.IsNearlyZero(0.01f))
		{
			return Current == EOnyxDirection::None ? EOnyxDirection::Down : Current;
		}
		const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Heading.Y, FMath::Abs(Heading.X)));
		constexpr float Keep = 22.5f + 8.f;
		if (Current != EOnyxDirection::None && FMath::Abs(Angle - DirectionAngle(Current)) <= Keep)
		{
			return Current;
		}
		return Angle < -67.5f ? EOnyxDirection::Down : Angle < -22.5f ? EOnyxDirection::DownSide : Angle <= 22.5f ? EOnyxDirection::Side
			: Angle <= 67.5f ? EOnyxDirection::UpSide : EOnyxDirection::Up;
	}

	/** A direction as a unit heading (screen X right, Y up); its side ones point the way Facing says. */
	inline FVector2D HeadingFromDirection(EOnyxDirection Direction, EOnyxFacing Facing)
	{
		const float Radians = FMath::DegreesToRadians(DirectionAngle(Direction));
		return FVector2D(FMath::Cos(Radians) * FacingSign(Facing), FMath::Sin(Radians));
	}
}

/** Engine-wide gameplay notifications. Lets the UI react without the runtime depending on it. */
struct ONYX2DRUNTIME_API FOnyxGameDelegates
{
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPauseRequested, APlayerController* /*Player*/);

	/** Broadcast when a player presses Pause. The UI module opens or closes the pause menu. */
	static FOnPauseRequested OnPauseRequested;
};
