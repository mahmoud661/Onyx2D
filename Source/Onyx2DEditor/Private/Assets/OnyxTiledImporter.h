// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UPaperTileMap;
class UPaperTileSet;

/** What to import from a Tiled map and where to put it. */
struct FOnyxTiledImportSpec
{
	/** A Tiled map saved as JSON (.json / .tmj). */
	FString FilePath;
	/** Tile set made from the same sheet as the map's (first) Tiled tile set. */
	TWeakObjectPtr<UPaperTileSet> TileSet;
	float CentimetersPerPixel = 4.f;
	bool bPlaceInLevel = true;
	FString Name = TEXT("TM_Imported");
	FString Folder = TEXT("/Game/Tiles");
};

/**
 * Imports Tiled (mapeditor.org) JSON maps into Paper2D tile maps the Onyx way: every tile layer becomes a
 * Paper2D layer in the same order (names, visibility, flipped / rotated tiles kept), and the map is placed with
 * Onyx Tile Collision. Collision types come from the tile set (set them in the Level Painter).
 */
class FOnyxTiledImporter
{
public:
	static UPaperTileMap* Import(const FOnyxTiledImportSpec& Spec, FText& OutError);
};
