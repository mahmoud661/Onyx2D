// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "OnyxLight2D.generated.h"

class UMaterialInstanceDynamic;
class UOnyxLight2DComponent;
class UStaticMeshComponent;

/** How a light's brightness moves over time. */
UENUM(BlueprintType)
enum class EOnyxLightAnimation : uint8
{
	/** Steady. */
	None,
	/** Organic noise: torches, candles, broken lamps. */
	Flicker,
	/** Smooth breathing: crystals, magic, lava. */
	Pulse
};

/** Animates the brightness of the owner's Onyx Light 2D (flicker or pulse). */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent))
class ONYX2DRUNTIME_API UOnyxLightFlickerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxLightFlickerComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Light")
	EOnyxLightAnimation Animation = EOnyxLightAnimation::Flicker;

	/** 0 = steady, 1 = goes fully dark at the low point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Light", meta = (ClampMin = "0", ClampMax = "1"))
	float Strength = 0.25f;

	/** Flicker: noise speed. Pulse: radians per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Light", meta = (ClampMin = "0"))
	float Speed = 6.f;

	/** Light to animate; defaults to the first Onyx Light 2D on the owner. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Light")
	void SetLight(UOnyxLight2DComponent* InLight);

	/** Current brightness relative to the light's own intensity (1 = full). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Light")
	float GetIntensityScale() const { return Scale; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UOnyxLight2DComponent> Light;

	float Time = 0.f;
	float Scale = 1.f;
};

/** Ready-made light looks. */
UENUM(BlueprintType)
enum class EOnyxLightPreset : uint8
{
	Custom,
	Torch,
	Candle,
	Lantern,
	Magic,
	Crystal,
	Lava,
	Window
};

/**
 * 2D light actor: a Onyx Light 2D (soft light with 2D shadows) plus flicker / pulse and an additive glow
 * halo that reads as light in the air. Presets set a good look in one click.
 */
UCLASS(meta = (DisplayName = "Onyx Light 2D"))
class ONYX2DRUNTIME_API AOnyxLight2D : public AActor
{
	GENERATED_BODY()

public:
	AOnyxLight2D();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxLight2DComponent> Light2D;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxLightFlickerComponent> Flicker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UStaticMeshComponent> Halo;

	/** Choosing a preset sets colour, brightness, radius and animation; tweak them afterwards freely. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	EOnyxLightPreset Preset = EOnyxLightPreset::Torch;

	/** Soft additive glow disc around the light (off by default: on crisp pixel art it can read as a smudge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Halo")
	bool bShowHalo = false;

	/** Halo diameter as a fraction of the light radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Halo", meta = (ClampMin = "0.05", ClampMax = "2", EditCondition = "bShowHalo"))
	float HaloSize = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Halo", meta = (ClampMin = "0", UIMax = "4", EditCondition = "bShowHalo"))
	float HaloIntensity = 0.35f;

	/** Higher = tighter, hotter core. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Halo", meta = (ClampMin = "0.5", UIMax = "8", EditCondition = "bShowHalo"))
	float HaloFalloff = 3.f;

	/** Applies one of the ready-made looks. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D")
	void ApplyPreset(EOnyxLightPreset NewPreset);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

private:
	void UpdateHalo();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HaloMaterial;
};
