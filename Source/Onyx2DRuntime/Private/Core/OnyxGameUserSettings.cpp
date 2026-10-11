// Copyright Onyx2D. All Rights Reserved.

#include "Core/OnyxGameUserSettings.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "Misc/App.h"

namespace
{
	constexpr float DefaultDisplayGamma = 2.2f;
}

UOnyxGameUserSettings::UOnyxGameUserSettings()
{
	MasterVolume = 1.f;
	Brightness = 1.f;
	bMuteWhenUnfocused = false;
}

UOnyxGameUserSettings* UOnyxGameUserSettings::GetOnyxSettings()
{
	return GEngine ? Cast<UOnyxGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UOnyxGameUserSettings::ApplyCustomSettings()
{
	if (!GEngine)
	{
		return;
	}

	GEngine->DisplayGamma = DefaultDisplayGamma * Brightness;

	if (FAudioDeviceManager* DeviceManager = GEngine->GetAudioDeviceManager())
	{
		const float Volume = MasterVolume;
		DeviceManager->IterateOverAllDevices([Volume](Audio::FDeviceId, FAudioDevice* Device)
		{
			if (Device)
			{
				Device->SetTransientPrimaryVolume(Volume);
			}
		});
	}

	FApp::SetUnfocusedVolumeMultiplier(bMuteWhenUnfocused ? 0.f : 1.f);
}

void UOnyxGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();
	ApplyCustomSettings();
}

void UOnyxGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	MasterVolume = 1.f;
	Brightness = 1.f;
	bMuteWhenUnfocused = false;
}
