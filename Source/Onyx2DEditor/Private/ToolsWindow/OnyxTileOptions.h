// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Builders/OnyxNormalMaps.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "OnyxTileOptions.generated.h"

class UPaperTileSet;
class UTexture2D;
struct FOnyxTileMapSpec;
struct FOnyxTileSetSpec;

/*
 * Option objects edited through the standard Details panel in the Onyx2D tools.
 * They are plain data: the tool widgets turn them into builder specs.
 */

UCLASS(Transient)
class UOnyxTileOptions : public UObject
{
	GENERATED_BODY()

public:
	UOnyxTileOptions();

	UPROPERTY(EditAnywhere, Category = "Tile Set", meta = (DisplayThumbnail = "true"))
	TObjectPtr<UTexture2D> TileSheet;

	UPROPERTY(EditAnywhere, Category = "Tile Set", meta = (ClampMin = "1"))
	FIntPoint TileSize = FIntPoint(16, 16);

	/** Pixels around the whole sheet. */
	UPROPERTY(EditAnywhere, Category = "Tile Set", meta = (ClampMin = "0"))
	int32 Margin = 0;

	/** Pixels between tiles. */
	UPROPERTY(EditAnywhere, Category = "Tile Set", meta = (ClampMin = "0"))
	int32 Spacing = 0;

	/** Every tile gets a full box collision (good for ground/wall sheets). */
	UPROPERTY(EditAnywhere, Category = "Tile Set")
	bool bSolidCollision = true;

	/** Build a normal map from the pixel art and plug it into the new assets, so 2D lights shade them in relief. */
	UPROPERTY(EditAnywhere, Category = "Lighting")
	bool bGenerateNormalMap = true;

	UPROPERTY(EditAnywhere, Category = "Lighting", meta = (EditCondition = "bGenerateNormalMap"))
	FOnyxNormalMapSettings NormalMap;

	/** Extrude every tile's edge pixels so no lines show between tiles while the camera moves (recommended). */
	UPROPERTY(EditAnywhere, Category = "Tile Set")
	bool bPadTiles = true;

	UPROPERTY(EditAnywhere, Category = "Tile Set")
	FString TileSetName = TEXT("TS_Tiles");

	/** Tile set used for new maps. Filled in automatically after "Create Tile Set". */
	UPROPERTY(EditAnywhere, Category = "Tile Map")
	TObjectPtr<UPaperTileSet> TileSet;

	UPROPERTY(EditAnywhere, Category = "Tile Map")
	FString TileMapName = TEXT("TM_Level");

	UPROPERTY(EditAnywhere, Category = "Tile Map", meta = (ClampMin = "1", ClampMax = "1024"))
	FIntPoint MapSize = FIntPoint(60, 24);

	UPROPERTY(EditAnywhere, Category = "Tile Map", meta = (ClampMin = "0.1", UIMax = "10"))
	float CentimetersPerPixel = 4.f;

	/** Fill the bottom rows with Ground Tile so there is something to stand on. */
	UPROPERTY(EditAnywhere, Category = "Tile Map")
	bool bFillGround = true;

	UPROPERTY(EditAnywhere, Category = "Tile Map", meta = (ClampMin = "1", EditCondition = "bFillGround"))
	int32 GroundRows = 2;

	UPROPERTY(EditAnywhere, Category = "Tile Map", meta = (ClampMin = "0", EditCondition = "bFillGround"))
	int32 GroundTile = 0;

	/** Drop the new map into the open level. */
	UPROPERTY(EditAnywhere, Category = "Tile Map")
	bool bPlaceInLevel = true;

	UPROPERTY(EditAnywhere, Category = "Output", meta = (ContentDir))
	FDirectoryPath Folder;

	FOnyxTileSetSpec ToTileSetSpec() const;
	FOnyxTileMapSpec ToTileMapSpec() const;
};
