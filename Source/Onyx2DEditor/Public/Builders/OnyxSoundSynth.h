// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USoundWave;

/** The shape of a voice's wave. */
enum class EOnyxWave : uint8
{
	Sine,
	Triangle,
	Square,
	Saw,
	/** Random values held for 1 / Hz each: high Hz hisses, low Hz rumbles. */
	Noise
};

/** One voice of a sound made in code: a wave whose pitch glides, shaped by an envelope and filters. */
struct ONYX2DEDITOR_API FOnyxSoundVoice
{
	EOnyxWave Wave = EOnyxWave::Sine;
	/** Pitch at the start and at the end of the voice, Hz (it glides between them). */
	float FromHz = 440.f;
	float ToHz = 440.f;
	/** When it starts, seconds after the start of the sound. */
	float Delay = 0.f;
	/** Rising to full, held there, then dying away, seconds. */
	float Attack = 0.004f;
	float Hold = 0.f;
	float Decay = 0.2f;
	/** How it dies away: 1 evenly, higher falls fast then lingers. */
	float DecayCurve = 3.f;
	float Volume = 1.f;
	/** A low-pass filter whose cutoff glides from LowFrom to LowTo, Hz (0: no low-pass). */
	float LowFrom = 0.f;
	float LowTo = 0.f;
	/** A high-pass filter, Hz (0: none). */
	float High = 0.f;
	/** A resonant band-pass whose centre glides from BandFrom to BandTo, Hz (0: none): wind whistling, a ringing blade. */
	float BandFrom = 0.f;
	float BandTo = 0.f;
	/** How narrow the band is: 1 broad, 10 a whistle. */
	float Resonance = 2.f;
	/** Pitch wobble: how fast (Hz) and how far (a share of the pitch). */
	float VibratoHz = 0.f;
	float VibratoDepth = 0.f;
	/** Loudness wobble: how fast (Hz) and how deep (0 to 1). */
	float TremoloHz = 0.f;
	float TremoloDepth = 0.f;
	/** Square waves: the share of each cycle that is up. */
	float Duty = 0.5f;

	FOnyxSoundVoice& Shape(float InAttack, float InHold, float InDecay, float InCurve = 3.f)
	{
		Attack = InAttack;
		Hold = InHold;
		Decay = InDecay;
		DecayCurve = InCurve;
		return *this;
	}
	FOnyxSoundVoice& LowPass(float From, float To)
	{
		LowFrom = From;
		LowTo = To;
		return *this;
	}
	FOnyxSoundVoice& Band(float From, float To, float InResonance)
	{
		BandFrom = From;
		BandTo = To;
		Resonance = InResonance;
		return *this;
	}
	float Length() const { return Delay + Attack + Hold + Decay; }
};

/** A sound effect made in code: voices mixed, brought to Volume, optionally crushed to a few levels (a retro edge). */
struct ONYX2DEDITOR_API FOnyxSoundRecipe
{
	TArray<FOnyxSoundVoice> Voices;
	/** Loudest point after mixing, 0 to 1. */
	float Volume = 0.8f;
	/** 0 clean, 1 down to a handful of levels. */
	float Crush = 0.f;
	int32 Seed = 1;
	/** Its end fades into its start, and it plays round and round (wind, a drone). */
	bool bLooping = false;
};

/**
 * Sound effects synthesised in code, no recordings needed: blips, swishes, hits, booms, chimes and droning ambience.
 * A recipe is rendered to samples, written as a WAV under the project's Saved folder and imported as a Sound Wave.
 */
struct ONYX2DEDITOR_API FOnyxSoundSynth
{
	static constexpr int32 SampleRate = 44100;

	/** The recipe's samples, mono, from -1 to 1. */
	static TArray<float> Render(const FOnyxSoundRecipe& Recipe);

	/** Rendered, saved as Saved/OnyxSounds/Name.wav and imported into Folder (a /Game/... path); null on failure. */
	static USoundWave* CreateSound(const FOnyxSoundRecipe& Recipe, const FString& Folder, const FString& Name);

	/** A voice of a wave from one pitch to another (shape it with Shape, LowPass and its fields). */
	static FOnyxSoundVoice Voice(EOnyxWave Wave, float FromHz, float ToHz, float Volume = 1.f)
	{
		FOnyxSoundVoice Made;
		Made.Wave = Wave;
		Made.FromHz = FromHz;
		Made.ToHz = ToHz;
		Made.Volume = Volume;
		return Made;
	}
};
