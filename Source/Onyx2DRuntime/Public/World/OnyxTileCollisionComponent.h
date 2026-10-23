// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "World/OnyxTileCollision.h"
#include "OnyxTileCollisionComponent.generated.h"

class UPaperTileMapComponent;
class UPrimitiveComponent;

/**
 * Builds the physics of a Paper2D tile map from its tiles' collision types (Unity's Tilemap Collider 2D
 * + Composite Collider 2D in one): solid tiles merge into a few boxes, one-way tiles become one-way
 * surfaces along their top edge, hazard tiles become damage zones. Only layers marked as colliding count.
 * Add it to the tile map actor; the Onyx2D tools do this for you.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Onyx Tile Collision"))
class ONYX2DRUNTIME_API UOnyxTileCollisionComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UOnyxTileCollisionComponent();

	/** Depth of the collision along Y (towards / away from the camera), in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Collision", meta = (ClampMin = "1"))
	float Thickness = 300.f;

	/** Height of one-way platforms as a fraction of the tile height (they sit at the top of the tile). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Collision", meta = (ClampMin = "0.05", ClampMax = "1"))
	float OneWayHeight = 0.25f;

	/**
	 * Solid shapes reach this fraction of a tile past their tiles on every side: for art drawn with a rim around the solid
	 * tiles (corner-based terrains), so characters stand on the rim they see.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Collision", meta = (ClampMin = "0", ClampMax = "0.5"))
	float SolidOutset = 0.f;

	/** How thick an Edge tile's walls are, as a fraction of a tile (inside the tile, along each blocked side). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Collision", meta = (ClampMin = "0.02", ClampMax = "0.5"))
	float EdgeThickness = 0.2f;

	/** Hazard zones are shrunk by this fraction of a tile on every side, so grazing a spike doesn't hurt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Collision", meta = (ClampMin = "0", ClampMax = "0.45"))
	float HazardInset = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Collision", meta = (ClampMin = "0"))
	float HazardDamage = 1.f;

	/** Turn off Paper2D's per-tile collision so only the merged shapes are used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Collision")
	bool bReplacePaperCollision = true;

	/** Solid tiles block Onyx 2D lights (2D shadows behind platforms and walls). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Lighting")
	bool bCastLightShadows = true;

	/** How deep light soaks into solid tiles, in cells: lights the surface of the ground instead of only the air. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Lighting", meta = (ClampMin = "0", UIMax = "4", EditCondition = "bCastLightShadows"))
	float LightPenetration = 0.75f;

	/** Invisible walls on the left and right edges of the map, so nothing walks off the end of the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Collision")
	bool bBlockLevelEdges = true;

	/** How far the edge walls reach above the top of the map, in cells (so high jumps can't clear them). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Collision", meta = (ClampMin = "0", EditCondition = "bBlockLevelEdges"))
	int32 EdgeWallOverhang = 20;

	/** Regenerates every shape (call after painting tiles or changing tile collision types). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Collision")
	void Rebuild();

	/** Collision type of a cell after combining every colliding layer (Solid > One-Way > Hazard). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Collision")
	EOnyxTileCollision GetCellCollision(int32 X, int32 Y) const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Collision")
	int32 GetShapeCount() const { return Shapes.Num(); }

	UPaperTileMapComponent* FindTileMapComponent() const;

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

private:
	void ClearShapes();
	TArray<EOnyxTileCollision> BuildCellGrid(const UPaperTileMapComponent& TileMap, int32& OutWidth, int32& OutHeight) const;
	/** Local-space box (relative to the tile map component) covering a cell rectangle, optionally trimmed. */
	FBox CellRectToLocalBox(const UPaperTileMapComponent& TileMap, const FIntRect& Rect) const;
	UPrimitiveComponent* AddBox(UPaperTileMapComponent& TileMap, TSubclassOf<UPrimitiveComponent> Class, const FBox& LocalBox);
	void UpdateRegistry(bool bRegister);
	/** Sends the solid rectangles and the ramps' surfaces (local space) to the 2D lighting as shadow occluders. */
	void UpdateLightOccluders(const UPaperTileMapComponent* TileMapComponent, const TArray<FIntRect>& SolidRects, const TArray<TPair<FVector, FVector>>& RampSurfaces);
	/**
	 * Slope tiles: one smooth ramp per diagonal row of them (a box turned to 45 degrees, its top on the slope's line raised by
	 * Solid Outset), tagged "Onyx.Slope". Returns each ramp's surface in local space.
	 */
	TArray<TPair<FVector, FVector>> BuildRamps(UPaperTileMapComponent& TileMap, const TArray<EOnyxTileCollision>& Cells, int32 Width, int32 Height);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> Shapes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> OneWayShapes;
};
