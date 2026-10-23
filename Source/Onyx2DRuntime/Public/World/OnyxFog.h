// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "OnyxFog.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UTexture;

/** Ready-made looks for Onyx Fog; Custom keeps whatever Look says. */
UENUM(BlueprintType)
enum class EOnyxFogPreset : uint8
{
	Custom,
	/** Low, thin and pale: hugs the ground, thickest at the bottom of the box. */
	GroundMist UMETA(DisplayName = "Ground Mist"),
	/** Thick, slow, sickly green banks that hide the far side of a room. */
	MarshFog UMETA(DisplayName = "Marsh Fog"),
	/** A faint even haze filling a cave, catching every lamp. */
	CaveHaze UMETA(DisplayName = "Cave Haze"),
	/** Near solid: hides what is behind until a lantern pushes it back. */
	FogWall UMETA(DisplayName = "Fog Wall"),
	/** Cold drifting wisps that only really show at night. */
	NightMist UMETA(DisplayName = "Night Mist")
};

/** What a fog looks like and how it moves. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxFogLook
{
	GENERATED_BODY()

	/** Its colour; the alpha is how thick it can get (0 to 1). Lit by the level's mood and by 2D lights on top of this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	FLinearColor Color = FLinearColor(0.62f, 0.7f, 0.78f, 0.75f);

	/** How much of the box it fills (0: wisps here and there, 1: solid through). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog", meta = (ClampMin = "0", ClampMax = "1"))
	float Density = 0.5f;

	/** Size of its clouds, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog", meta = (ClampMin = "20", UIMax = "2000", Units = "cm"))
	float CloudSize = 320.f;

	/** How it drifts, cm/s (X along, Y up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	FVector2D Drift = FVector2D(14.f, 0.f);

	/** Thicker towards the bottom of the box (0: even all through, 1: a layer lying on the ground: put the box's bottom on the floor, it barely fades there). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog", meta = (ClampMin = "0", ClampMax = "1"))
	float Ground = 0.4f;

	/** Fine wisps over the big banks (0: soft banks only, 1: mostly wisps). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog", meta = (ClampMin = "0", ClampMax = "1"))
	float Detail = 0.5f;

	/** How far in from the box's sides it fades in, cm: no hard border anywhere. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog", meta = (ClampMin = "0", UIMax = "1200", Units = "cm"))
	float EdgeSoftness = 340.f;

	/** How brightly 2D lights shine in it (0: they don't, 1: a lamp's glow fills the fog round it, 2: strong beams). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog", meta = (ClampMin = "0", UIMax = "3"))
	float LightScatter = 1.f;

	/** How much it shows by itself in the dark, with no lamp near (0: only lit fog shows, 1: its colour as it is). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog", meta = (ClampMin = "0", ClampMax = "1"))
	float OwnLight = 0.45f;
};

/**
 * Onyx Fog: drifting fog over part of a level (side view or top down). Put it where the fog should be and set its Size: it
 * fades in softly from the box's edges, drifts, thickens towards the bottom as Ground says, glows where 2D lights shine
 * into it, thins round the player (and anything with an Onyx Fog Clearer, such as a lantern) and thickens at night. In
 * front of characters it hides what is behind it; behind them it is a haze over the background. Pick a Preset or set the
 * Look yourself. The Onyx Fog subsystem fades every fog in or out (weather) and says how thick the fog is anywhere.
 */
UCLASS(meta = (DisplayName = "Onyx Fog"))
class ONYX2DRUNTIME_API AOnyxFog : public AActor
{
	GENERATED_BODY()

public:
	AOnyxFog();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Area;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UProceduralMeshComponent> Mesh;

	/** Width and height of the fog, cm; the actor is its middle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog", meta = (ClampMin = "50"))
	FVector2D Size = FVector2D(1600.f, 600.f);

	/** A ready-made look (changing it fills in Look); Custom keeps your own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	EOnyxFogPreset Preset = EOnyxFogPreset::GroundMist;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	FOnyxFogLook Look;

	/** Drawn in front of characters (it hides them a little); off: behind them, a haze over the background. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	bool bInFront = true;

	/** Its thickness at night, times its usual (1: the same day and night). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Time", meta = (ClampMin = "0", UIMax = "3"))
	float NightThickness = 1.3f;

	/** Pushed back round the player, cm (0: the player walks through it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Clearing", meta = (ClampMin = "0", UIMax = "1000", Units = "cm"))
	float PlayerClearRadius = 120.f;

	/** How soft the edge of a cleared space is (0: sharp, 1: fades over the whole radius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Clearing", meta = (ClampMin = "0", ClampMax = "1"))
	float ClearSoftness = 0.7f;

	/** Applies Preset to Look (done for you when Preset changes in the editor). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Fog")
	void ApplyPreset();

	/** The look a preset gives. */
	static FOnyxFogLook GetPresetLook(EOnyxFogPreset InPreset);

	/** How thick it is at Location (0 to 1), without the drifting clouds: its density, edges, ground layer and cleared spaces. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Fog")
	float GetThicknessAt(const FVector& Location) const;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BuildMesh();
	void UpdateMaterial(float DeltaSeconds);
	/** The places it is pushed back from (X, Z, radius), nearest first, at most four. */
	void GatherClearings(TArray<FVector, TInlineAllocator<4>>& Out) const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	UPROPERTY(Transient)
	TObjectPtr<UTexture> ShownLightBuffer;

	/** 0 by day, 1 at night, eased between. */
	float NightWeight = 0.f;
};

/** Pushes Onyx Fog back round itself (a lantern, a brazier, a spell): give it to anything that should clear a space. */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Onyx Fog Clearer"))
class ONYX2DRUNTIME_API UOnyxFogClearerComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	/** How far round it the fog is pushed back, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog", meta = (ClampMin = "0", UIMax = "1500", Units = "cm"))
	float Radius = 220.f;

	/** Off: it clears nothing (a lantern put away). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	bool bClearing = true;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
};

/**
 * Every Onyx Fog in the world: fades them all in or out together (weather: Set Fog Amount), says how thick the fog is
 * anywhere (Get Fog At, for foes that should not see through it), and keeps the list of Fog Clearers.
 */
UCLASS()
class ONYX2DRUNTIME_API UOnyxFogSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UOnyxFogSubsystem* Get(const UObject* WorldContext);

	/** Fades every fog towards Amount (0: none, 1: as set, above 1: thicker) over Seconds. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Fog")
	void SetFogAmount(float Amount, float Seconds = 3.f);

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Fog")
	float GetFogAmount() const { return Amount; }

	/** How thick the fog is at Location (0 to 1): the thickest fog there. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Fog")
	float GetFogAt(const FVector& Location) const;

	void Register(AOnyxFog* Fog) { Fogs.AddUnique(Fog); }
	void Unregister(AOnyxFog* Fog) { Fogs.Remove(Fog); }
	void Register(UOnyxFogClearerComponent* Clearer) { Clearers.AddUnique(Clearer); }
	void Unregister(UOnyxFogClearerComponent* Clearer) { Clearers.Remove(Clearer); }
	const TArray<TWeakObjectPtr<UOnyxFogClearerComponent>>& GetClearers() const { return Clearers; }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<TWeakObjectPtr<AOnyxFog>> Fogs;
	TArray<TWeakObjectPtr<UOnyxFogClearerComponent>> Clearers;
	float Amount = 1.f;
	float TargetAmount = 1.f;
	float FadeSpeed = 0.f;
};
