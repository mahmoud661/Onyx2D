// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UPaperTileSet;

/**
 * Auto-tiling terrains stored in Paper2D's own tile set data (Terrains + each tile's corner membership),
 * the Wang-corner model Tiled uses: every tile says which of its four corners belong to the terrain.
 * Painting a terrain marks cells; each cell then gets the tile whose corners match its neighbours, so
 * edges, outer corners and inner corners appear by themselves.
 */
class FOnyxTerrains
{
public:
	/** Corner bits, in Paper2D's order (counter-clockwise from top-left). */
	enum ECorner : uint8
	{
		TopLeft = 1 << 0,
		BottomLeft = 1 << 1,
		BottomRight = 1 << 2,
		TopRight = 1 << 3,
		All = TopLeft | BottomLeft | BottomRight | TopRight
	};

	static int32 GetCount(const UPaperTileSet* TileSet);
	static FString GetName(const UPaperTileSet* TileSet, int32 Terrain);

	/**
	 * Makes a terrain from a 3 x 3 template laid out the usual way: outer corners in the corners, edges on the
	 * sides, fill in the middle. The selection is 3 x 3, or 5 x 5 for sheets that leave a gap between tiles.
	 * Returns the new terrain's index (INDEX_NONE for any other size).
	 */
	static int32 CreateFromBlock(UPaperTileSet* TileSet, const FIntPoint& BlockMin, const FIntPoint& BlockSize, const FString& Name);

	/**
	 * Adds inner (concave) corners from a 2 x 2 block arranged around a hole: top-left tile is missing its
	 * bottom-right corner, top-right its bottom-left, and so on (3 x 3 selection for gapped sheets).
	 */
	static bool SetInnerCorners(UPaperTileSet* TileSet, int32 Terrain, const FIntPoint& BlockMin, const FIntPoint& BlockSize);

	/** Does this tile belong to the terrain (any corner)? */
	static bool IsTerrainTile(const UPaperTileSet* TileSet, int32 TileIndex, int32 Terrain);

	/** Corner bits of a tile for a terrain. */
	static uint8 GetCorners(const UPaperTileSet* TileSet, int32 TileIndex, int32 Terrain);

	/** The terrain tile that best matches the wanted corners (exact match first). INDEX_NONE if the terrain is empty. */
	static int32 FindTile(const UPaperTileSet* TileSet, int32 Terrain, uint8 WantedCorners);

	/**
	 * Corners a cell should have from its eight neighbours (true = also painted with the terrain): a corner is
	 * inside when the three cells around it are. Index neighbours as [dy + 1][dx + 1].
	 */
	static uint8 CornersFromNeighbours(const bool (&Neighbours)[3][3]);

	/** Distance between template tiles for a selection: 1 for a packed template, 2 for a gapped one (0 = not a template). */
	static int32 GetTemplateStride(const FIntPoint& SelectionSize, int32 TemplateTiles);

private:
	static void SetCorners(UPaperTileSet* TileSet, int32 TileIndex, int32 Terrain, uint8 Corners);
};
