// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxParallaxBackground.generated.h"

class UMaterialInterface;
class UTexture2D;

/**
 * One background layer behind the gameplay plane: a picture placed once (a moon, a lone tower), or repeated sideways
 * across the background's Total Width (a skyline), or a plain colour. The Level Painter shows and edits these as image
 * layers of the map.
 */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxParallaxLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	FName Name = TEXT("Layer");

	/** Empty = solid Tint color (good for a sky). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	TObjectPtr<UTexture2D> Texture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	FLinearColor Tint = FLinearColor::White;

	/** 0 = moves with the world, 1 = locked to the camera (infinitely far). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (UIMin = "0", UIMax = "1"))
	FVector2D Factor = FVector2D(0.5f, 0.2f);

	/** Distance behind the gameplay plane (cm, along -Y). Larger = drawn further back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (ClampMin = "1"))
	float Depth = 500.f;

	/** World height of the layer; the width follows the texture aspect and repeats. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (ClampMin = "1"))
	float Height = 1200.f;

	/** Sideways from the actor (cm): where the middle of the picture is, with the camera where it starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	float OffsetX = 0.f;

	/** Up from the actor (cm): where the middle of the picture is, with the camera where it starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	float OffsetZ = 0.f;

	/** Repeats the picture sideways across Total Width. Off: the picture once, where it is placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	bool bRepeat = true;

	/**
	 * Repeats the picture's bottom row this much further down (cm). A skyline or a row of houses then still reaches below
	 * the ground when the camera rises, instead of ending in a hard line above the street.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (ClampMin = "0"))
	float ExtendBelow = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	bool bVisible = true;

	/**
	 * Draws this layer with another material instead of the background's (for example M_OnyxFog: drifting mist between
	 * two layers). It gets the same Texture / Tint / TilingX / TilingY parameters.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	TSoftObjectPtr<UMaterialInterface> Material;
};

/**
 * Multi-layer parallax background. Add layers back-to-front; each one tiles horizontally
 * across TotalWidth and scrolls with the camera according to its Factor.
 */
UCLASS(meta = (DisplayName = "Onyx Parallax Background"))
class ONYX2DRUNTIME_API AOnyxParallaxBackground : public AActor
{
	GENERATED_BODY()

public:
	AOnyxParallaxBackground();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (TitleProperty = "Name"))
	TArray<FOnyxParallaxLayer> BackgroundLayers;

	/** Horizontal extent covered by each layer (texture repeats inside it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (ClampMin = "100"))
	float TotalWidth = 30000.f;

	/** Unlit masked material with Texture / Tint / TilingX / TilingY parameters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	TSoftObjectPtr<UMaterialInterface> Material;

	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void BuildLayer(const FOnyxParallaxLayer& Layer, int32 Index, UMaterialInterface* BaseMaterial);
};
