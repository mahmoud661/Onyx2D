// Copyright Onyx2D. All Rights Reserved.

#include "Audio/OnyxLevelAudio.h"
#include "Sound/SoundBase.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"

AOnyxLevelAudio::AOnyxLevelAudio()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITORONLY_DATA
	Icon = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Icon"));
	if (Icon)
	{
		Icon->SetupAttachment(RootComponent);
		Icon->bIsScreenSizeScaled = true;
	}
#endif
}

void AOnyxLevelAudio::BeginPlay()
{
	Super::BeginPlay();
	if (UOnyxAudioSubsystem* Audio = UOnyxAudioSubsystem::Get(this))
	{
		Audio->SetLevelMusic(Music, FadeTime, MusicVolume);
		Audio->SetLevelAmbience(Ambience, FadeTime, AmbienceVolume);
	}
}
