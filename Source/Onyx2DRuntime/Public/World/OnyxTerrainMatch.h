// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UPaperTileSet;

/**
 * Picks a tile set's terrain tiles: which tile fits a cell, from which of its neighbours have the terrain. The Level
 * Painter's terrain brush and the Level Builder share these rules, so a terrain looks the same painted or typed.
 */
struct ONYX2DRUNTIME_API FOnyxTerrainMatch
{
	/** Corner bits, in Paper2D's order (counter-clockwise from top-left). */
	enum ECorner : uint8
	{
		TopLeft = 1 << 0,
		BottomLeft = 1 << 1,
		BottomRight = 1 << 2,
		TopRight = 1 << 3,
		All = TopLeft | BottomLeft | BottomRight | TopRight
	};

	/** Which corners of the tile belong to the terrain (0: the tile isn't part of it). */
	static uint8 GetCorners(const UPaperTileSet* TileSet, int32 TileIndex, int32 Terrain);

	/** The terrain's tile whose corners are closest to WantedCorners (INDEX_NONE when the tile set has no such terrain). */
	static int32 FindTile(const UPaperTileSet* TileSet, int32 Terrain, uint8 WantedCorners);

	/** The corners a cell should have, from its 3 x 3 neighbourhood (true: that cell has the terrain; [1][1] is the cell). */
	static uint8 CornersFromNeighbours(const bool (&Neighbours)[3][3]);
};
