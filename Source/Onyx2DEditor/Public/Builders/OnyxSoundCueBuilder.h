// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USoundCue;
class USoundWave;

/** One recording a layer can pick, turned up or down to sit level with the others (they are rarely recorded alike). */
struct ONYX2DEDITOR_API FOnyxSoundTake
{
	USoundWave* Sound = nullptr;
	/** dB. */
	float Gain = 0.f;
};

/**
 * One layer of a sound: one of its takes at random each time it plays, pitched and turned up or down a little at random
 * (no two footsteps or blows alike), Delay seconds after the sound starts.
 */
struct ONYX2DEDITOR_API FOnyxSoundLayer
{
	TArray<FOnyxSoundTake> Takes;
	float Delay = 0.f;
	/** dB. */
	float Gain = 0.f;
	/** Pitch each time, from X to Y (1: as recorded). */
	FVector2D Pitch = FVector2D(1.f, 1.f);
	/** Loudness each time, from X to Y (a share). */
	FVector2D Volume = FVector2D(1.f, 1.f);

	FOnyxSoundLayer& Vary(float PitchFrom, float PitchTo, float VolumeFrom = 1.f, float VolumeTo = 1.f)
	{
		Pitch = FVector2D(PitchFrom, PitchTo);
		Volume = FVector2D(VolumeFrom, VolumeTo);
		return *this;
	}
};

/** A sound made of recordings: layers played together (a slice over a thud), each picking among its takes. */
struct ONYX2DEDITOR_API FOnyxSoundCueRecipe
{
	TArray<FOnyxSoundLayer> Layers;
	/** The whole sound's loudness, a share. */
	float Volume = 1.f;
};

/**
 * Sounds from recordings for a content builder in C++: imports sound files (wav, ogg, flac, mp3) as sound waves, and builds
 * sound cues that layer them and vary every play. The counterpart of FOnyxSoundSynth, for when a sound wants a real
 * recording (footsteps, blows on flesh).
 */
class ONYX2DEDITOR_API FOnyxSoundCueBuilder
{
public:
	/** A sound file imported (or imported again) as Folder/Name. */
	static USoundWave* ImportFile(const FString& File, const FString& Folder, const FString& Name);
	/** A sound cue playing the recipe, made anew as Folder/Name (an older one there is replaced). */
	static USoundCue* CreateCue(const FOnyxSoundCueRecipe& Recipe, const FString& Folder, const FString& Name);
};
