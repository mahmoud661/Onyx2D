// Copyright Onyx2D. All Rights Reserved.

#include "Animation/OnyxClipEvent.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxMoveset.h"
#include "Animation/OnyxMovesetComponent.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Feedback/OnyxFeedbackSubsystem.h"
#include "GameFramework/Actor.h"
#include "Lighting/OnyxLightFlash.h"
#include "Lighting/OnyxLightingSubsystem.h"
#include "PaperFlipbook.h"
#include "Sound/SoundBase.h"
#include "World/OnyxFlipbookEffect.h"

void UOnyxClipEvent::Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const
{
	OnFired(Owner, Moveset);
}

FText UOnyxClipEvent::GetLabel() const
{
	return GetClass()->GetDisplayNameText();
}

UOnyxClip* UOnyxClipEvent::GetClip() const
{
	return GetTypedOuter<UOnyxClip>();
}

UOnyxClipEvent_Message::UOnyxClipEvent_Message()
{
	TimelineColor = FLinearColor(0.22f, 0.71f, 1.f);
}

void UOnyxClipEvent_Message::Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const
{
	if (Owner && !Message.IsNone())
	{
		// A function or custom event of that name on the actor, without parameters.
		if (UFunction* Function = Owner->FindFunction(Message); Function && Function->ParmsSize == 0)
		{
			Owner->ProcessEvent(Function, nullptr);
		}
	}
	if (Moveset)
	{
		Moveset->OnClipMessage.Broadcast(Message);
	}
	Super::Fire(Owner, Moveset);
}

TArray<FString> UOnyxClipEvent_Message::GetMessageOptions() const
{
	TArray<FString> Options;
	if (const UOnyxClip* Clip = GetClip())
	{
		if (const UOnyxMoveset* Moveset = Clip->Moveset.LoadSynchronous())
		{
			for (const FName& Name : Moveset->Messages)
			{
				Options.Add(Name.ToString());
			}
		}
	}
	if (!Message.IsNone() && !Options.Contains(Message.ToString()))
	{
		Options.Add(Message.ToString());
	}
	return Options;
}

UOnyxClipEvent_Sound::UOnyxClipEvent_Sound()
{
	TimelineColor = FLinearColor(1.f, 0.76f, 0.2f);
}

void UOnyxClipEvent_Sound::Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const
{
	if (Owner && Sound)
	{
		const float RandomPitch = Pitch * (1.f + FMath::FRandRange(-PitchVariation, PitchVariation));
		if (bFollowCharacter && Owner->GetRootComponent())
		{
			UOnyxAudioSubsystem::PlaySoundAttached(Sound, Owner->GetRootComponent(), EOnyxSoundCategory::Effects, Volume, RandomPitch);
		}
		else
		{
			UOnyxAudioSubsystem::PlaySoundAt(Owner, Sound, Owner->GetActorLocation(), EOnyxSoundCategory::Effects, Volume, RandomPitch);
		}
	}
	Super::Fire(Owner, Moveset);
}

FText UOnyxClipEvent_Sound::GetLabel() const
{
	return Sound ? FText::FromString(Sound->GetName()) : INVTEXT("Sound");
}

UOnyxClipEvent_Effect::UOnyxClipEvent_Effect()
{
	TimelineColor = FLinearColor(0.75f, 0.45f, 1.f);
}

void UOnyxClipEvent_Effect::Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const
{
	if (Owner && Effect)
	{
		const float Facing = Moveset ? Moveset->GetFacingSign() : 1.f;
		const FVector Location = Owner->GetActorLocation() + FVector(Offset.X * Facing, 0.f, Offset.Y);
		AOnyxFlipbookEffect::Spawn(Owner, Effect, Location, Scale);
	}
	Super::Fire(Owner, Moveset);
}

FText UOnyxClipEvent_Effect::GetLabel() const
{
	return Effect ? FText::FromString(Effect->GetName()) : INVTEXT("Effect");
}

UOnyxClipEvent_CameraShake::UOnyxClipEvent_CameraShake()
{
	TimelineColor = FLinearColor(1.f, 0.4f, 0.35f);
}

void UOnyxClipEvent_CameraShake::Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const
{
	if (UOnyxFeedbackSubsystem* Feedback = Owner ? UOnyxFeedbackSubsystem::Get(Owner) : nullptr)
	{
		Feedback->AddTrauma(Strength);
	}
	Super::Fire(Owner, Moveset);
}

UOnyxClipEvent_LightFlash::UOnyxClipEvent_LightFlash()
{
	TimelineColor = FLinearColor(1.f, 0.9f, 0.4f);
}

void UOnyxClipEvent_LightFlash::Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const
{
	if (Owner)
	{
		const float Facing = Moveset ? Moveset->GetFacingSign() : 1.f;
		const FVector Location = Owner->GetActorLocation() + FVector(Offset.X * Facing, 0.f, Offset.Y);
		AOnyxLightFlash::Spawn(Owner, Location, Color, Intensity, Radius, Duration);
	}
	Super::Fire(Owner, Moveset);
}

UOnyxClipEvent_SceneFlash::UOnyxClipEvent_SceneFlash()
{
	TimelineColor = FLinearColor(0.85f, 0.9f, 1.f);
}

void UOnyxClipEvent_SceneFlash::Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const
{
	if (UOnyxLightingSubsystem* Lighting = UOnyxLightingSubsystem::Get(Owner))
	{
		Lighting->Flash(Color, Intensity, Duration);
	}
	Super::Fire(Owner, Moveset);
}
