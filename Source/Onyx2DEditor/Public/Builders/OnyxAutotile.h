// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Builders/OnyxPixelCanvas.h"

/**
 * Terrain drawn once as a block and composed into every edge and corner (Onyx Autotile). A block is a 32 x 48 picture of
 * 16 px tiles: at (0,0) a lone one-tile patch, at (16,0) the four inner corners, and below them a 2 x 2-tile patch whose
 * corners, edges and middle give the outer corners, the edges and the fill. Each cell's tile is put together from four
 * 8 x 8 quarters, each picked by its two side neighbours and the diagonal between them. Fills (16 x 16 each) vary the
 * plain inside. Pixels outside the terrain are clear, so terrains lay over one another.
 */
struct ONYX2DEDITOR_API FOnyxAutotile
{
	static constexpr int32 Tile = 16;
	static constexpr int32 Quarter = 8;

	/** Where quarter Q (0 top left, 1 top right, 2 bottom left, 3 bottom right) of a cell with this neighbour mask comes from. */
	static FIntPoint QuarterSource(uint8 Mask, int32 Q);

	/** True when the mask has every neighbour: the cell is plain fill. */
	static bool IsFill(uint8 Mask) { return Mask == 0xFF; }

	/** Draws the tile for a mask from a block into Target with its top-left at (X, Y), over what is there. */
	static void DrawTile(const FOnyxPixelCanvas& Block, uint8 Mask, FOnyxPixelCanvas& Target, int32 X, int32 Y);

	/** Draws a whole 16 x 16 fill tile into Target at (X, Y), over what is there. */
	static void DrawFill(const FOnyxPixelCanvas& Fill, FOnyxPixelCanvas& Target, int32 X, int32 Y);

	/**
	 * Lays a terrain over a picture of a whole map: each cell IsTerrain(X, Y) names gets its tile (plain ones a fill,
	 * picked by the cell), cells of 16 px from the picture's top-left.
	 */
	static void Paint(FOnyxPixelCanvas& Target, const FOnyxPixelCanvas& Block, const TArray<FOnyxPixelCanvas>& Fills, int32 Width, int32 Height,
		TFunctionRef<bool(int32, int32)> IsTerrain, int32 Seed = 0);

	/** The distinct tiles a block makes (the fills first) and, for each of the 256 masks, the tile that draws it. */
	static void BuildSet(const FOnyxPixelCanvas& Block, const TArray<FOnyxPixelCanvas>& Fills, TArray<FOnyxPixelCanvas>& OutTiles, TArray<uint8>& OutMaskToTile);

	/** Reads <Name>.png (the block) and <Name>_Fill.png (fills side by side) from a folder. False without the block. */
	static bool Load(const FString& Folder, const FString& Name, FOnyxPixelCanvas& OutBlock, TArray<FOnyxPixelCanvas>& OutFills);

	/** Cuts a strip into frames of a width. */
	static TArray<FOnyxPixelCanvas> SplitStrip(const FOnyxPixelCanvas& Strip, int32 FrameWidth);
};
