// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UPaperTileSet;
class UTexture2D;

/**
 * Stops lines appearing between tiles. When the camera sits between pixels, the GPU can sample the texel just
 * outside a tile: a neighbour, or the empty gap of a spaced sheet. Padding gives every tile a border of copies of
 * its own edge pixels (extrusion), so those samples land on the right colour. Tile indices, collision and
 * terrains are unchanged; the tile set just reads the padded sheet with margin / spacing.
 */
class FOnyxTilePadding
{
public:
	static constexpr int32 DefaultPadding = 2;

	/** Is the tile set already reading a padded sheet? */
	static bool IsPadded(const UPaperTileSet* TileSet);

	/** Pads the tile set's sheet (and its normal map) and points the tile set at them. Returns false if it can't. */
	static bool PadTileSet(UPaperTileSet* TileSet, int32 Padding = DefaultPadding);

private:
	/** Copies every tile of Source into a new layout with Padding extruded pixels around each tile. */
	static UTexture2D* PadTexture(UTexture2D* Source, const UPaperTileSet* Layout, int32 Padding, bool bNormalMap);
};
