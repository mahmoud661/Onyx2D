// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Lighting/OnyxLightingTypes.h"
#include "OnyxLightingEnvironment.generated.h"

class UBillboardComponent;
class UMaterialInstanceDynamic;
class UOnyxLightingProfile;

/** One moment of the day/night cycle and how the level looks then. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxTimeOfDayKey
{
	GENERATED_BODY()

	/** 0 = midnight, 0.25 = dawn, 0.5 = noon, 0.75 = dusk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time of Day", meta = (ClampMin = "0", ClampMax = "1"))
	float Time = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time of Day")
	EOnyxMoodPreset Preset = EOnyxMoodPreset::Day;

	/** Your own look instead of the preset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time of Day")
	TObjectPtr<UOnyxLightingProfile> Profile;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxDayPhaseSignature, bool, bNight);
class UPostProcessComponent;
class UTexture;

/**
 * The lighting of a 2D level, done the 2D way: no 3D sun or sky light. A level-wide post process lights every
 * lit sprite from the global light, the 2D sun and the 2D light buffer (Onyx Light 2D components, with 2D
 * shadows), keeps painted colours exact (neutral tonemapping, fixed exposure) and adds bloom, vignette and
 * grading. Pick a mood preset or tune it; Camera Zones and scripts blend to other moods at runtime.
 */
UCLASS(meta = (DisplayName = "Onyx Lighting"))
class ONYX2DRUNTIME_API AOnyxLightingEnvironment : public AActor
{
	GENERATED_BODY()

public:
	AOnyxLightingEnvironment();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D|Lighting")
	TObjectPtr<UPostProcessComponent> PostProcess;

	/** Start from a preset; editing any Mood value switches to Custom. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Lighting")
	EOnyxMoodPreset Preset = EOnyxMoodPreset::Day;

	/** The level's own mood (what you see outside Camera Zones that change it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Lighting")
	FOnyxLightingMood Mood;

	/** Use a shared Lighting Profile instead of the values above (several levels with the same look). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Lighting")
	TObjectPtr<UOnyxLightingProfile> Profile;

	/** The level goes through night, dawn, day and dusk instead of keeping one mood. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Day and Night")
	bool bDayNightCycle = false;

	/** Real seconds for a whole day (night to night). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Day and Night", meta = (ClampMin = "5", Units = "s", EditCondition = "bDayNightCycle"))
	float DayLength = 240.f;

	/** Where the day is: 0 = midnight, 0.25 = dawn, 0.5 = noon, 0.75 = dusk. Scrub it to preview the level at that hour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Day and Night", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bDayNightCycle"))
	float TimeOfDay = 0.35f;

	/** Moods through the day, blended in between. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Day and Night", meta = (EditCondition = "bDayNightCycle", TitleProperty = "Preset"))
	TArray<FOnyxTimeOfDayKey> TimeOfDayKeys;

	/** Fired when night falls (true) and when day breaks (false): light the street lamps, wake the bats... */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Day and Night")
	FOnyxDayPhaseSignature OnDayPhaseChanged;

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Day and Night")
	void SetTimeOfDay(float NewTime) { TimeOfDay = FMath::Frac(NewTime); }

	/** Between dusk and dawn. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Day and Night")
	bool IsNight() const { return TimeOfDay < 0.23f || TimeOfDay > 0.77f; }

	/** Mood the level falls back to: the day/night cycle, the profile when set, otherwise this actor's values. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Lighting")
	FOnyxLightingMood GetBaseMood() const;

	/** Pushes a mood to the composite material and the post process. */
	void ApplyMood(const FOnyxLightingMood& InMood);

	/** The screen-space buffers the lighting subsystem draws every frame. */
	void SetLightBuffers(UTexture* LightBuffer, UTexture* PositionBuffer, UTexture* OwnGlowBuffer, UTexture* HaloBuffer = nullptr);

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

private:
	void EnsureComposite();
	FOnyxLightingMood GetCycleMood() const;

	bool bWasNight = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Composite;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Icon;
#endif
};
