// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Lighting/OnyxLightingTypes.h"
#include "OnyxLightingEnvironment.generated.h"

class UBillboardComponent;
class UMaterialInstanceDynamic;
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

	/** Mood the level falls back to: the profile when set, otherwise this actor's values. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Lighting")
	FOnyxLightingMood GetBaseMood() const;

	/** Pushes a mood to the composite material and the post process. */
	void ApplyMood(const FOnyxLightingMood& InMood);

	/** The screen-space buffers the lighting subsystem draws every frame. */
	void SetLightBuffers(UTexture* LightBuffer, UTexture* PositionBuffer);

	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

private:
	void EnsureComposite();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Composite;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Icon;
#endif
};
