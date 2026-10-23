// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxLevelBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPaperFlipbook;
class UPaperTileMapComponent;
class UPaperTileSet;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Prototype levels from text. Each character is one tile:
 *   #  solid block          =  one-way platform     ^  spikes
 *   P  player start         E  enemy                L  light
 *   C  coin                 H  health pickup        K  checkpoint
 *   X  level exit           M  moving platform (a run of M is one platform; it travels right)
 *   ~  water (type it as rectangles: a run of ~, repeated on the rows below for depth)
 *   any other character is empty space. The first line is the top of the level.
 * Solid blocks and the background are previewed live in the editor. Pick a Tile Set to paint the blocks and platforms
 * with tiles instead, and give the pickups, checkpoints and exit art of your own under Look.
 */
UCLASS(meta = (DisplayName = "Onyx Level Builder"))
class ONYX2DRUNTIME_API AOnyxLevelBuilder : public AActor
{
	GENERATED_BODY()

public:
	AOnyxLevelBuilder();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Layout", meta = (MultiLine = true))
	FString Layout;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Layout", meta = (ClampMin = "10"))
	float TileSize = 100.f;

	/** Thickness of solid blocks along Y (gives them shadows and parallax depth). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Layout", meta = (ClampMin = "1"))
	float BlockDepth = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UStaticMesh> BlockMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	FLinearColor BlockColor = FLinearColor(0.16f, 0.2f, 0.26f);

	/** Paints the solid blocks ('#') with this tile set's terrain, picking edges and corners by themselves. Empty: plain blocks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Tiles")
	TObjectPtr<UPaperTileSet> TileSet;

	/** The tile set's terrain painted on solid blocks (empty: its first terrain). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Tiles", meta = (GetOptions = "GetTerrainOptions", EditCondition = "TileSet != nullptr"))
	FString Terrain;

	/** Tile drawn on each one-way platform tile ('='), by its number in the tile set. -1: the plain platform bar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Tiles", meta = (ClampMin = "-1", EditCondition = "TileSet != nullptr"))
	int32 PlatformTile = INDEX_NONE;

	/** Tile for the left end of a platform. -1: Platform Tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Tiles", meta = (ClampMin = "-1", EditCondition = "TileSet != nullptr"))
	int32 PlatformLeftTile = INDEX_NONE;

	/** Tile for the right end of a platform. -1: Platform Tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Tiles", meta = (ClampMin = "-1", EditCondition = "TileSet != nullptr"))
	int32 PlatformRightTile = INDEX_NONE;

	/** Art for the coins ('C'). Empty: Onyx2D's stand-in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperFlipbook> CoinLook;

	/** Art for the health pickups ('H'). Empty: Onyx2D's stand-in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperFlipbook> HealthLook;

	/** Art for a checkpoint ('K') before the player reaches it. Empty: Onyx2D's stand-in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperFlipbook> CheckpointLook;

	/** Art for a checkpoint once the player has reached it. Empty: Checkpoint Look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperFlipbook> CheckpointActiveLook;

	/** Art for the level exit ('X'). Empty: Onyx2D's stand-in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperFlipbook> ExitLook;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	bool bBuildBackground = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	FLinearColor BackgroundColor = FLinearColor(0.05f, 0.06f, 0.09f);

	/** Distance behind the gameplay plane; shadows from lights land here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look", meta = (ClampMin = "0"))
	float BackgroundDistance = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Spawning")
	TSubclassOf<AActor> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Spawning")
	TSubclassOf<AActor> LightClass;

	/** Tiles a moving platform ('M') travels to the right before coming back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Spawning", meta = (ClampMin = "1"))
	int32 MovingPlatformTravel = 4;

	/** Level opened by the exit tile ('X'). Empty = restart this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Spawning")
	TSoftObjectPtr<UWorld> NextLevel;

	/** Adds a camera bounds volume around the whole layout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Spawning")
	bool bAddCameraBounds = true;

	/** World location of the 'P' tile. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	bool GetPlayerStart(FVector& OutLocation) const;

	/** The rectangle the typed layout covers in the world (X and Z; invalid when the layout is empty). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	FBox GetLayoutBounds() const;

	/** Rebuilds what shows in the editor: the solid blocks (or their tiles), the platform tiles and the background. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D")
	void RebuildStatic();

	/** The terrains of the Tile Set, for the Terrain list. */
	UFUNCTION()
	TArray<FString> GetTerrainOptions() const;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	TArray<FString> GetRows() const;
	FVector TileToWorld(int32 Column, int32 Row, int32 RowCount) const;
	void SpawnDynamic();
	UClass* ResolveEnemyClass() const;
	/** Paints the tile map from the layout; false when the Tile Set can't paint it (no terrain). */
	bool PaintTiles(const TArray<FString>& Rows, int32 Columns);
	int32 FindTerrain() const;
	bool UsesPlatformTiles() const;

	UPROPERTY(VisibleAnywhere, Category = "Onyx2D")
	TObjectPtr<UInstancedStaticMeshComponent> Blocks;

	UPROPERTY(VisibleAnywhere, Category = "Onyx2D")
	TObjectPtr<UStaticMeshComponent> Background;

	/** The tiles, when a Tile Set is picked (the blocks then only collide). */
	UPROPERTY(VisibleAnywhere, Category = "Onyx2D")
	TObjectPtr<UPaperTileMapComponent> Tiles;
};
