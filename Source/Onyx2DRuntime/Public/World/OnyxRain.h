// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxRain.generated.h"

class UAudioComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class USoundBase;

/** One band of rain at its own depth. Far bands are thin, dim and slow; a near band is long, bright and sparse. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxRainLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	FName Name = TEXT("Rain");

	/** Distance from the gameplay plane, in cm: negative = behind the gameplay (in front of the background), positive = in front of everything. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	float Depth = -2.f;

	/** Drops in the view at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "0", UIMax = "1500"))
	int32 Drops = 300;

	/** Falling speed, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "1"))
	float Speed = 1800.f;

	/** Streak length, cm (each drop varies a little around it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "1"))
	float Length = 60.f;

	/** Streak width, cm (one art pixel looks right on pixel art). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "0.5"))
	float Width = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "0", ClampMax = "1"))
	float Opacity = 0.45f;

	/** How much Onyx 2D lights brighten this band's drops (0 = not at all). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "0", UIMax = "2"))
	float LightResponse = 1.f;

	/** Drops stop where they meet the level and splash. Otherwise they fall past everything to the bottom of the view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	bool bSplash = false;
};

/**
 * Onyx Rain: rain that follows the camera. Each layer is a band of streaks at its own depth (behind the gameplay, or in
 * front of everything), slanted by the wind and swaying with gusts. Drops of a splashing layer stop where they meet the
 * level (ground, rooftops, platforms) and burst into a ring and a few droplets. Drops near Onyx 2D lights catch their
 * colour, so the rain glows around lamps. Every layer is one mesh rebuilt each frame: no particle system needed.
 * Shows in the editor viewport too.
 */
UCLASS(meta = (DisplayName = "Onyx Rain"))
class ONYX2DRUNTIME_API AOnyxRain : public AActor
{
	GENERATED_BODY()

public:
	AOnyxRain();

	/** The bands of rain, back to front. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (DisplayName = "Layers", TitleProperty = "Name"))
	TArray<FOnyxRainLayer> RainLayers;

	/** 0 = dry, 1 = every layer's full drop count. Change it over time with Set Intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "0", ClampMax = "1"))
	float Intensity = 1.f;

	/** Sideways speed from the wind, cm/s (negative blows to the left). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	float Wind = -260.f;

	/** How far gusts swing the wind, as a fraction of Wind. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "0", UIMax = "1"))
	float Gustiness = 0.35f;

	/** Colour of the rain where no light reaches it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	FLinearColor Color = FLinearColor(0.55f, 0.68f, 0.9f);

	/**
	 * Indoors: no rain inside these actors' bounds (a Camera Zone over an interior, a volume under a roof). Drops there
	 * aren't drawn and don't splash.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	TArray<TObjectPtr<AActor>> Shelters;

	/** Drops inside an Onyx 2D light's radius take on its colour and brightness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	bool bLitByLights = true;

	// --- Splashes ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain|Splashes")
	FLinearColor SplashColor = FLinearColor(0.65f, 0.78f, 1.f);

	/** Width of the ring a drop makes on the ground, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain|Splashes", meta = (ClampMin = "1"))
	float SplashSize = 18.f;

	/** Droplets thrown up by each splash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain|Splashes", meta = (ClampMin = "0", UIMax = "6"))
	int32 SplashDroplets = 2;

	// --- Sound ---

	/** Looping rain sound, as loud as the rain is heavy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain|Sound")
	TObjectPtr<USoundBase> RainSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain|Sound", meta = (ClampMin = "0", ClampMax = "2"))
	float SoundVolume = 0.5f;

	// --- Advanced ---

	/** Drops live this far outside the view too (cm), so none pop in at the edges when the camera moves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain|Advanced", meta = (ClampMin = "0"))
	float Margin = 300.f;

	/** Rain in the level viewport while editing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain|Advanced")
	bool bPreviewInEditor = true;

	/** Unlit translucent material drawing streaks, rings and droplets (UV1.x = the shape). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain|Advanced")
	TSoftObjectPtr<UMaterialInterface> Material;

	/** Fades the rain to NewIntensity over FadeTime seconds (start a storm, let it clear). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Rain")
	void SetIntensity(float NewIntensity, float FadeTime = 2.f);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return bPreviewInEditor; }

private:
	struct FDrop
	{
		FVector2D Position = FVector2D::ZeroVector;
		/** Where it meets the level (world Z); far below the view when nothing is in its way. */
		float StopZ = -UE_BIG_NUMBER;
		FVector2D Stop = FVector2D::ZeroVector;
		bool bSplashAtStop = false;
		float Length = 60.f;
		float Alpha = 1.f;
		float SpeedScale = 1.f;
	};

	enum class EBurstKind : uint8 { Ring = 1, Droplet = 2 };

	struct FBurst
	{
		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Velocity = FVector2D::ZeroVector;
		float Age = 0.f;
		float Life = 0.2f;
		float Size = 10.f;
		EBurstKind Kind = EBurstKind::Ring;
	};

	struct FLayerState
	{
		TArray<FDrop> Drops;
		TArray<FBurst> Bursts;
	};

	struct FLightSample
	{
		FVector2D Position;
		float Radius;
		FLinearColor Color;
	};

	/** The rectangle the rain fills this frame (world X / Z), around the camera. False when there is no camera yet. */
	bool GetRainArea(float LayerY, FBox2D& OutArea) const;
	void Respawn(FDrop& Drop, const FOnyxRainLayer& Layer, const FBox2D& Area, const FVector2D& Direction, bool bAtTop) const;
	void FindStop(FDrop& Drop, const FVector2D& Direction, float BottomZ) const;
	void Splash(FLayerState& State, const FVector2D& Where);
	FLinearColor LightAt(const FVector2D& Where) const;
	bool IsSheltered(const FVector2D& Where) const;
	void GatherLights(const FBox2D& Area);
	void Simulate(int32 LayerIndex, float DeltaSeconds, const FVector2D& Velocity);
	void BuildMesh(int32 LayerIndex, const FVector2D& Velocity);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DropMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> SoundLoop;

	TArray<FLayerState> States;
	TArray<FLightSample> LightSamples;
	/** The shelters' areas this frame (world X / Z). */
	TArray<FBox2D> ShelterAreas;
	float Time = 0.f;
	/** The level is seen from above: drops fall into the ground (each lands on its own spot of the view) instead of onto the level's geometry. */
	bool bFromAbove = false;
	float IntensityTarget = 1.f;
	float IntensityRate = 0.f;
};
