// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxSoundCueBuilder.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Builders/OnyxAssetUtils.h"
#include "Builders/OnyxContentKit.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeDelay.h"
#include "Sound/SoundNodeMixer.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeRandom.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"

namespace OnyxSoundCue
{
	float FromDecibels(float Decibels)
	{
		return FMath::Pow(10.f, Decibels / 20.f);
	}

	/** Puts Child in the next input of Parent. */
	void Feed(USoundNode* Parent, USoundNode* Child)
	{
		const int32 Index = Parent->ChildNodes.Num();
		Parent->InsertChildNode(Index);
		if (Parent->ChildNodes.IsValidIndex(Index))
		{
			Parent->ChildNodes[Index] = Child;
		}
	}

	/** Its volume and pitch held at, or varied between, the given values (nothing added when there is nothing to change). */
	USoundNode* Modulate(USoundCue* Cue, USoundNode* Input, FVector2D Pitch, FVector2D Volume)
	{
		if (Pitch.Equals(FVector2D(1.f, 1.f)) && Volume.Equals(FVector2D(1.f, 1.f)))
		{
			return Input;
		}
		USoundNodeModulator* Modulator = Cue->ConstructSoundNode<USoundNodeModulator>(USoundNodeModulator::StaticClass(), false);
		Modulator->PitchMin = FMath::Min(Pitch.X, Pitch.Y);
		Modulator->PitchMax = FMath::Max(Pitch.X, Pitch.Y);
		Modulator->VolumeMin = FMath::Min(Volume.X, Volume.Y);
		Modulator->VolumeMax = FMath::Max(Volume.X, Volume.Y);
		Feed(Modulator, Input);
		return Modulator;
	}
}

USoundWave* FOnyxSoundCueBuilder::ImportFile(const FString& File, const FString& Folder, const FString& Name)
{
	if (!FPaths::FileExists(File))
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
	return LoadObject<USoundWave>(nullptr, *(Folder / Name + TEXT(".") + Name));
}

USoundCue* FOnyxSoundCueBuilder::CreateCue(const FOnyxSoundCueRecipe& Recipe, const FString& Folder, const FString& Name)
{
	using namespace OnyxSoundCue;
	// A cue's graph is made by the audio editor: it must be up before the cue is.
	FModuleManager::Get().LoadModule(TEXT("AudioEditor"));
	if (!USoundCue::GetSoundCueAudioEditor().IsValid())
	{
		return nullptr;
	}
	// Made anew each time: an older cue's nodes and graph aren't worth untangling.
	FOnyxContentKit::ClearAssetPath(Folder / Name);
	USoundCue* Cue = FOnyxAssetUtils::CreateOrReuse<USoundCue>(Folder, Name);
	if (!Cue)
	{
		return nullptr;
	}
	Cue->Modify();

	TArray<USoundNode*> Layers;
	TArray<float> LayerGains;
	for (const FOnyxSoundLayer& Layer : Recipe.Layers)
	{
		// Each take, levelled with the others.
		TArray<USoundNode*> Takes;
		for (const FOnyxSoundTake& Take : Layer.Takes)
		{
			if (!Take.Sound)
			{
				continue;
			}
			USoundNodeWavePlayer* Player = Cue->ConstructSoundNode<USoundNodeWavePlayer>(USoundNodeWavePlayer::StaticClass(), false);
			Player->SetSoundWave(Take.Sound);
			const float Level = FromDecibels(Take.Gain);
			Takes.Add(Modulate(Cue, Player, FVector2D(1.f, 1.f), FVector2D(Level, Level)));
		}
		if (Takes.IsEmpty())
		{
			continue;
		}
		// One of them at random (never the same twice running while others are left), varied a little every time.
		USoundNode* Node = Takes[0];
		if (Takes.Num() > 1)
		{
			USoundNodeRandom* Random = Cue->ConstructSoundNode<USoundNodeRandom>(USoundNodeRandom::StaticClass(), false);
			for (USoundNode* Take : Takes)
			{
				Feed(Random, Take);
			}
			Node = Random;
		}
		Node = Modulate(Cue, Node, Layer.Pitch, Layer.Volume);
		if (Layer.Delay > 0.f)
		{
			USoundNodeDelay* Delay = Cue->ConstructSoundNode<USoundNodeDelay>(USoundNodeDelay::StaticClass(), false);
			Delay->DelayMin = Layer.Delay;
			Delay->DelayMax = Layer.Delay;
			Feed(Delay, Node);
			Node = Delay;
		}
		Layers.Add(Node);
		LayerGains.Add(FromDecibels(Layer.Gain));
	}
	if (Layers.IsEmpty())
	{
		return nullptr;
	}
	if (Layers.Num() == 1)
	{
		Cue->FirstNode = Modulate(Cue, Layers[0], FVector2D(1.f, 1.f), FVector2D(LayerGains[0], LayerGains[0]));
	}
	else
	{
		USoundNodeMixer* Mixer = Cue->ConstructSoundNode<USoundNodeMixer>(USoundNodeMixer::StaticClass(), false);
		for (int32 Index = 0; Index < Layers.Num(); ++Index)
		{
			Feed(Mixer, Layers[Index]);
			if (Mixer->InputVolume.IsValidIndex(Index))
			{
				Mixer->InputVolume[Index] = LayerGains[Index];
			}
		}
		Cue->FirstNode = Mixer;
	}
	Cue->VolumeMultiplier = Recipe.Volume;
	Cue->LinkGraphNodesFromSoundNodes(); // the editor's graph drawn from the nodes, so the cue opens as built
	Cue->PostEditChange();
	Cue->MarkPackageDirty();
	FOnyxAssetUtils::SavePackages({ Cue->GetPackage() });
	return Cue;
}
