// Copyright Onyx2D. All Rights Reserved.

#include "Core/OnyxGameUserSettings.h"
#include "AudioDevice.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Engine/Engine.h"
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
	MusicVolume = 0.8f;
	EffectsVolume = 1.f;
	UIVolume = 1.f;
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

	// Music, effects and UI volumes live on the sounds Onyx2D plays.
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (UOnyxAudioSubsystem* Audio = Context.World() ? UOnyxAudioSubsystem::Get(Context.World()) : nullptr)
		{
			Audio->ApplyVolumes();
		}
	}
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
	MusicVolume = 0.8f;
	EffectsVolume = 1.f;
	UIVolume = 1.f;
	Brightness = 1.f;
	bMuteWhenUnfocused = false;
}
