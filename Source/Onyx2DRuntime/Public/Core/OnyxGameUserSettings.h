// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "OnyxGameUserSettings.generated.h"

/**
 * Engine user settings plus master / music / effects / UI volume, brightness and mute-when-unfocused.
 * Enable with GameUserSettingsClassName=/Script/Onyx2DRuntime.OnyxGameUserSettings in DefaultEngine.ini.
 */
UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class ONYX2DRUNTIME_API UOnyxGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UOnyxGameUserSettings();

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Settings")
	static UOnyxGameUserSettings* GetOnyxSettings();

	/** Applies volume, brightness and focus muting (the parts the engine does not know about). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Settings")
	void ApplyCustomSettings();

	virtual void ApplyNonResolutionSettings() override;
	virtual void SetToDefaults() override;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Settings")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Settings")
	void SetMasterVolume(float Volume) { MasterVolume = FMath::Clamp(Volume, 0.f, 1.f); }

	/** Music, sound effects (and ambience) and UI sounds, on top of Master. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Settings")
	float GetMusicVolume() const { return MusicVolume; }

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Settings")
	void SetMusicVolume(float Volume) { MusicVolume = FMath::Clamp(Volume, 0.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Settings")
	float GetEffectsVolume() const { return EffectsVolume; }

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Settings")
	void SetEffectsVolume(float Volume) { EffectsVolume = FMath::Clamp(Volume, 0.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Settings")
	float GetUIVolume() const { return UIVolume; }

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Settings")
	void SetUIVolume(float Volume) { UIVolume = FMath::Clamp(Volume, 0.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Settings")
	float GetBrightness() const { return Brightness; }

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Settings")
	void SetBrightness(float InBrightness) { Brightness = FMath::Clamp(InBrightness, 0.5f, 1.5f); }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Settings")
	bool GetMuteWhenUnfocused() const { return bMuteWhenUnfocused; }

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Settings")
	void SetMuteWhenUnfocused(bool bMute) { bMuteWhenUnfocused = bMute; }

protected:
	UPROPERTY(config)
	float MasterVolume;

	UPROPERTY(config)
	float MusicVolume;

	UPROPERTY(config)
	float EffectsVolume;

	UPROPERTY(config)
	float UIVolume;

	UPROPERTY(config)
	float Brightness;

	UPROPERTY(config)
	bool bMuteWhenUnfocused;
};
