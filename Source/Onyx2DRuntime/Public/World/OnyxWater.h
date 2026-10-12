// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxWater.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UOnyxMovementComponent;
class UPaperFlipbook;
class UPrimitiveComponent;
class UProceduralMeshComponent;
class USoundBase;

/**
 * A body of water for 2D levels. The actor's location is the middle of the surface; Size sets width and depth (in the
 * Level Painter, drag it out on the Entities layer and resize it by its edges).
 *  - Floating: physics objects are lifted at several points across their body by the part under the surface, so they
 *    tilt, right themselves and bob; each point is slowed by the water (drag, spin drag) and carried by the current.
 *    Heavy objects sink.
 *  - Swimming: Onyx characters swim (Swim settings on their movement component) and ride the waves.
 *  - Waves: a spring surface that ripples by itself, moves with floating objects and splashes (with droplets) when
 *    anything goes in or comes out; optional splash effect and sound.
 *  - Look: smooth shader water: depth gradient, drifting caustics and light shafts, rising bubbles, a crisp surface line,
 *    anti-aliased edge; darkened by the lighting mood.
 */
UCLASS(meta = (DisplayName = "Onyx Water"))
class ONYX2DRUNTIME_API AOnyxWater : public AActor
{
	GENERATED_BODY()

public:
	AOnyxWater();

	/** The middle of the calm surface. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<USceneComponent> SurfaceRoot;

	/** What is in the water (from the bottom to a little above the surface). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Area;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UProceduralMeshComponent> WaterMesh;

	/** Width and depth, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water", meta = (ClampMin = "10"))
	FVector2D Size = FVector2D(800.f, 300.f);

	// --- Floating ---------------------------------------------------------------------------

	/** g/cm³ (1 = water): objects lighter than this float (an Onyx Physics Prop's Density, wood 0.6), heavier ones sink. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Floating", meta = (ClampMin = "0", UIMax = "5"))
	float Density = 1.f;

	/** Slows objects moving through the water (per second, where they are in it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Floating", meta = (ClampMin = "0", UIMax = "10"))
	float Drag = 2.f;

	/** Slows objects spinning in the water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Floating", meta = (ClampMin = "0", UIMax = "10"))
	float SpinDrag = 2.f;

	/** How firmly floating objects settle on a flat side (0 = pure physics: a square crate floats on a corner). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Floating", meta = (ClampMin = "0", UIMax = "10"))
	float Steadiness = 3.f;

	/** Pushes everything in the water, in cm/s² (X right, Y up): a river, the pull of a waterfall. Swimmers feel it too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Floating")
	FVector2D Current = FVector2D::ZeroVector;

	/** How much the current changes over time (0 = steady, 1 = from nothing to double). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Floating", meta = (ClampMin = "0", ClampMax = "1"))
	float CurrentVariation = 0.f;

	// --- Waves ------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Waves")
	bool bWaves = true;

	/** Distance between surface points, cm: smaller is smoother. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Waves", meta = (ClampMin = "2", UIMax = "32"))
	float WaveResolution = 6.f;

	/** How quickly a disturbed point springs back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Waves", meta = (ClampMin = "1", UIMax = "300"))
	float WaveStiffness = 40.f;

	/** How quickly waves calm down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Waves", meta = (ClampMin = "0", UIMax = "20"))
	float WaveDamping = 1.6f;

	/** How far waves travel along the surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Waves", meta = (ClampMin = "0", UIMax = "40"))
	float WaveSpread = 14.f;

	/** Splash size per cm/s of the speed things cross the surface at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Waves", meta = (ClampMin = "0", UIMax = "1"))
	float SplashStrength = 0.15f;

	/** Gentle ripples when nothing touches the water, cm (0 = still). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Waves", meta = (ClampMin = "0", UIMax = "20"))
	float IdleWaveHeight = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Waves", meta = (ClampMin = "0", UIMax = "5"))
	float IdleWaveSpeed = 1.f;

	// --- Look -------------------------------------------------------------------------------

	/** Colour just under the surface (alpha = how much it hides what is behind). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Look")
	FLinearColor ShallowColor = FLinearColor(0.12f, 0.6f, 0.86f, 0.5f);

	/** Colour at the bottom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Look")
	FLinearColor DeepColor = FLinearColor(0.02f, 0.14f, 0.4f, 0.88f);

	/** The surface line, caustics, light shafts, bubbles and droplets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Look")
	FLinearColor SurfaceColor = FLinearColor(0.85f, 0.97f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Look", meta = (ClampMin = "0", UIMax = "12"))
	float SurfaceThickness = 3.f;

	/** Moving light patterns and light shafts under the surface (0 = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Look", meta = (ClampMin = "0", UIMax = "2"))
	float Caustics = 0.6f;

	/** Bubbles rising through the water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Look")
	bool bBubbles = true;

	/** Darkens with the scene's lighting mood (night, caves) like the rest of the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Look")
	bool bFollowLighting = true;

	// --- Splashes ---------------------------------------------------------------------------

	/** Droplets thrown up by splashes (per splash at full strength; 0 = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Splashes", meta = (ClampMin = "0", UIMax = "60"))
	int32 SplashDroplets = 20;

	/** Played once where something crosses the surface fast enough. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Splashes")
	TObjectPtr<UPaperFlipbook> SplashEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Splashes")
	TObjectPtr<USoundBase> SplashSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Splashes", meta = (ClampMin = "0"))
	float SplashVolume = 0.8f;

	/** Slowest crossing (cm/s) that splashes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Splashes", meta = (ClampMin = "0"))
	float SplashMinSpeed = 120.f;

	// --- Queries ----------------------------------------------------------------------------

	/** Height of the calm surface (world Z). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Water")
	float GetSurfaceZ() const;

	/** Height of the surface at a world X, waves included. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Water")
	float GetSurfaceHeightAt(float WorldX) const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Water")
	bool IsUnderwater(const FVector& Location) const;

	/** The current at a place and time (variation included), cm/s². */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Water")
	FVector2D GetCurrentAt(const FVector& Location) const;

	/** Pushes the surface down (positive Strength) or up over Width cm around a world X. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Water")
	void Splash(float WorldX, float Strength, float Width = 30.f);

	/** Throws Count droplets up from the surface at a world X. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Water")
	void ThrowDroplets(float WorldX, float Speed, int32 Count);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FDroplet
	{
		FVector2D Position;
		FVector2D Velocity;
		float Age = 0.f;
		float Life = 1.f;
		float Radius = 4.f;
	};

	void ResetSurface();
	void SimulateWaves(float DeltaSeconds);
	void UpdateDroplets(float DeltaSeconds);
	void UpdateMesh();
	void UpdateDropletMesh();
	void UpdateLook();
	void EnsureMaterials();
	void AffectOverlaps(float DeltaSeconds);
	/** Lift, drag and current on a physics object, at points across its body. Returns how deep it is (0..1). */
	float Float(UPrimitiveComponent* Primitive, float DeltaSeconds);
	/** Ripples, droplets, effect and sound where something crossed the surface at Speed. */
	void CrossedSurface(float WorldX, float Speed, float Width, bool bEntering);
	float GetLocalX(float WorldX) const;
	float GetRippleAt(float LocalX) const;

	/** Surface points: offset from the calm surface and its speed. */
	TArray<float> Heights;
	TArray<float> Speeds;
	TArray<FDroplet> Droplets;
	float WaveTime = 0.f;
	float StepRemainder = 0.f;

	/** Where each object's centre was last frame (above or below the surface). */
	TMap<TWeakObjectPtr<UPrimitiveComponent>, float> LastCenters;
	TSet<TWeakObjectPtr<UOnyxMovementComponent>> Swimmers;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WaterMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DropMaterial;

	int32 BuiltColumns = 0;
};
