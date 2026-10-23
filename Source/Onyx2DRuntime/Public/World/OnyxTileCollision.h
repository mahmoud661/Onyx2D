// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnyxTileCollision.generated.h"

class UPaperTileMap;
class UPaperTileSet;

/**
 * How a tile behaves physically (like a Unity Tile's Collider Type, plus platformer types).
 * Stored per tile in the tile set (FPaperTileMetadata::UserDataName), so every map using the set agrees.
 */
UENUM(BlueprintType)
enum class EOnyxTileCollision : uint8
{
	/** Decoration: characters pass through. */
	None,
	/** Full block: ground, walls, ceilings. */
	Solid,
	/** Platform: stand on its top, jump up through it, drop down with Down + Jump. */
	OneWay,
	/** Spikes / lava: hurts on touch, doesn't block. */
	Hazard,
	/**
	 * Blocks only across some of the tile's sides (EOnyxTileEdge): from above, a cliff's rim, a fence, a thin wall
	 * between two cells; walking along it is free.
	 */
	Edge,
	/**
	 * A 45 degree slope rising to the right: solid below the line from the tile's bottom-left corner to its top-right.
	 * Slope tiles in a diagonal row become one smooth ramp (Solid Outset raises it like every other edge).
	 */
	SlopeUp UMETA(DisplayName = "Slope Up"),
	/** A 45 degree slope going down to the right: solid below the line from the tile's top-left corner to its bottom-right. */
	SlopeDown UMETA(DisplayName = "Slope Down")
};

/** Sides of a tile, for Edge tiles: north is the top of the map. */
namespace EOnyxTileEdge
{
	enum : uint8
	{
		None = 0,
		North = 1 << 0,
		East = 1 << 1,
		South = 1 << 2,
		West = 1 << 3,
		All = North | East | South | West
	};
}

/** Reads and writes the collision type of tiles in a Paper2D tile set. */
struct ONYX2DRUNTIME_API FOnyxTileCollision
{
	static EOnyxTileCollision Get(const UPaperTileSet* TileSet, int32 TileIndex);

	/** Effective type of a map cell: every layer marked as colliding counts, Solid > Slope > One-Way > Hazard > Edge > None. */
	static EOnyxTileCollision ResolveCell(const UPaperTileMap* Map, int32 X, int32 Y);

	/** The sides an Edge tile blocks (EOnyxTileEdge bits; none for other types). */
	static uint8 GetEdges(const UPaperTileSet* TileSet, int32 TileIndex);

	/** The sides blocked in a map cell: every colliding layer's Edge tiles together. */
	static uint8 ResolveCellEdges(const UPaperTileMap* Map, int32 X, int32 Y);

	/** Display name and color used by the editor tools. */
	static FText GetDisplayName(EOnyxTileCollision Type);
	static FLinearColor GetColor(EOnyxTileCollision Type);

#if WITH_EDITOR
	/** Sets the type (and the matching Paper2D collision shape); Edge blocks every side. Call TileSet->Modify() first. */
	static void Set(UPaperTileSet* TileSet, int32 TileIndex, EOnyxTileCollision Type);

	/** Makes the tile an Edge tile blocking these sides (EOnyxTileEdge bits; none: no collision). Call TileSet->Modify() first. */
	static void SetEdges(UPaperTileSet* TileSet, int32 TileIndex, uint8 Edges);
#endif
};
