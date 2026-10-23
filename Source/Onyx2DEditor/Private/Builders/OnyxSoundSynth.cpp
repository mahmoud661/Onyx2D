// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxSoundSynth.h"
#include "Builders/OnyxAssetUtils.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "HAL/FileManager.h"
#include "Math/RandomStream.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundWave.h"

namespace OnyxSynth
{
	/** One-pole low-pass coefficient for a cutoff, per sample. */
	float LowPassCoefficient(float CutoffHz)
	{
		const float Clamped = FMath::Clamp(CutoffHz, 10.f, FOnyxSoundSynth::SampleRate * 0.45f);
		return 1.f - FMath::Exp(-UE_TWO_PI * Clamped / FOnyxSoundSynth::SampleRate);
	}

	float Oscillator(EOnyxWave Wave, float Phase, float Duty)
	{
		switch (Wave)
		{
		case EOnyxWave::Sine:
			return FMath::Sin(UE_TWO_PI * Phase);
		case EOnyxWave::Triangle:
			return 1.f - 4.f * FMath::Abs(Phase - 0.5f);
		case EOnyxWave::Square:
			return Phase < Duty ? 1.f : -1.f;
		case EOnyxWave::Saw:
			return 2.f * Phase - 1.f;
		default:
			return 0.f;
		}
	}

	void AppendLittleEndian(TArray<uint8>& Bytes, uint32 Value, int32 Size)
	{
		for (int32 Byte = 0; Byte < Size; ++Byte)
		{
			Bytes.Add(uint8((Value >> (8 * Byte)) & 0xFF));
		}
	}
}

TArray<float> FOnyxSoundSynth::Render(const FOnyxSoundRecipe& Recipe)
{
	float Length = 0.05f;
	for (const FOnyxSoundVoice& Voice : Recipe.Voices)
	{
		Length = FMath::Max(Length, Voice.Length());
	}
	const int32 Count = FMath::CeilToInt((Length + 0.02f) * SampleRate);
	TArray<float> Mix;
	Mix.Init(0.f, Count);
	FRandomStream Random(Recipe.Seed);
	for (const FOnyxSoundVoice& Voice : Recipe.Voices)
	{
		const int32 First = FMath::FloorToInt(Voice.Delay * SampleRate);
		const float Span = FMath::Max(Voice.Attack + Voice.Hold + Voice.Decay, 0.001f);
		const int32 Samples = FMath::CeilToInt(Span * SampleRate);
		float Phase = Random.FRand();
		float Held = Random.FRandRange(-1.f, 1.f);
		float LowA = 0.f;
		float LowB = 0.f;
		float HighState = 0.f;
		float BandLow = 0.f;
		float BandMid = 0.f;
		const float HighCoefficient = Voice.High > 0.f ? OnyxSynth::LowPassCoefficient(Voice.High) : 0.f;
		for (int32 Index = 0; Index < Samples && First + Index < Count; ++Index)
		{
			const float Time = float(Index) / SampleRate;
			const float Along = Time / Span;
			// The pitch glides evenly in octaves, wobbling if asked.
			float Hz = Voice.FromHz * FMath::Pow(FMath::Max(Voice.ToHz, 1.f) / FMath::Max(Voice.FromHz, 1.f), Along);
			if (Voice.VibratoHz > 0.f)
			{
				Hz *= 1.f + Voice.VibratoDepth * FMath::Sin(UE_TWO_PI * Voice.VibratoHz * Time);
			}
			Phase += Hz / SampleRate;
			float Value;
			if (Voice.Wave == EOnyxWave::Noise)
			{
				if (Phase >= 1.f)
				{
					Held = Random.FRandRange(-1.f, 1.f); // a new random level at the noise's rate
				}
				Value = Held;
			}
			else
			{
				Value = OnyxSynth::Oscillator(Voice.Wave, FMath::Frac(Phase), Voice.Duty);
			}
			Phase = FMath::Frac(Phase);
			// Filters: a resonant band-pass, a two-stage low-pass gliding with the sound, a gentle high-pass.
			if (Voice.BandFrom > 0.f)
			{
				const float Centre = Voice.BandFrom * FMath::Pow(FMath::Max(Voice.BandTo > 0.f ? Voice.BandTo : Voice.BandFrom, 1.f) / Voice.BandFrom, Along);
				const float Tune = 2.f * FMath::Sin(UE_PI * FMath::Min(Centre, SampleRate / 6.f) / SampleRate);
				const float Damping = 1.f / FMath::Max(Voice.Resonance, 0.5f);
				BandLow += Tune * BandMid;
				const float Rest = Value - BandLow - Damping * BandMid;
				BandMid += Tune * Rest;
				Value = BandMid;
			}
			if (Voice.LowFrom > 0.f)
			{
				const float Cutoff = Voice.LowFrom * FMath::Pow(FMath::Max(Voice.LowTo > 0.f ? Voice.LowTo : Voice.LowFrom, 1.f) / Voice.LowFrom, Along);
				const float Coefficient = OnyxSynth::LowPassCoefficient(Cutoff);
				LowA += Coefficient * (Value - LowA);
				LowB += Coefficient * (LowA - LowB);
				Value = LowB;
			}
			if (HighCoefficient > 0.f)
			{
				HighState += HighCoefficient * (Value - HighState);
				Value -= HighState;
			}
			// The envelope: up, held, dying away along its curve.
			float Envelope = 1.f;
			if (Time < Voice.Attack)
			{
				Envelope = Time / FMath::Max(Voice.Attack, 0.0001f);
			}
			else if (Time > Voice.Attack + Voice.Hold)
			{
				const float Left = 1.f - (Time - Voice.Attack - Voice.Hold) / FMath::Max(Voice.Decay, 0.0001f);
				Envelope = FMath::Pow(FMath::Max(Left, 0.f), Voice.DecayCurve);
			}
			if (Voice.TremoloHz > 0.f)
			{
				Envelope *= 1.f - Voice.TremoloDepth * (0.5f + 0.5f * FMath::Sin(UE_TWO_PI * Voice.TremoloHz * Time));
			}
			Mix[First + Index] += Value * Envelope * Voice.Volume;
		}
	}
	if (Recipe.bLooping)
	{
		// The last half second laid over the first, crossfading: no seam where it comes round again.
		const int32 Fade = FMath::Min(SampleRate / 2, Count / 3);
		for (int32 Index = 0; Index < Fade; ++Index)
		{
			const float T = float(Index) / Fade;
			Mix[Index] = Mix[Index] * T + Mix[Count - Fade + Index] * (1.f - T);
		}
		Mix.SetNum(Count - Fade);
	}
	// Loud enough, never clipping harshly; a few levels if crushed; no click at either end.
	float Peak = 0.0001f;
	for (const float Value : Mix)
	{
		Peak = FMath::Max(Peak, FMath::Abs(Value));
	}
	const float Gain = Recipe.Volume / Peak;
	const float Levels = Recipe.Crush > 0.f ? FMath::Lerp(256.f, 6.f, FMath::Clamp(Recipe.Crush, 0.f, 1.f)) : 0.f;
	const int32 Edge = Recipe.bLooping ? 0 : FMath::Min(SampleRate / 500, Mix.Num() / 4);
	for (int32 Index = 0; Index < Mix.Num(); ++Index)
	{
		float Value = FMath::Clamp(Mix[Index] * Gain, -1.f, 1.f);
		if (Levels > 0.f)
		{
			Value = FMath::RoundToFloat(Value * Levels) / Levels;
		}
		if (Index < Edge)
		{
			Value *= float(Index) / Edge;
		}
		else if (Index >= Mix.Num() - Edge)
		{
			Value *= float(Mix.Num() - 1 - Index) / FMath::Max(Edge, 1);
		}
		Mix[Index] = Value;
	}
	return Mix;
}

USoundWave* FOnyxSoundSynth::CreateSound(const FOnyxSoundRecipe& Recipe, const FString& Folder, const FString& Name)
{
	const TArray<float> Samples = Render(Recipe);
	// A 16-bit mono WAV.
	TArray<uint8> Wav;
	const uint32 DataSize = Samples.Num() * 2;
	Wav.Append(reinterpret_cast<const uint8*>("RIFF"), 4);
	OnyxSynth::AppendLittleEndian(Wav, 36 + DataSize, 4);
	Wav.Append(reinterpret_cast<const uint8*>("WAVEfmt "), 8);
	OnyxSynth::AppendLittleEndian(Wav, 16, 4);
	OnyxSynth::AppendLittleEndian(Wav, 1, 2); // PCM
	OnyxSynth::AppendLittleEndian(Wav, 1, 2); // mono
	OnyxSynth::AppendLittleEndian(Wav, SampleRate, 4);
	OnyxSynth::AppendLittleEndian(Wav, SampleRate * 2, 4);
	OnyxSynth::AppendLittleEndian(Wav, 2, 2);
	OnyxSynth::AppendLittleEndian(Wav, 16, 2);
	Wav.Append(reinterpret_cast<const uint8*>("data"), 4);
	OnyxSynth::AppendLittleEndian(Wav, DataSize, 4);
	for (const float Value : Samples)
	{
		OnyxSynth::AppendLittleEndian(Wav, uint32(uint16(int16(FMath::RoundToInt(Value * 32767.f)))), 2);
	}
	const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("OnyxSounds") / Name + TEXT(".wav"));
	if (!FFileHelper::SaveArrayToFile(Wav, *File))
	{
		return nullptr;
	}
	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = File;
	Task->DestinationPath = Folder;
	Task->DestinationName = Name;
	Task->bAutomated = true;
	Task->bReplaceExisting = true;
	Task->bSave = true;
	FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().ImportAssetTasks({ Task });
	USoundWave* Sound = LoadObject<USoundWave>(nullptr, *(Folder / Name + TEXT(".") + Name));
	if (Sound && Sound->bLooping != Recipe.bLooping)
	{
		Sound->Modify();
		Sound->bLooping = Recipe.bLooping;
		Sound->MarkPackageDirty();
		FOnyxAssetUtils::SavePackages({ Sound->GetPackage() });
	}
	return Sound;
}
