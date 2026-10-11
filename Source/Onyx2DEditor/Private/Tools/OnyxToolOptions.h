// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Assets/OnyxNormalMaps.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "OnyxToolOptions.generated.h"

class UPaperTileSet;
class UTexture2D;
struct FOnyxCharacterSpec;
struct FOnyxTileMapSpec;
struct FOnyxTileSetSpec;

/*
 * Option objects edited through the standard Details panel in the Onyx2D tools.
 * They are plain data: the tool widgets turn them into builder specs.
 */

UENUM()
enum class EOnyxCharacterType : uint8
{
	Player UMETA(ToolTip = "Controlled by the player (Onyx Player Character)."),
	Enemy UMETA(ToolTip = "Patrols and hurts the player on contact (Onyx Enemy Character).")
};

/** One animation row in the Character Wizard. */
USTRUCT()
struct FOnyxWizardAnimation
{
	GENERATED_BODY()

	/** Idle, Run, Jump, Fall, WallSlide, Dash, Attack1-3, Hurt and Death are used automatically. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	FName Name = TEXT("Idle");

	UPROPERTY(EditAnywhere, Category = "Animation")
	bool bEnabled = true;

	/** Row of the sprite sheet (0 = top). */
	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0"))
	int32 Row = 0;

	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0"))
	int32 StartColumn = 0;

	/** 0 = auto: every frame until the first empty cell. */
	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0"))
	int32 FrameCount = 0;

	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "1", ClampMax = "60"))
	float FramesPerSecond = 10.f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	bool bLoop = true;

	/** Use a separate image for this animation (one strip per PNG). Empty = the main sprite sheet. */
	UPROPERTY(EditAnywhere, Category = "Animation", AdvancedDisplay)
	TObjectPtr<UTexture2D> TextureOverride;
};

UCLASS(Transient)
class UOnyxCharacterWizardOptions : public UObject
{
	GENERATED_BODY()

public:
	UOnyxCharacterWizardOptions();

	UPROPERTY(EditAnywhere, Category = "Source", meta = (DisplayThumbnail = "true"))
	TObjectPtr<UTexture2D> SpriteSheet;

	/** Size of one frame in pixels. Use "Detect Frame Size" in the toolbar to guess it from the image. */
	UPROPERTY(EditAnywhere, Category = "Source", meta = (ClampMin = "1"))
	FIntPoint FrameSize = FIntPoint(32, 32);

	UPROPERTY(EditAnywhere, Category = "Output")
	FString Name = TEXT("Hero");

	UPROPERTY(EditAnywhere, Category = "Output", meta = (ContentDir))
	FDirectoryPath Folder;

	UPROPERTY(EditAnywhere, Category = "Output")
	EOnyxCharacterType Type = EOnyxCharacterType::Player;

	/** World size of one pixel in cm (2 = a 64 px frame is 128 cm tall). */
	UPROPERTY(EditAnywhere, Category = "Size", meta = (ClampMin = "0.1", UIMax = "10"))
	float CentimetersPerPixel = 4.f;

	/** Size the collision capsule to the visible pixels of the Idle animation, and put its feet on the ground. */
	UPROPERTY(EditAnywhere, Category = "Size")
	bool bAutoFitCapsule = true;

	/** Transparent pixels below the feet (ignored when Auto Fit Capsule is on). */
	UPROPERTY(EditAnywhere, Category = "Size", meta = (ClampMin = "0", EditCondition = "!bAutoFitCapsule"))
	int32 PixelsBelowFeet = 0;

	/** Build a normal map from the pixel art and plug it into the new assets, so 2D lights shade them in relief. */
	UPROPERTY(EditAnywhere, Category = "Lighting")
	bool bGenerateNormalMap = true;

	UPROPERTY(EditAnywhere, Category = "Lighting", meta = (EditCondition = "bGenerateNormalMap"))
	FOnyxNormalMapSettings NormalMap;

	UPROPERTY(EditAnywhere, Category = "Animations", meta = (TitleProperty = "{Name}  (row {Row})"))
	TArray<FOnyxWizardAnimation> Animations;

	void ResetAnimations();
	FOnyxCharacterSpec ToSpec() const;
};

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
