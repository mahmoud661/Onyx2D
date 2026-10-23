// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "OnyxLight2D.generated.h"

class UAudioComponent;
class UMaterialInstanceDynamic;
class USoundBase;
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
	Pulse,
	/** Blinks on and off (Speed blinks a second): an alarm, a beacon, a warning lamp. */
	Strobe,
	/** Mostly on, now and then a few quick drop-outs: a dying tube, a failing sign. */
	Stutter
};

/**
 * Brings the owner's Onyx Light 2D to life while playing: its brightness (flicker, pulse, strobe, stutter), its colour
 * drifting to a second one and back, and its motion: sweeping its direction back and forth (a searchlight) or swinging
 * like a lantern on a chain (everything attached to the light swings with it).
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Light Animation"))
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

	/** Flicker: noise speed. Pulse: radians per second. Strobe: blinks per second. Stutter: how often it stutters (6 = now and then). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Light", meta = (ClampMin = "0"))
	float Speed = 6.f;

	/** Strobe: the share of each blink that is lit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Light", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float StrobeDuty = 0.5f;

	/** How far its colour drifts towards Color B at the turn (0: it keeps its own colour). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Light|Colour", meta = (ClampMin = "0", ClampMax = "1"))
	float ColorShift = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Light|Colour")
	FLinearColor ColorB = FLinearColor(0.4f, 0.6f, 1.f);

	/** Seconds for the drift there and back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Light|Colour", meta = (ClampMin = "0.1", Units = "s"))
	float ColorShiftPeriod = 4.f;

	/** Sweeps the light's direction back and forth this many degrees each way: a searchlight, a lighthouse, an alarm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Light|Motion", meta = (ClampMin = "0", ClampMax = "180"))
	float SweepAngle = 0.f;

	/** Seconds for a sweep there and back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Light|Motion", meta = (ClampMin = "0.1", Units = "s"))
	float SweepPeriod = 4.f;

	/** Swings the light like a lantern on a chain, this far each way (cm); whatever is attached to the light swings and tilts with it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Light|Motion", meta = (ClampMin = "0"))
	float SwayDistance = 0.f;

	/** Seconds for a swing there and back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Light|Motion", meta = (ClampMin = "0.1", Units = "s"))
	float SwayPeriod = 2.4f;

	/** The chain: the light hangs this far under the point it swings from (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Light|Motion", meta = (ClampMin = "1"))
	float SwayLength = 120.f;

	/** Light to animate; defaults to the first Onyx Light 2D on the owner. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Light")
	void SetLight(UOnyxLight2DComponent* InLight);

	/** Starts (or stops) ticking for what is set now: call after changing the settings while playing. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Light")
	void Refresh();

	/** Current brightness relative to the light's own intensity (1 = full). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Light")
	float GetIntensityScale() const { return Scale; }

	/** True when any of its settings moves the light (for the editor's gizmo). */
	bool HasMotion() const { return SweepAngle > 0.f || SwayDistance > 0.f; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** 0 lit to 1 at its darkest, for this frame. */
	float Darkness(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<UOnyxLight2DComponent> Light;

	float Time = 0.f;
	float Scale = 1.f;
	FLinearColor BaseColor = FLinearColor::White;
	FVector BaseLocation = FVector::ZeroVector;
	FRotator BaseRotation = FRotator::ZeroRotator;
	bool bHasBase = false;
	// Stutter: blinks left in this stutter, time to the next change, whether it is dark now.
	int32 StutterBlinks = 0;
	float StutterTimer = 0.f;
	bool bStutterDark = false;
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
	Window,
	/** A neon tube (Line) that stutters and drifts pink to blue. */
	Neon,
	/** A cold white tube (Line), now and then failing. */
	Fluorescent,
	/** A big open fire: a hot crisp pool, a hard flicker. */
	Brazier,
	/** A narrow beam (Spot) sweeping back and forth. */
	Searchlight,
	/** A warm lantern swinging on its chain. */
	HangingLantern,
	/** A red warning light, blinking and sweeping. */
	Alarm,
	/** A soft cyan-green glow, slowly breathing: mushrooms, moss, deep water. */
	Glowshroom,
	/** Cold light pouring from a tall window (Area). */
	MoonWindow,
	/** Light through a stained-glass window: give it a Pattern. */
	StainedGlass
};

/** When a light is lit on its own, with the level's day and night (Onyx Lighting's day/night cycle). */
UENUM(BlueprintType)
enum class EOnyxLightActiveTime : uint8
{
	/** Whenever it is switched on. */
	Always,
	/** From dusk to dawn: street lamps, windows. */
	Night,
	/** Through the day only. */
	Day
};

/**
 * 2D light actor: an Onyx Light 2D (from a point, a cone, a line or a rectangle, with 2D shadows), its Light Animation
 * (flicker, pulse, strobe, stutter, colour drift, sweep, sway) and an additive glow halo that reads as light in the air.
 * Presets set a good look in one click; it can light itself at night only.
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

	/** Choosing a preset sets shape, colour, brightness, radius, falloff and animation; tweak them afterwards freely. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	EOnyxLightPreset Preset = EOnyxLightPreset::Torch;

	/** Lit always, or only at night or by day (it follows Onyx Lighting's day/night cycle, fading over Day Fade Time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	EOnyxLightActiveTime ActiveTime = EOnyxLightActiveTime::Always;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (ClampMin = "0", Units = "s", EditCondition = "ActiveTime != EOnyxLightActiveTime::Always"))
	float DayFadeTime = 1.5f;

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

	/** Looping sound at the light (a crackling torch, a humming lamp), quieter with distance; stops when the light is off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Sound")
	TObjectPtr<USoundBase> AmbientSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Sound", meta = (ClampMin = "0", ClampMax = "2"))
	float SoundVolume = 0.6f;

	/** Played when the light is switched on / off (a click, a whoosh of fire). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Sound")
	TObjectPtr<USoundBase> SwitchSound;

	/** Turns the light (and its halo and sound) on or off with a fade. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D")
	void SetLightEnabled(bool bEnabled, float FadeTime = 0.25f);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D")
	void ToggleLight(float FadeTime = 0.25f);

	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	bool IsLightEnabled() const;

	/** Applies one of the ready-made looks. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D")
	void ApplyPreset(EOnyxLightPreset NewPreset);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

private:
	void UpdateHalo();

	UFUNCTION()
	void HandleDayPhase(bool bNight);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HaloMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> SoundLoop;
};
