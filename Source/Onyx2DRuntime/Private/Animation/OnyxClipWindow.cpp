// Copyright Onyx2D. All Rights Reserved.

#include "Animation/OnyxClipWindow.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Combat/OnyxContactDamageComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "Combat/OnyxHitboxComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundBase.h"

UOnyxClipWindow::UOnyxClipWindow()
{
	TimelineColor = FLinearColor(0.85f, 0.25f, 0.25f);
}

void UOnyxClipWindow::Open(AActor* Owner, UOnyxMovesetComponent* Moveset) const
{
	OnOpen(Owner, Moveset);
}

void UOnyxClipWindow::Update(AActor* Owner, UOnyxMovesetComponent* Moveset, float DeltaTime) const
{
	OnUpdate(Owner, Moveset, DeltaTime);
}

void UOnyxClipWindow::Close(AActor* Owner, UOnyxMovesetComponent* Moveset) const
{
	OnClose(Owner, Moveset);
}

namespace
{
	/** The hitbox a Hitbox window opens: by name, else the first one that isn't contact damage. */
	UOnyxHitboxComponent* FindWindowHitbox(AActor* Owner, FName Name)
	{
		if (!Owner)
		{
			return nullptr;
		}
		TArray<UOnyxHitboxComponent*> Hitboxes;
		Owner->GetComponents(Hitboxes);
		for (UOnyxHitboxComponent* Hitbox : Hitboxes)
		{
			if (Name.IsNone() ? !Hitbox->IsA<UOnyxContactDamageComponent>() : Hitbox->GetFName() == Name)
			{
				return Hitbox;
			}
		}
		return nullptr;
	}
}

UOnyxClipWindow_Hitbox::UOnyxClipWindow_Hitbox()
{
	TimelineColor = FLinearColor(0.9f, 0.22f, 0.22f);
}

void UOnyxClipWindow_Hitbox::Open(AActor* Owner, UOnyxMovesetComponent* Moveset) const
{
	if (UOnyxHitboxComponent* Window = FindWindowHitbox(Owner, Hitbox))
	{
		Window->BeginHitWindow();
	}
	Super::Open(Owner, Moveset);
}

void UOnyxClipWindow_Hitbox::Close(AActor* Owner, UOnyxMovesetComponent* Moveset) const
{
	if (UOnyxHitboxComponent* Window = FindWindowHitbox(Owner, Hitbox))
	{
		Window->EndHitWindow();
	}
	Super::Close(Owner, Moveset);
}

UOnyxClipWindow_Invulnerable::UOnyxClipWindow_Invulnerable()
{
	TimelineColor = FLinearColor(0.35f, 0.8f, 0.95f);
}

void UOnyxClipWindow_Invulnerable::Open(AActor* Owner, UOnyxMovesetComponent* Moveset) const
{
	if (UOnyxHealthComponent* Health = Owner ? Owner->FindComponentByClass<UOnyxHealthComponent>() : nullptr)
	{
		Health->SetInvulnerableFor(Length);
	}
	Super::Open(Owner, Moveset);
}

UOnyxClipWindow_LoopSound::UOnyxClipWindow_LoopSound()
{
	TimelineColor = FLinearColor(1.f, 0.62f, 0.18f);
}

void UOnyxClipWindow_LoopSound::Open(AActor* Owner, UOnyxMovesetComponent* Moveset) const
{
	if (Owner && Sound && Owner->GetRootComponent())
	{
		UOnyxAudioSubsystem::PlaySoundAttached(Sound, Owner->GetRootComponent(), EOnyxSoundCategory::Effects, Volume);
	}
	Super::Open(Owner, Moveset);
}

void UOnyxClipWindow_LoopSound::Close(AActor* Owner, UOnyxMovesetComponent* Moveset) const
{
	// The window is shared by every character playing the clip: find this character's copy of the sound.
	if (Owner && Sound)
	{
		TArray<UAudioComponent*> Sounds;
		Owner->GetComponents(Sounds);
		for (UAudioComponent* Playing : Sounds)
		{
			if (Playing->Sound == Sound && Playing->IsPlaying())
			{
				Playing->FadeOut(FMath::Max(FadeOut, 0.01f), 0.f);
			}
		}
	}
	Super::Close(Owner, Moveset);
}

FText UOnyxClipWindow_LoopSound::GetLabel() const
{
	return Sound ? FText::FromString(Sound->GetName()) : INVTEXT("Loop Sound");
}
