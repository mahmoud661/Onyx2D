// Copyright Onyx2D. All Rights Reserved.

#include "Audio/OnyxAudioSubsystem.h"
#include "Camera/OnyxCameraBounds.h"
#include "Components/AudioComponent.h"
#include "Core/OnyxGameUserSettings.h"
#include "Core/OnyxWorldSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxAudio, Log, All);

UOnyxAudioSubsystem* UOnyxAudioSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UOnyxAudioSubsystem>() : nullptr;
}

float UOnyxAudioSubsystem::GetCategoryVolume(EOnyxSoundCategory Category)
{
	const UOnyxGameUserSettings* Settings = UOnyxGameUserSettings::GetOnyxSettings();
	if (!Settings)
	{
		return 1.f;
	}
	switch (Category)
	{
	case EOnyxSoundCategory::Music: return Settings->GetMusicVolume();
	case EOnyxSoundCategory::UI: return Settings->GetUIVolume();
	default: return Settings->GetEffectsVolume();
	}
}

USoundAttenuation* UOnyxAudioSubsystem::GetWorldAttenuation()
{
	static TStrongObjectPtr<USoundAttenuation> Attenuation;
	if (!Attenuation.IsValid())
	{
		Attenuation.Reset(NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("OnyxWorldAttenuation")));
		FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
		Settings.bAttenuate = true;
		Settings.bSpatialize = true;
		Settings.AttenuationShape = EAttenuationShape::Sphere;
		Settings.AttenuationShapeExtents = FVector(500.f, 0.f, 0.f); // full volume around the camera's focus
		Settings.FalloffDistance = 2200.f;
		Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		Settings.NonSpatializedRadiusStart = 250.f; // close sounds stay centred instead of jumping left / right
		Settings.NonSpatializedRadiusEnd = 100.f;
	}
	return Attenuation.Get();
}

void UOnyxAudioSubsystem::PlaySound2D(const UObject* WorldContext, USoundBase* Sound, EOnyxSoundCategory Category, float Volume, float Pitch)
{
	if (Sound && WorldContext)
	{
		UGameplayStatics::PlaySound2D(WorldContext, Sound, Volume * GetCategoryVolume(Category), Pitch);
	}
}

void UOnyxAudioSubsystem::PlaySoundAt(const UObject* WorldContext, USoundBase* Sound, FVector Location, EOnyxSoundCategory Category, float Volume, float Pitch)
{
	if (Sound && WorldContext)
	{
		UE_LOG(LogOnyxAudio, Verbose, TEXT("%s (volume %.2f, pitch %.2f)"), *Sound->GetName(), Volume * GetCategoryVolume(Category), Pitch);
		UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location, FRotator::ZeroRotator, Volume * GetCategoryVolume(Category), Pitch, 0.f, GetWorldAttenuation());
	}
}

UAudioComponent* UOnyxAudioSubsystem::PlaySoundAttached(USoundBase* Sound, USceneComponent* AttachTo, EOnyxSoundCategory Category, float Volume, float Pitch)
{
	if (!Sound || !AttachTo)
	{
		return nullptr;
	}
	UAudioComponent* Component = UGameplayStatics::SpawnSoundAttached(Sound, AttachTo, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
		false, Volume * GetCategoryVolume(Category), Pitch, 0.f, GetWorldAttenuation());
	UE_LOG(LogOnyxAudio, Verbose, TEXT("%s on %s (looping %d, playing %d)"), *Sound->GetName(), *GetNameSafe(AttachTo->GetOwner()), Sound->IsLooping() ? 1 : 0,
		Component && Component->IsPlaying() ? 1 : 0);
	if (UOnyxAudioSubsystem* Audio = Get(AttachTo); Audio && Component)
	{
		Audio->Loops.RemoveAll([](const FLoop& Loop) { return !Loop.Component.IsValid(); });
		Audio->Loops.Add({ Component, Category, Volume });
	}
	return Component;
}

void UOnyxAudioSubsystem::ApplyVolumes()
{
	for (FOnyxAudioChannel* Channel : { &MusicChannel, &AmbienceChannel })
	{
		if (Channel->Track)
		{
			Channel->Track->SetVolumeMultiplier(Channel->TrackVolume * GetCategoryVolume(Channel->Category));
		}
	}
	Loops.RemoveAll([](const FLoop& Loop) { return !Loop.Component.IsValid(); });
	for (const FLoop& Loop : Loops)
	{
		Loop.Component->SetVolumeMultiplier(Loop.BaseVolume * GetCategoryVolume(Loop.Category));
	}
}

void UOnyxAudioSubsystem::StartTrack(FOnyxAudioChannel& Channel, USoundBase* Sound, float FadeTime, float Volume)
{
	if (Channel.Track && Channel.Track->Sound == Sound && Channel.Track->IsPlaying())
	{
		Channel.TrackVolume = Volume;
		Channel.Track->SetVolumeMultiplier(Volume * GetCategoryVolume(Channel.Category));
		return;
	}
	if (Channel.Track)
	{
		Channel.Track->FadeOut(FMath::Max(FadeTime, 0.01f), 0.f);
		Channel.Track = nullptr;
	}
	if (!Sound)
	{
		return;
	}
	// Persists across level loads, so a track shared by two levels doesn't restart.
	Channel.Track = UGameplayStatics::CreateSound2D(GetGameInstance(), Sound, 1.f, 1.f, 0.f, nullptr, true, true);
	UE_LOG(LogOnyxAudio, Verbose, TEXT("%s track: %s"), Channel.Category == EOnyxSoundCategory::Music ? TEXT("Music") : TEXT("Ambience"), *Sound->GetName());
	if (Channel.Track)
	{
		Channel.TrackVolume = Volume;
		Channel.Track->bIsMusic = Channel.Category == EOnyxSoundCategory::Music;
		Channel.Track->SetVolumeMultiplier(Volume * GetCategoryVolume(Channel.Category));
		Channel.Track->FadeIn(FMath::Max(FadeTime, 0.01f), 1.f);
	}
}

void UOnyxAudioSubsystem::Refresh(FOnyxAudioChannel& Channel, float FadeTime)
{
	for (int32 Index = Channel.PushedOwners.Num() - 1; Index >= 0; --Index)
	{
		if (!Channel.PushedOwners[Index].IsValid())
		{
			Channel.PushedOwners.RemoveAt(Index);
			Channel.PushedSounds.RemoveAt(Index);
		}
	}
	if (!Channel.PushedSounds.IsEmpty())
	{
		StartTrack(Channel, Channel.PushedSounds.Last(), FadeTime, 1.f);
	}
	else
	{
		StartTrack(Channel, Channel.LevelSound, FadeTime, Channel.LevelVolume);
	}
}

void UOnyxAudioSubsystem::Push(FOnyxAudioChannel& Channel, UObject* Owner, USoundBase* Sound, float FadeTime)
{
	Pop(Channel, Owner, -1.f);
	Channel.PushedOwners.Add(Owner);
	Channel.PushedSounds.Add(Sound);
	Refresh(Channel, FadeTime);
}

void UOnyxAudioSubsystem::Pop(FOnyxAudioChannel& Channel, UObject* Owner, float FadeTime)
{
	const int32 Index = Channel.PushedOwners.IndexOfByKey(Owner);
	if (Index == INDEX_NONE)
	{
		return;
	}
	Channel.PushedOwners.RemoveAt(Index);
	Channel.PushedSounds.RemoveAt(Index);
	if (FadeTime >= 0.f)
	{
		Refresh(Channel, FadeTime);
	}
}

void UOnyxAudioSubsystem::SetLevelMusic(USoundBase* Music, float FadeTime, float Volume)
{
	MusicChannel.Category = EOnyxSoundCategory::Music;
	MusicChannel.LevelSound = Music;
	MusicChannel.LevelVolume = Volume;
	Refresh(MusicChannel, FadeTime);
}

void UOnyxAudioSubsystem::PushMusic(UObject* Owner, USoundBase* Music, float FadeTime)
{
	MusicChannel.Category = EOnyxSoundCategory::Music;
	Push(MusicChannel, Owner, Music, FadeTime);
}

void UOnyxAudioSubsystem::PopMusic(UObject* Owner, float FadeTime)
{
	Pop(MusicChannel, Owner, FadeTime);
}

void UOnyxAudioSubsystem::StopMusic(float FadeTime)
{
	MusicChannel.LevelSound = nullptr;
	MusicChannel.PushedOwners.Reset();
	MusicChannel.PushedSounds.Reset();
	StartTrack(MusicChannel, nullptr, FadeTime, 1.f);
}

void UOnyxAudioSubsystem::SetLevelAmbience(USoundBase* Ambience, float FadeTime, float Volume)
{
	AmbienceChannel.Category = EOnyxSoundCategory::Effects;
	AmbienceChannel.LevelSound = Ambience;
	AmbienceChannel.LevelVolume = Volume;
	Refresh(AmbienceChannel, FadeTime);
}

void UOnyxAudioSubsystem::PushAmbience(UObject* Owner, USoundBase* Ambience, float FadeTime)
{
	AmbienceChannel.Category = EOnyxSoundCategory::Effects;
	Push(AmbienceChannel, Owner, Ambience, FadeTime);
}

void UOnyxAudioSubsystem::PopAmbience(UObject* Owner, float FadeTime)
{
	Pop(AmbienceChannel, Owner, FadeTime);
}

void UOnyxAudioSubsystem::Tick(float DeltaTime)
{
	ZoneCheckTimer -= DeltaTime;
	if (ZoneCheckTimer <= 0.f)
	{
		ZoneCheckTimer = 0.25f;
		UpdateZones();
	}
}

UWorld* UOnyxAudioSubsystem::GetTickableGameObjectWorld() const
{
	return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

TStatId UOnyxAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOnyxAudioSubsystem, STATGROUP_Tickables);
}

void UOnyxAudioSubsystem::UpdateZones()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	const UOnyxWorldSubsystem* Registry = World ? World->GetSubsystem<UOnyxWorldSubsystem>() : nullptr;
	if (!Pawn || !Registry)
	{
		return;
	}
	const FVector Location = Pawn->GetActorLocation();

	// The zone the player is in with music (or ambience) of its own; leaving it brings the level's back.
	AOnyxCameraBounds* Music = Registry->FindCameraBounds(Location, [](const AOnyxCameraBounds& Zone) { return Zone.Music != nullptr; });
	if (Music != MusicZone.Get())
	{
		const float Fade = Music ? Music->AudioFadeTime : (MusicZone.IsValid() ? MusicZone->AudioFadeTime : 1.5f);
		if (MusicZone.IsValid())
		{
			Pop(MusicChannel, MusicZone.Get(), Music ? -1.f : Fade);
		}
		if (Music)
		{
			PushMusic(Music, Music->Music, Fade);
		}
		MusicZone = Music;
	}
	AOnyxCameraBounds* Ambience = Registry->FindCameraBounds(Location, [](const AOnyxCameraBounds& Zone) { return Zone.Ambience != nullptr; });
	if (Ambience != AmbienceZone.Get())
	{
		const float Fade = Ambience ? Ambience->AudioFadeTime : (AmbienceZone.IsValid() ? AmbienceZone->AudioFadeTime : 2.f);
		if (AmbienceZone.IsValid())
		{
			Pop(AmbienceChannel, AmbienceZone.Get(), Ambience ? -1.f : Fade);
		}
		if (Ambience)
		{
			PushAmbience(Ambience, Ambience->Ambience, Fade);
		}
		AmbienceZone = Ambience;
	}
}

void UOnyxAudioSubsystem::Deinitialize()
{
	for (FOnyxAudioChannel* Channel : { &MusicChannel, &AmbienceChannel })
	{
		if (Channel->Track)
		{
			Channel->Track->Stop();
			Channel->Track = nullptr;
		}
	}
	Super::Deinitialize();
}
