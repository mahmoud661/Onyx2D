// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaperTileLayer.h"
#include "ScopedTransaction.h"
#include "World/OnyxTileCollision.h"

class APaperTileMapActor;
class UPaperTileMap;
class UPaperTileSet;
struct FOnyxPainterBrush;

/** A block of tiles picked from a tile set (1 x 1 for a single tile). Painted as a repeating pattern. */
struct FOnyxStamp
{
	TWeakObjectPtr<UPaperTileSet> TileSet;
	/** Top-left tile in the tile set grid. */
	FIntPoint Min = FIntPoint::ZeroValue;
	FIntPoint Size = FIntPoint(1, 1);

	/** Paint mirrored left-right / upside down (Paper2D tile flip flags, so collision and rendering follow). */
	bool bFlipX = false;
	bool bFlipY = false;
	/** Paint one cell at a time with a random tile from the block (natural-looking ground, grass, rocks). */
	bool bRandom = false;
	/** Changes every stroke so random painting doesn't repeat. */
	int32 RandomSeed = 0;

	bool IsValid() const { return TileSet.IsValid() && Size.X > 0 && Size.Y > 0; }
	/** Area one dab of the brush covers (1 x 1 in random mode). */
	FIntPoint GetPaintSize() const { return bRandom ? FIntPoint(1, 1) : Size; }
	/** The selected tile at an offset inside the block (wraps around), without flips or randomness. */
	FPaperTileInfo GetSourceTile(int32 X, int32 Y) const;
	/** The tile to paint at an offset inside the pattern, with flips / randomness applied (Cell seeds the randomness). */
	FPaperTileInfo GetTile(int32 X, int32 Y, const FIntPoint& Cell = FIntPoint::ZeroValue) const;
};

/** A placed object as seen by the painter. */
struct FOnyxPlacedObject
{
	AActor* Actor = nullptr;
	FIntPoint Cell = FIntPoint::ZeroValue;
	FName Layer;
};

/**
 * Edits a Paper2D tile map (tiles, layers) and the actors painted onto it (object layers).
 * Knows nothing about widgets: the Level Painter tab drives it.
 */
class FOnyxMapPainter
{
public:
	void SetTileMap(UPaperTileMap* InTileMap);
	UPaperTileMap* GetTileMap() const { return TileMap.Get(); }

	/** The level actor showing this map (needed to place objects). */
	APaperTileMapActor* FindMapActor() const;
	/** Adds a tile map actor for this map to the level (bottom two rows below Z = 0). */
	APaperTileMapActor* PlaceMapInLevel();

	// --- Grid --------------------------------------------------------------------------
	FIntPoint GetMapSize() const;
	FIntPoint GetTileSize() const;
	bool IsValidCell(const FIntPoint& Cell) const;
	bool WorldToCell(const FVector& Location, FIntPoint& OutCell) const;
	FVector CellToWorld(const FIntPoint& Cell) const;
	void Resize(const FIntPoint& NewSize);

	// --- Tile layers -----------------------------------------------------------------------
	int32 GetLayer() const { return Layer; }
	void SetLayer(int32 InLayer);
	int32 GetLayerCount() const;
	FText GetLayerName(int32 LayerIndex) const;
	bool GetLayerCollides(int32 LayerIndex) const;
	FPaperTileInfo GetCell(const FIntPoint& Cell, int32 LayerIndex) const;
	void AddLayer();
	void RenameLayer(int32 LayerIndex, const FText& Name);
	void SetLayerCollides(int32 LayerIndex, bool bCollides);
	/** Moves a layer towards the front (-1) or the back (+1). */
	void MoveLayer(int32 LayerIndex, int32 Direction);
	void DeleteLayer(int32 LayerIndex);

	// --- Tile painting (active layer) ---------------------------------------------------
	/** Groups every change until EndStroke into one undo step. */
	void BeginStroke(const FText& Description);
	void EndStroke();
	bool PaintTile(const FIntPoint& Cell, const FPaperTileInfo& Tile);
	/** Paints the whole stamp with its top-left on Cell. */
	void PaintStamp(const FIntPoint& Cell, const FOnyxStamp& Stamp);
	/**
	 * Paints (or erases) a terrain on a cell and re-picks the tiles around it so edges and corners match
	 * (see FOnyxTerrains). Works on the active layer.
	 */
	void PaintTerrain(const FIntPoint& Cell, UPaperTileSet* TileSet, int32 Terrain, bool bErase);
	/** Fills the connected area with the stamp pattern (anchored at Start), or clears it. */
	int32 Fill(const FIntPoint& Start, const FOnyxStamp& Stamp, bool bErase);
	/** Pushes tile changes to the level (render data and collision). */
	void FlushChanges();

	// --- Collision ---------------------------------------------------------------------------
	/** Effective collision of a cell (colliding layers only). */
	EOnyxTileCollision GetCellCollision(const FIntPoint& Cell) const;
	/** Sets the collision type of every tile in the stamp (on its tile set). */
	void SetTileCollision(const FOnyxStamp& Stamp, EOnyxTileCollision Type);
	/** Re-classifies a whole tile set from its pixels (opaque = Solid, transparent = None). */
	void AutoAssignCollision(UPaperTileSet* TileSet);
	/** Rebuilds the Onyx Tile Collision of every map in the level (after tiles or types change). */
	static void RebuildLevelCollision();

	// --- Object layers ---------------------------------------------------------------------
	AActor* PlaceObject(const FIntPoint& Cell, const FOnyxPainterBrush& Brush, FName ObjectLayer);
	bool EraseObject(const FIntPoint& Cell, FName ObjectLayer);
	AActor* FindObject(const FIntPoint& Cell, FName ObjectLayer) const;
	bool MoveObject(AActor* Actor, const FIntPoint& Cell);
	void ForEachObject(TFunctionRef<void(const FOnyxPlacedObject&)> Visit) const;
	static FName GetObjectLayer(const AActor* Actor);
	/** Depth (Y) objects of an object layer are placed at: behind the tiles, on the gameplay plane or in front. */
	static float GetObjectLayerDepth(FName ObjectLayer);

private:
	UPaperTileLayer* GetTileLayer(int32 LayerIndex) const;
	void TouchLayer(UPaperTileLayer* TileLayer);
	void ChangeMapStructure(const FText& Description, TFunctionRef<void(UPaperTileMap&)> Change);

	TWeakObjectPtr<UPaperTileMap> TileMap;
	int32 Layer = 0;
	TUniquePtr<FScopedTransaction> Stroke;
	TSet<TWeakObjectPtr<UPaperTileLayer>> TouchedLayers;
	bool bPendingFlush = false;
};
