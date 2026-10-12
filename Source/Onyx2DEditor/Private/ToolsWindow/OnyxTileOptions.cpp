// Copyright Onyx2D. All Rights Reserved.

#include "ToolsWindow/OnyxTileOptions.h"
#include "Builders/OnyxTileBuilder.h"
#include "Engine/Texture2D.h"
#include "PaperTileSet.h"

UOnyxTileOptions::UOnyxTileOptions()
{
	Folder.Path = TEXT("/Game/Tiles");
}

FOnyxTileSetSpec UOnyxTileOptions::ToTileSetSpec() const
{
	FOnyxTileSetSpec Spec;
	Spec.Texture = TileSheet;
	Spec.TileSize = TileSize;
	Spec.Margin = Margin;
	Spec.Spacing = Spacing;
	Spec.bSolidCollision = bSolidCollision;
	Spec.bGenerateNormalMap = bGenerateNormalMap;
	Spec.NormalMap = NormalMap;
	Spec.bPadTiles = bPadTiles;
	Spec.Name = TileSetName;
	Spec.Folder = Folder.Path;
	return Spec;
}

FOnyxTileMapSpec UOnyxTileOptions::ToTileMapSpec() const
{
	FOnyxTileMapSpec Spec;
	Spec.TileSet = TileSet;
	Spec.Width = MapSize.X;
	Spec.Height = MapSize.Y;
	Spec.CentimetersPerPixel = CentimetersPerPixel;
	Spec.bFillGround = bFillGround;
	Spec.GroundRows = GroundRows;
	Spec.GroundTile = GroundTile;
	Spec.bPlaceInLevel = bPlaceInLevel;
	Spec.Name = TileMapName;
	Spec.Folder = Folder.Path;
	return Spec;
}
