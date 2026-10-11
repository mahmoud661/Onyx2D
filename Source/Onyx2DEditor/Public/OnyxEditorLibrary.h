// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "World/OnyxTileCollision.h"
#include "OnyxEditorLibrary.generated.h"

class UBlueprint;
class UPaperFlipbook;
class UPaperSprite;
class UPaperTileMap;
class UPaperTileSet;
class UTexture2D;

/** The Onyx2D editor tools as functions, for Editor Utility Blueprints and Python scripts. */
UCLASS()
class UOnyxEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Builds a character from a sprite sheet with one animation per row.
	 * RowAnimations lists animation names top to bottom (e.g. Idle, Run, Jump, Fall, Attack1, Hurt, Death).
	 */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static UBlueprint* CreateCharacterFromSheet(UTexture2D* SpriteSheet, const FString& Name, const FString& Folder, FIntPoint FrameSize,
		const TArray<FName>& RowAnimations, bool bPlayer = true, float CentimetersPerPixel = 4.f, float FramesPerSecond = 10.f);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static UPaperTileSet* CreateTileSet(UTexture2D* Texture, FIntPoint TileSize, const FString& Name, const FString& Folder, bool bSolidCollision = true);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static UPaperTileMap* CreateTileMap(UPaperTileSet* TileSet, int32 Width, int32 Height, const FString& Name, const FString& Folder,
		float CentimetersPerPixel = 4.f, int32 GroundTile = 0, bool bPlaceInLevel = false);

	/** Cuts one cell (column, row) of a grid sheet into a sprite (pivot at the bottom centre). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static UPaperSprite* CreateSpriteFromCell(UTexture2D* Texture, FIntPoint CellSize, FIntPoint Cell, const FString& Name, const FString& Folder,
		float CentimetersPerPixel = 4.f);

	/** Cuts several cells of a grid sheet into an animated flipbook (e.g. a spinning coin or swaying grass). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static UPaperFlipbook* CreateFlipbookFromCells(UTexture2D* Texture, FIntPoint CellSize, const TArray<FIntPoint>& Cells, const FString& Name,
		const FString& Folder, float CentimetersPerPixel = 4.f, float FramesPerSecond = 6.f);

	/** Sets every tile's collision from its pixels (opaque = Solid, transparent = None). Returns the number of solid tiles. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static int32 AutoAssignTileCollision(UPaperTileSet* TileSet, float SolidCoverage = 0.7f);

	/**
	 * Builds a normal map for a sprite / tile sheet ("<Sheet>_N") and plugs it into every sprite and tile set
	 * using the sheet, so lights shade the art in relief. Returns how many assets received it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static int32 GenerateNormalMap(UTexture2D* Sheet, float BevelWidth = 2.5f, float BevelHeight = 2.f, float DetailHeight = 0.6f,
		int32 Smoothing = 1, float Strength = 1.f);

	/**
	 * Packs frame images (run-1, run-2...) into one strip texture, left to right in natural name order.
	 * Returns the strip; FrameSize receives the cell size.
	 */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static UTexture2D* PackFramesIntoStrip(const TArray<UTexture2D*>& Frames, const FString& Name, const FString& Folder, FIntPoint& FrameSize);

	/**
	 * Builds a character from one strip per animation (e.g. Idle, Run, Jump, Fall, Crouch, Hurt), as made by
	 * PackFramesIntoStrip. FramesPerSecond can override the speed of any animation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static UBlueprint* CreateCharacterFromStrips(const TMap<FName, UTexture2D*>& Strips, const FString& Name, const FString& Folder, FIntPoint FrameSize,
		bool bPlayer, float CentimetersPerPixel, const TMap<FName, float>& FramesPerSecond);

	/** Imports a Tiled JSON map (.json / .tmj) as a Paper2D tile map using TileSet, optionally placing it in the level. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static UPaperTileMap* ImportTiledMap(const FString& FilePath, UPaperTileSet* TileSet, const FString& Name, const FString& Folder,
		float CentimetersPerPixel = 4.f, bool bPlaceInLevel = true);

	/** Sets the collision type (None, Solid, OneWay, Hazard) of tiles by index. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static void SetTileCollision(UPaperTileSet* TileSet, const TArray<int32>& TileIndices, EOnyxTileCollision Collision);

	/**
	 * Makes an auto-tiling terrain from a 3 x 3 template in the tile sheet (5 x 5 when the sheet has gaps
	 * between tiles). Returns the terrain index, or -1.
	 */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static int32 CreateTerrain(UPaperTileSet* TileSet, FIntPoint TemplateMin, FIntPoint TemplateSize, const FString& Name);

	/**
	 * Fixes lines between tiles: pads the tile set's sheet (and normal map) with extruded edge pixels and points the
	 * tile set at the padded copy. Tile indices, collision and terrains are kept. Returns true when padded.
	 */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static bool PadTileSheet(UPaperTileSet* TileSet, int32 Padding = 2);

	/** Names of a tile set's terrains, in index order. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Editor")
	static TArray<FString> GetTerrainNames(const UPaperTileSet* TileSet);

	/** Adds a terrain's inner corners from a 2 x 2 block arranged around a hole (3 x 3 for gapped sheets). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static bool SetTerrainInnerCorners(UPaperTileSet* TileSet, int32 Terrain, FIntPoint BlockMin, FIntPoint BlockSize);

	/** Paints (or erases) a terrain on cells of a tile map layer; edges and corners are picked automatically. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static void PaintTerrain(UPaperTileMap* TileMap, int32 LayerIndex, const TArray<FIntPoint>& Cells, UPaperTileSet* TileSet, int32 Terrain, bool bErase = false);

	/**
	 * Imports art files / folders the way the Import tab does: tile sheets become padded, lit tile sets; animations
	 * (frame sequences, strips or sheets, named idle / run / jump...) become characters; other images become sprites or
	 * background textures. Returns the main asset made for each item.
	 */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static TArray<UObject*> ImportArt(const TArray<FString>& Paths, const FString& Folder = TEXT("/Game/Art"), float CentimetersPerPixel = 4.f);

	/** Adds game mode, player start, lights and tile collision to the open level when missing. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Editor")
	static TArray<FString> SetupCurrentLevelFor2D();
};
