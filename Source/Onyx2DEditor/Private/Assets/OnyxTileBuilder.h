// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Assets/OnyxNormalMaps.h"

class APaperTileMapActor;
class UPaperTileMap;
class UPaperTileSet;
class UTexture2D;

struct FOnyxTileSetSpec
{
	TWeakObjectPtr<UTexture2D> Texture;
	FIntPoint TileSize = FIntPoint(16, 16);
	int32 Margin = 0;
	int32 Spacing = 0;
	/** Classify tiles automatically: mostly opaque = Solid, mostly transparent = None (see AutoAssignCollision). */
	bool bSolidCollision = true;
	/** Normal map made per tile from the art, so tiles join seamlessly and 2D lights shade them in relief. */
	bool bGenerateNormalMap = true;
	FOnyxNormalMapSettings NormalMap;
	/** Extrude every tile's edge pixels so no lines show between tiles while the camera moves. */
	bool bPadTiles = true;
	FString Name = TEXT("TS_Tiles");
	FString Folder = TEXT("/Game/Tiles");
};

struct FOnyxTileMapSpec
{
	TWeakObjectPtr<UPaperTileSet> TileSet;
	int32 Width = 60;
	int32 Height = 24;
	/** World size of one texture pixel in cm. */
	float CentimetersPerPixel = 4.f;
	/** Fills the bottom rows with GroundTile so there is something to stand on. */
	bool bFillGround = true;
	int32 GroundRows = 2;
	int32 GroundTile = 0;
	bool bPlaceInLevel = true;
	FString Name = TEXT("TM_Level");
	FString Folder = TEXT("/Game/Tiles");
};

/** Creates Paper2D tile sets and tile maps set up for Onyx2D (pixel art, lit, with collision). */
class FOnyxTileBuilder
{
public:
	static UPaperTileSet* CreateTileSet(const FOnyxTileSetSpec& Spec, FText& OutError);
	static UPaperTileMap* CreateTileMap(const FOnyxTileMapSpec& Spec, FText& OutError);

	/**
	 * Sets every tile's collision type from its pixels: tiles at least SolidCoverage opaque become Solid,
	 * the rest None (plants, coins, signs...). Returns how many tiles became solid.
	 */
	static int32 AutoAssignCollision(UPaperTileSet* TileSet, float SolidCoverage = 0.7f);

	/** Makes sure a tile map actor has a Onyx Tile Collision component (adds one if missing). */
	static void EnsureTileCollision(APaperTileMapActor* Actor);
};
