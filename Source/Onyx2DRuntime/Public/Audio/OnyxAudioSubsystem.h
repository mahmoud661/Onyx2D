// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "OnyxAudioSubsystem.generated.h"

class AOnyxCameraBounds;
class UAudioComponent;
class USoundAttenuation;
class USoundBase;

/** Which volume slider a sound follows (Master applies to all). */
UENUM(BlueprintType)
enum class EOnyxSoundCategory : uint8
{
	/** Footsteps, hits, pickups, ambience. */
	Effects,
	/** Level and zone music. */
	Music,
	/** Menus and HUD. */
	UI
};

/** One looping 2D track that crossfades: the level's sound, with sounds pushed on top (zones, boss fights). */
USTRUCT()
struct FOnyxAudioChannel
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Track;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LevelSound;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> PushedSounds;

	TArray<TWeakObjectPtr<UObject>> PushedOwners;
	float LevelVolume = 1.f;
	float TrackVolume = 1.f;
	EOnyxSoundCategory Category = EOnyxSoundCategory::Music;
};

/**
 * The game's sound:
 *  - music and ambience channels that crossfade and keep playing across level loads; a level sets its own tracks
 *    (Onyx Level Audio) and Camera Zones push theirs while the player is inside (cave music, wind in the mountains);
 *  - sounds played by category, so the Music / Sound Effects / Interface sliders apply;
 *  - a distance falloff made for side-on 2D worlds.
 * Everything Onyx2D plays goes through here.
 */
UCLASS()
class ONYX2DRUNTIME_API UOnyxAudioSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static UOnyxAudioSubsystem* Get(const UObject* WorldContext);

	// --- Music ------------------------------------------------------------------------------------

	/** The level's track (plays when nothing is pushed). Same track again = keeps playing. Null = silence. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Audio")
	void SetLevelMusic(USoundBase* Music, float FadeTime = 1.5f, float Volume = 1.f);

	/** Plays Music instead of the level's track until PopMusic with the same Owner. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Audio")
	void PushMusic(UObject* Owner, USoundBase* Music, float FadeTime = 1.5f);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Audio")
	void PopMusic(UObject* Owner, float FadeTime = 1.5f);

	/** Fades out whatever plays (pushed tracks included) and clears the level's track. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Audio")
	void StopMusic(float FadeTime = 1.5f);

	// --- Ambience (wind, rain, cave drips): same rules, its own channel ----------------------------------

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Audio")
	void SetLevelAmbience(USoundBase* Ambience, float FadeTime = 2.f, float Volume = 1.f);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Audio")
	void PushAmbience(UObject* Owner, USoundBase* Ambience, float FadeTime = 2.f);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Audio")
	void PopAmbience(UObject* Owner, float FadeTime = 2.f);

	// --- One-shots and attached sounds ---------------------------------------------------------------

	/** A non-positional sound (UI clicks, jingles, the player's own voice). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Audio", meta = (WorldContext = "WorldContext"))
	static void PlaySound2D(const UObject* WorldContext, USoundBase* Sound, EOnyxSoundCategory Category = EOnyxSoundCategory::Effects, float Volume = 1.f, float Pitch = 1.f);

	/** A sound in the world: quieter and panned with distance from the camera. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Audio", meta = (WorldContext = "WorldContext"))
	static void PlaySoundAt(const UObject* WorldContext, USoundBase* Sound, FVector Location, EOnyxSoundCategory Category = EOnyxSoundCategory::Effects,
		float Volume = 1.f, float Pitch = 1.f);

	/** A sound that follows a component (a crackling torch, a voice line). Follows the volume sliders while it plays. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Audio")
	static UAudioComponent* PlaySoundAttached(USoundBase* Sound, USceneComponent* AttachTo, EOnyxSoundCategory Category = EOnyxSoundCategory::Effects,
		float Volume = 1.f, float Pitch = 1.f);

	/** Volume of a category from the player's settings (Master is applied on top by the audio device). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Audio")
	static float GetCategoryVolume(EOnyxSoundCategory Category);

	/** Re-applies the settings' volumes to music, ambience and attached sounds (the settings call it). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Audio")
	void ApplyVolumes();

	/** Distance falloff for world sounds: full volume near the camera focus, silent far away, panned left / right. */
	static USoundAttenuation* GetWorldAttenuation();

	// FTickableGameObject (follows the player through Camera Zones)
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override { return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Always; }
	virtual UWorld* GetTickableGameObjectWorld() const override;

	virtual void Deinitialize() override;

private:
	struct FLoop
	{
		TWeakObjectPtr<UAudioComponent> Component;
		EOnyxSoundCategory Category = EOnyxSoundCategory::Effects;
		float BaseVolume = 1.f;
	};

	void Push(FOnyxAudioChannel& Channel, UObject* Owner, USoundBase* Sound, float FadeTime);
	void Pop(FOnyxAudioChannel& Channel, UObject* Owner, float FadeTime);
	void Refresh(FOnyxAudioChannel& Channel, float FadeTime);
	void StartTrack(FOnyxAudioChannel& Channel, USoundBase* Sound, float FadeTime, float Volume);
	void UpdateZones();

	UPROPERTY(Transient)
	FOnyxAudioChannel MusicChannel;

	UPROPERTY(Transient)
	FOnyxAudioChannel AmbienceChannel;

	TArray<FLoop> Loops;
	TWeakObjectPtr<AOnyxCameraBounds> MusicZone;
	TWeakObjectPtr<AOnyxCameraBounds> AmbienceZone;
	float ZoneCheckTimer = 0.f;
};
