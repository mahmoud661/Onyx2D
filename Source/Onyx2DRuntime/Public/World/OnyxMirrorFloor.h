// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxMirrorFloor.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPostProcessComponent;

/**
 * A polished floor that mirrors the scene: under its surface it shows everything above it upside down (the characters,
 * props, walls, windows and lights, lit as they are), fading out with depth. It mirrors the finished picture on the
 * screen (a post-process pass after the 2D lighting), so what is in view above the floor is what it reflects.
 * Place it with its location on the floor's surface, in the middle of the polished stretch; Width sets how far it goes.
 * The reflection shows while playing (it follows the player's camera).
 */
UCLASS(meta = (DisplayName = "Onyx Mirror Floor"))
class ONYX2DRUNTIME_API AOnyxMirrorFloor : public AActor
{
	GENERATED_BODY()

public:
	AOnyxMirrorFloor();

	/** The polished stretch (its top is the surface, its height the reflection's depth), shown in the editor only. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Area;

	/** Draws the reflection over the screen after the 2D lighting. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPostProcessComponent> Reflection;

	/** How far the polished floor goes, in cm (centred on the actor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirror", meta = (ClampMin = "10"))
	float Width = 800.f;

	/** How clearly the reflection shows right at the surface (1: a true mirror, 0.5: half the floor, half the reflection). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirror", meta = (ClampMin = "0", ClampMax = "1"))
	float Strength = 0.5f;

	/** How far under the surface the reflection fades out, in cm (about a character's height reads as polished stone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirror", meta = (ClampMin = "1"))
	float Depth = 160.f;

	/** How the reflection fades with depth: 1 evenly, higher fades sooner under the surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirror", meta = (ClampMin = "0.1", UIMax = "4"))
	float Falloff = 1.5f;

	/** The polish's colour: the reflection's colours are multiplied by it (warm for black marble, gold for brass). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirror")
	FLinearColor Tint = FLinearColor(0.9f, 0.86f, 0.8f);

	/** The reflection's material (empty: Onyx2D's M_OnyxMirror). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirror", AdvancedDisplay)
	TSoftObjectPtr<UMaterialInterface> Material;

	/** The surface height in world cm. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Mirror")
	float GetSurfaceZ() const { return GetActorLocation().Z; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void UpdateArea();
	/** Shows or hides the pass (hidden while the floor is out of view or nobody is looking). */
	void SetShown(bool bShown);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Instance;
};
