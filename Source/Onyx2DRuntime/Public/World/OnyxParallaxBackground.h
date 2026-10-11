// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxParallaxBackground.generated.h"

class UMaterialInterface;
class UTexture2D;

/** One background layer: a horizontally tiling texture quad behind the gameplay plane. */
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

	/** Vertical offset from the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	float OffsetZ = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	bool bVisible = true;
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
