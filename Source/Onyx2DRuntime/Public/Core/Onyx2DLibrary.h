// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/Onyx2DTypes.h"
#include "Onyx2DLibrary.generated.h"

class UMeshComponent;
class UPaperFlipbookComponent;

/** Small helpers exposed to Blueprint. */
UCLASS()
class ONYX2DRUNTIME_API UOnyx2DLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Makes a sprite or flipbook component react to lights and (optionally) cast shadows. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Rendering")
	static void ConfigureSpriteLighting(UMeshComponent* SpriteComponent, bool bCastShadows = true);

	/** Gives a mesh a flat lit color using the Onyx2D solid-color material. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Rendering")
	static void ApplySolidColor(UMeshComponent* Mesh, FLinearColor Color);

	/**
	 * Glowing pixels on a lit sprite: its pixels brighter than Threshold (0..1 of full brightness) shine on their own,
	 * lit or not, Strength times (0 none): flames, eyes, runes, a blade's edge, lit windows in the art. A Threshold below 0
	 * takes the sprite's glow mask instead (Additional Textures slot 1, made with a Pixel Light): only those pixels glow.
	 * In the editor it is kept with the level.
	 */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Rendering")
	static void SetSpriteGlow(UPrimitiveComponent* Sprite, float Strength, float Threshold = 0.75f);

	/** Self-lit color (Glow Material in Project Settings > Onyx2D): readable even in the dark. Good for pickups and effects. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Rendering")
	static void ApplyGlowColor(UMeshComponent* Mesh, FLinearColor Color, float Intensity = 2.f);

	/** Rounds a location to the pixel grid defined in the Onyx2D project settings. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Rendering")
	static FVector SnapToPixelGrid(const FVector& Location);

	/** Moves a flipbook so the middle of its frames sits on its parent's origin, whatever pivot the sprites use. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Rendering")
	static void CenterFlipbook(UPaperFlipbookComponent* Flipbook);

	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	static float GetFacingSign(EOnyxFacing Facing) { return Onyx2D::FacingSign(Facing); }

	/** Facing for a horizontal direction; returns Current when the direction is ~0. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	static EOnyxFacing FacingFromDirection(float DirectionX, EOnyxFacing Current);

	/** How the level is played: its own style (World Settings > Onyx Level), else the project's. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D", meta = (WorldContext = "WorldContextObject"))
	static EOnyxGameStyle GetGameStyle(const UObject* WorldContextObject);

	/** The level is seen from above (Top Down game style). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D", meta = (WorldContext = "WorldContextObject"))
	static bool IsTopDown(const UObject* WorldContextObject) { return GetGameStyle(WorldContextObject) == EOnyxGameStyle::TopDown; }
};
