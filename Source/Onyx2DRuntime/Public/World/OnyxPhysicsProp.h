// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxPhysicsProp.generated.h"

class UBoxComponent;
class UPaperSprite;
class UPaperSpriteComponent;
class UStaticMeshComponent;

/**
 * A loose object for 2D levels: a crate, barrel or rock that falls, tumbles, gets knocked around and floats in Onyx Water
 * (or sinks, when it is denser than the water). It only moves and turns in the level's plane. Its body is a box the size
 * of its sprite; until it has a sprite it shows a coloured stand-in.
 */
UCLASS(meta = (DisplayName = "Onyx Physics Prop"))
class ONYX2DRUNTIME_API AOnyxPhysicsProp : public AActor
{
	GENERATED_BODY()

public:
	AOnyxPhysicsProp();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperSpriteComponent> SpriteComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UStaticMeshComponent> Placeholder;

	/** What it looks like. The body takes the sprite's size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Prop")
	TObjectPtr<UPaperSprite> Sprite;

	/** Body width and height in cm (0 = the sprite's size, or 60 cm without one). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Prop", meta = (ClampMin = "0"))
	FVector2D Size = FVector2D::ZeroVector;

	/** g/cm³: lighter than the water floats (wood about 0.6), heavier sinks (stone about 2.5). Sets its mass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Prop", meta = (ClampMin = "0.01", UIMax = "8"))
	float Density = 0.6f;

	/** From above (Top Down levels): how quickly it slides to a stop on the ground once pushed (it doesn't fall or turn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Physics Prop", meta = (ClampMin = "0", UIMax = "20"))
	float GroundFriction = 6.f;

	/** Colour of the stand-in shown until it has a sprite. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Prop")
	FLinearColor PlaceholderColor = FLinearColor(0.55f, 0.36f, 0.2f);

	/** Width and height of the body in cm. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Prop")
	FVector2D GetBodySize() const;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;
};
