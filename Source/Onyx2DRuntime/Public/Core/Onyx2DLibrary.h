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
};
