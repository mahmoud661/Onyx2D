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
	Hazard
};

/** Reads and writes the collision type of tiles in a Paper2D tile set. */
struct ONYX2DRUNTIME_API FOnyxTileCollision
{
	static EOnyxTileCollision Get(const UPaperTileSet* TileSet, int32 TileIndex);

	/** Effective type of a map cell: every layer marked as colliding counts, Solid > One-Way > Hazard > None. */
	static EOnyxTileCollision ResolveCell(const UPaperTileMap* Map, int32 X, int32 Y);

	/** Display name and color used by the editor tools. */
	static FText GetDisplayName(EOnyxTileCollision Type);
	static FLinearColor GetColor(EOnyxTileCollision Type);

#if WITH_EDITOR
	/** Sets the type (and the matching Paper2D collision shape). Call TileSet->Modify() first. */
	static void Set(UPaperTileSet* TileSet, int32 TileIndex, EOnyxTileCollision Type);
#endif
};
