// Copyright Onyx2D. All Rights Reserved.

#include "Logic/OnyxAction.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Camera/OnyxCameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/OnyxPlayerControlComponent.h"
#include "Character/OnyxRespawnComponent.h"
#include "Combat/OnyxCombatTypes.h"
#include "Combat/OnyxHealthComponent.h"
#include "Core/OnyxScoreSubsystem.h"
#include "Engine/World.h"
#include "Feedback/OnyxFeedbackSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Lighting/OnyxLight2D.h"
#include "Lighting/OnyxLight2DComponent.h"
#include "Logic/OnyxLogicSubsystem.h"
#include "Logic/OnyxTriggerZone.h"
#include "PaperFlipbook.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "World/OnyxFlipbookEffect.h"
#include "World/OnyxLightSwitch.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxActions, Log, All);

namespace
{
	/** The name people see: the label in the editor, the object name in a packaged game. */
	FString NameOf(const UObject* Object)
	{
		if (!Object)
		{
			return TEXT("nothing");
		}
#if WITH_EDITOR
		if (const AActor* Actor = Cast<AActor>(Object))
		{
			return Actor->GetActorLabel();
		}
#endif
		return Object->GetName();
	}

	FString NamesOf(const TArray<TObjectPtr<AActor>>& Actors)
	{
		TArray<FString> Names;
		for (const AActor* Actor : Actors)
		{
			if (Actor)
			{
				Names.Add(NameOf(Actor));
			}
		}
		if (Names.Num() > 2)
		{
			return FString::Printf(TEXT("%s and %d more"), *Names[0], Names.Num() - 1);
		}
		return Names.Num() > 0 ? FString::Join(Names, TEXT(", ")) : TEXT("nothing");
	}

	bool HasAny(const TArray<TObjectPtr<AActor>>& Actors)
	{
		return Actors.ContainsByPredicate([](const AActor* Actor) { return Actor != nullptr; });
	}

	FString Shorten(const FText& Text, int32 Length = 32)
	{
		const FString String = Text.ToString().Replace(TEXT("\n"), TEXT(" "));
		return String.Len() > Length ? String.Left(Length - 1) + TEXT("…") : String;
	}

	FVector SpotOf(EOnyxActionSpot Where, const FOnyxActionContext& Context)
	{
		const AActor* At = Where == EOnyxActionSpot::AtPlayer ? Context.GetPlayer() : Context.Source.Get();
		return At ? At->GetActorLocation() : FVector::ZeroVector;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Context and base

APawn* FOnyxActionContext::GetPlayer() const
{
	if (APawn* Pawn = Cast<APawn>(Instigator.Get()); Pawn && Pawn->IsPlayerControlled())
	{
		return Pawn;
	}
	UWorld* World = GetWorld();
	return World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
}

UWorld* FOnyxActionContext::GetWorld() const
{
	if (Source)
	{
		return Source->GetWorld();
	}
	return Instigator ? Instigator->GetWorld() : nullptr;
}

void UOnyxAction::Run(const FOnyxActionContext& Context) const
{
	OnRun(Context.Instigator, Context.Source);
}

FString UOnyxAction::Describe() const
{
#if WITH_EDITOR
	return GetClass()->GetDisplayNameText().ToString();
#else
	FString Name = GetClass()->GetName();
	Name.RemoveFromEnd(TEXT("_C"));
	return Name;
#endif
}

UWorld* UOnyxAction::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr; // lets Blueprint actions use world functions
	}
	for (const UObject* Outer = GetOuter(); Outer; Outer = Outer->GetOuter())
	{
		if (const AActor* Actor = Cast<AActor>(Outer))
		{
			return Actor->GetWorld();
		}
	}
	return nullptr;
}

void UOnyxAction::RunAll(const TArray<TObjectPtr<UOnyxAction>>& Actions, const FOnyxActionContext& Context)
{
	if (UOnyxLogicSubsystem* Logic = UOnyxLogicSubsystem::Get(Context.GetWorld()))
	{
		Logic->Run(Actions, Context);
	}
}

FString UOnyxAction::DescribeAll(const TArray<TObjectPtr<UOnyxAction>>& Actions)
{
	TArray<FString> Lines;
	for (const UOnyxAction* Action : Actions)
	{
		if (Action)
		{
			Lines.Add(Action->Delay > 0.f ? FString::Printf(TEXT("wait %gs, %s"), Action->Delay, *Action->Describe()) : Action->Describe());
		}
	}
	return Lines.Num() > 0 ? FString::Join(Lines, TEXT(" → ")) : TEXT("nothing yet");
}

// ---------------------------------------------------------------------------------------------------------------------
// Show Message

void UOnyxAction_ShowMessage::Run(const FOnyxActionContext& Context) const
{
	if (UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(Context.GetWorld()))
	{
		Dialogue->ShowMessage(Message, Duration);
	}
}

FString UOnyxAction_ShowMessage::Describe() const
{
	return FString::Printf(TEXT("Message \"%s\""), *Shorten(Message));
}

FText UOnyxAction_ShowMessage::CheckSetup() const
{
	return Message.IsEmpty() ? INVTEXT("Show Message has no text to show.") : FText::GetEmpty();
}

// ---------------------------------------------------------------------------------------------------------------------
// Dialogue

UOnyxAction_Dialogue::UOnyxAction_Dialogue()
{
	FOnyxDialogueLine& First = Lines.AddDefaulted_GetRef();
	First.Text = INVTEXT("Hello there!");
}

void UOnyxAction_Dialogue::Run(const FOnyxActionContext& Context) const
{
	if (UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(Context.GetWorld()))
	{
		Dialogue->StartDialogue(Lines, LettersPerSecond);
	}
}

bool UOnyxAction_Dialogue::RunAndWait(const FOnyxActionContext& Context, TFunction<void()> OnFinished) const
{
	UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(Context.GetWorld());
	if (!Dialogue)
	{
		return false;
	}
	Dialogue->QueueDialogue(Lines, LettersPerSecond, MoveTemp(OnFinished));
	return true;
}

FString UOnyxAction_Dialogue::Describe() const
{
	if (Lines.Num() == 1)
	{
		return FString::Printf(TEXT("Dialogue \"%s\""), *Shorten(Lines[0].Text));
	}
	return FString::Printf(TEXT("Dialogue (%d lines)"), Lines.Num());
}

FText UOnyxAction_Dialogue::CheckSetup() const
{
	const bool bHasText = Lines.ContainsByPredicate([](const FOnyxDialogueLine& Line) { return !Line.Text.IsEmpty(); });
	return bHasText ? FText::GetEmpty() : INVTEXT("Dialogue has no lines with text.");
}

// ---------------------------------------------------------------------------------------------------------------------
// Play Sound

void UOnyxAction_PlaySound::Run(const FOnyxActionContext& Context) const
{
	if (bWholeLevel)
	{
		UOnyxAudioSubsystem::PlaySound2D(Context.GetWorld(), Sound, EOnyxSoundCategory::Effects, Volume);
		return;
	}
	UOnyxAudioSubsystem::PlaySoundAt(Context.GetWorld(), Sound, SpotOf(Where, Context), EOnyxSoundCategory::Effects, Volume);
}

FString UOnyxAction_PlaySound::Describe() const
{
	return FString::Printf(TEXT("Sound %s"), *NameOf(Sound));
}

FText UOnyxAction_PlaySound::CheckSetup() const
{
	return Sound ? FText::GetEmpty() : INVTEXT("Play Sound has no Sound picked.");
}

// ---------------------------------------------------------------------------------------------------------------------
// Spawn Effect

void UOnyxAction_SpawnEffect::Run(const FOnyxActionContext& Context) const
{
	const FVector At = SpotOf(Where, Context) + FVector(Offset.X, 0.f, Offset.Y);
	AOnyxFlipbookEffect::Spawn(Context.Source ? static_cast<UObject*>(Context.Source) : Context.Instigator.Get(), Effect, At, Scale);
}

FString UOnyxAction_SpawnEffect::Describe() const
{
	return FString::Printf(TEXT("Effect %s"), *NameOf(Effect));
}

FText UOnyxAction_SpawnEffect::CheckSetup() const
{
	return Effect ? FText::GetEmpty() : INVTEXT("Spawn Effect has no Effect flipbook picked.");
}

// ---------------------------------------------------------------------------------------------------------------------
// Teleport Player

void UOnyxAction_Teleport::Run(const FOnyxActionContext& Context) const
{
	APawn* Player = Context.GetPlayer();
	if (!Player || !Destination)
	{
		return;
	}
	Player->SetActorLocation(Destination->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	if (!bKeepSpeed)
	{
		if (UMovementComponent* Movement = Player->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
	}
	// The camera jumps there too, instead of sliding across the level.
	if (UOnyxCameraComponent* Camera = Player->FindComponentByClass<UOnyxCameraComponent>())
	{
		Camera->SnapToTarget();
	}
}

FString UOnyxAction_Teleport::Describe() const
{
	return FString::Printf(TEXT("Teleport to %s"), *NameOf(Destination));
}

FText UOnyxAction_Teleport::CheckSetup() const
{
	return Destination ? FText::GetEmpty() : INVTEXT("Teleport Player has no Destination: pick an actor where the player should appear.");
}

// ---------------------------------------------------------------------------------------------------------------------
// Show or Hide

void UOnyxAction_ShowHide::Run(const FOnyxActionContext& Context) const
{
	for (AActor* Actor : Actors)
	{
		if (!Actor)
		{
			continue;
		}
		const bool bHide = Visible == EOnyxSwitchTo::Toggle ? !Actor->IsHidden() : Visible == EOnyxSwitchTo::Off;
		Actor->SetActorHiddenInGame(bHide);
		Actor->SetActorEnableCollision(!bHide);
	}
}

FString UOnyxAction_ShowHide::Describe() const
{
	const TCHAR* Verb = Visible == EOnyxSwitchTo::On ? TEXT("Show") : Visible == EOnyxSwitchTo::Off ? TEXT("Hide") : TEXT("Show/hide");
	return FString::Printf(TEXT("%s %s"), Verb, *NamesOf(Actors));
}

FText UOnyxAction_ShowHide::CheckSetup() const
{
	return HasAny(Actors) ? FText::GetEmpty() : INVTEXT("Show or Hide has no Actors: add the ones to show or hide.");
}

// ---------------------------------------------------------------------------------------------------------------------
// Move

void UOnyxAction_Move::Run(const FOnyxActionContext& Context) const
{
	RunAndWait(Context, nullptr);
}

bool UOnyxAction_Move::RunAndWait(const FOnyxActionContext& Context, TFunction<void()> OnFinished) const
{
	UOnyxLogicSubsystem* Logic = UOnyxLogicSubsystem::Get(Context.GetWorld());
	if (!Logic || !HasAny(Actors))
	{
		return false;
	}
	const bool bWait = bWaitUntilDone && OnFinished;
	// The actions below carry on when the last actor arrives.
	TSharedRef<int32> Moving = MakeShared<int32>(0);
	TSharedRef<TFunction<void()>> Finished = MakeShared<TFunction<void()>>(bWait ? MoveTemp(OnFinished) : nullptr);
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			++*Moving;
		}
	}
	for (AActor* Actor : Actors)
	{
		if (!Actor)
		{
			continue;
		}
		Logic->MoveActor(Actor, Actor->GetActorLocation() + FVector(Offset.X, 0.f, Offset.Y), Time, [Moving, Finished]()
		{
			if (--*Moving == 0 && *Finished)
			{
				(*Finished)();
			}
		});
	}
	return bWait;
}

FString UOnyxAction_Move::Describe() const
{
	return FString::Printf(TEXT("Move %s by (%g, %g)"), *NamesOf(Actors), Offset.X, Offset.Y);
}

FText UOnyxAction_Move::CheckSetup() const
{
	return HasAny(Actors) ? FText::GetEmpty() : INVTEXT("Move has no Actors: add the ones that should slide.");
}

// ---------------------------------------------------------------------------------------------------------------------
// Give

void UOnyxAction_Give::Run(const FOnyxActionContext& Context) const
{
	if (What == EOnyxGiveWhat::Health)
	{
		const APawn* Player = Context.GetPlayer();
		if (UOnyxHealthComponent* Health = Player ? Player->FindComponentByClass<UOnyxHealthComponent>() : nullptr)
		{
			Health->Heal(Amount);
		}
		return;
	}
	if (UOnyxScoreSubsystem* Score = UOnyxScoreSubsystem::Get(Context.GetWorld()))
	{
		if (What == EOnyxGiveWhat::Coins)
		{
			Score->AddCoins(Amount);
		}
		else
		{
			Score->AddItem(Item, Amount);
		}
	}
}

FString UOnyxAction_Give::Describe() const
{
	switch (What)
	{
	case EOnyxGiveWhat::Coins: return FString::Printf(TEXT("Give %d coin%s"), Amount, Amount == 1 ? TEXT("") : TEXT("s"));
	case EOnyxGiveWhat::Health: return FString::Printf(TEXT("Give %d health"), Amount);
	default: return FString::Printf(TEXT("Give %d %s"), Amount, *Item.ToString());
	}
}

FText UOnyxAction_Give::CheckSetup() const
{
	return What == EOnyxGiveWhat::Item && Item.IsNone() ? INVTEXT("Give has no Item name.") : FText::GetEmpty();
}

// ---------------------------------------------------------------------------------------------------------------------
// Hurt Player

void UOnyxAction_Hurt::Run(const FOnyxActionContext& Context) const
{
	APawn* Player = Context.GetPlayer();
	if (!Player)
	{
		return;
	}
	if (bDefeat)
	{
		if (UOnyxHealthComponent* Health = Player->FindComponentByClass<UOnyxHealthComponent>())
		{
			Health->Kill(Context.Source);
		}
		return;
	}
	if (Player->Implements<UOnyxDamageable>())
	{
		FOnyxDamageInfo Hit;
		Hit.Amount = Damage;
		Hit.Instigator = Context.Source;
		Hit.DamageType = TEXT("Trap");
		IOnyxDamageable::Execute_ReceiveDamage(Player, Hit);
	}
}

FString UOnyxAction_Hurt::Describe() const
{
	return bDefeat ? TEXT("Defeat the player") : FString::Printf(TEXT("Hurt the player (%g)"), Damage);
}

// ---------------------------------------------------------------------------------------------------------------------
// Open Level

void UOnyxAction_OpenLevel::Run(const FOnyxActionContext& Context) const
{
	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return;
	}
	// Freeze the player and fade out, like a Level Exit.
	if (APawn* Player = Context.GetPlayer())
	{
		if (UOnyxPlayerControlComponent* Control = Player->FindComponentByClass<UOnyxPlayerControlComponent>())
		{
			Control->SetControlEnabled(false);
		}
		if (const APlayerController* PC = Cast<APlayerController>(Player->GetController()); PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, FadeTime, FLinearColor::Black, false, true);
		}
	}
	const TSoftObjectPtr<UWorld> Target = Level;
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(World, [World, Target]()
	{
		if (Target.IsNull())
		{
			UGameplayStatics::OpenLevel(World, FName(*UGameplayStatics::GetCurrentLevelName(World)));
		}
		else
		{
			UGameplayStatics::OpenLevelBySoftObjectPtr(World, Target);
		}
	}), FMath::Max(FadeTime, 0.01f), false);
}

FString UOnyxAction_OpenLevel::Describe() const
{
	return Level.IsNull() ? TEXT("Restart the level") : FString::Printf(TEXT("Open %s"), *Level.GetAssetName());
}

// ---------------------------------------------------------------------------------------------------------------------
// Shake Camera

void UOnyxAction_ShakeCamera::Run(const FOnyxActionContext& Context) const
{
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(Context.GetWorld()))
	{
		Feedback->AddTrauma(Strength);
	}
}

FString UOnyxAction_ShakeCamera::Describe() const
{
	return TEXT("Shake the camera");
}

// ---------------------------------------------------------------------------------------------------------------------
// Switch Lights

void UOnyxAction_SwitchLights::Run(const FOnyxActionContext& Context) const
{
	for (AActor* Target : Lights)
	{
		if (!Target)
		{
			continue;
		}
		if (AOnyxLightSwitch* Switch = Cast<AOnyxLightSwitch>(Target))
		{
			Switch->SetOn(SwitchTo == EOnyxSwitchTo::Toggle ? !Switch->IsOn() : SwitchTo == EOnyxSwitchTo::On);
			continue;
		}
		if (AOnyxLight2D* Light = Cast<AOnyxLight2D>(Target))
		{
			Light->SetLightEnabled(SwitchTo == EOnyxSwitchTo::Toggle ? !Light->IsLightEnabled() : SwitchTo == EOnyxSwitchTo::On, FadeTime);
			continue;
		}
		TArray<UOnyxLight2DComponent*> Components;
		Target->GetComponents(Components);
		for (UOnyxLight2DComponent* Component : Components)
		{
			Component->SetLightEnabled(SwitchTo == EOnyxSwitchTo::Toggle ? !Component->IsLightEnabled() : SwitchTo == EOnyxSwitchTo::On, FadeTime);
		}
	}
}

FString UOnyxAction_SwitchLights::Describe() const
{
	const TCHAR* State = SwitchTo == EOnyxSwitchTo::On ? TEXT("on") : SwitchTo == EOnyxSwitchTo::Off ? TEXT("off") : TEXT("over");
	return FString::Printf(TEXT("Switch %s %s"), *NamesOf(Lights), State);
}

FText UOnyxAction_SwitchLights::CheckSetup() const
{
	return HasAny(Lights) ? FText::GetEmpty() : INVTEXT("Switch Lights has no Lights: add the lights or switches to control.");
}

// ---------------------------------------------------------------------------------------------------------------------
// Launch Player

void UOnyxAction_Launch::Run(const FOnyxActionContext& Context) const
{
	if (ACharacter* Character = Cast<ACharacter>(Context.GetPlayer()))
	{
		Character->LaunchCharacter(FVector(Velocity.X, 0.f, Velocity.Y), bReplaceSideways, true);
	}
}

FString UOnyxAction_Launch::Describe() const
{
	return FString::Printf(TEXT("Launch the player (%g, %g)"), Velocity.X, Velocity.Y);
}

// ---------------------------------------------------------------------------------------------------------------------
// Set Checkpoint

void UOnyxAction_SetCheckpoint::Run(const FOnyxActionContext& Context) const
{
	const APawn* Player = Context.GetPlayer();
	if (UOnyxRespawnComponent* Respawn = Player ? Player->FindComponentByClass<UOnyxRespawnComponent>() : nullptr)
	{
		Respawn->SetRespawnLocation(SpotOf(Where, Context) + FVector(0.f, 0.f, 20.f));
	}
}

FString UOnyxAction_SetCheckpoint::Describe() const
{
	return TEXT("Set the checkpoint");
}

// ---------------------------------------------------------------------------------------------------------------------
// Play Music

void UOnyxAction_PlayMusic::Run(const FOnyxActionContext& Context) const
{
	if (UOnyxAudioSubsystem* Audio = UOnyxAudioSubsystem::Get(Context.GetWorld()))
	{
		if (Music)
		{
			Audio->SetLevelMusic(Music, FadeTime);
		}
		else
		{
			Audio->StopMusic(FadeTime);
		}
	}
}

FString UOnyxAction_PlayMusic::Describe() const
{
	return Music ? FString::Printf(TEXT("Music %s"), *NameOf(Music)) : TEXT("Stop the music");
}

// ---------------------------------------------------------------------------------------------------------------------
// Call Event

void UOnyxAction_CallEvent::Run(const FOnyxActionContext& Context) const
{
	TArray<AActor*> Targets;
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Targets.Add(Actor);
		}
	}
	if (Targets.Num() == 0 && Context.Source)
	{
		Targets.Add(Context.Source);
	}
	for (AActor* Target : Targets)
	{
		UFunction* Function = Target->FindFunction(Event);
		if (!Function)
		{
			UE_LOG(LogOnyxActions, Warning, TEXT("Call Event: %s has no function or custom event called %s."), *NameOf(Target), *Event.ToString());
			continue;
		}
		if (Function->ParmsSize > 0)
		{
			UE_LOG(LogOnyxActions, Warning, TEXT("Call Event: %s.%s has inputs; Call Event only calls events without inputs."), *NameOf(Target), *Event.ToString());
			continue;
		}
		Target->ProcessEvent(Function, nullptr);
	}
}

FString UOnyxAction_CallEvent::Describe() const
{
	return HasAny(Actors) ? FString::Printf(TEXT("Call %s on %s"), *Event.ToString(), *NamesOf(Actors)) : FString::Printf(TEXT("Call %s"), *Event.ToString());
}

FText UOnyxAction_CallEvent::CheckSetup() const
{
	return Event.IsNone() ? INVTEXT("Call Event has no Event name.") : FText::GetEmpty();
}

// ---------------------------------------------------------------------------------------------------------------------
// Control Zones

void UOnyxAction_ControlZones::Run(const FOnyxActionContext& Context) const
{
	for (AOnyxTriggerZone* Zone : Zones)
	{
		if (!Zone)
		{
			continue;
		}
		switch (Command)
		{
		case EOnyxZoneCommand::Fire: Zone->Fire(Context.Instigator); break;
		case EOnyxZoneCommand::TurnOn: Zone->SetZoneOn(true); break;
		case EOnyxZoneCommand::TurnOff: Zone->SetZoneOn(false); break;
		}
	}
}

FString UOnyxAction_ControlZones::Describe() const
{
	TArray<TObjectPtr<AActor>> Actors;
	for (AOnyxTriggerZone* Zone : Zones)
	{
		Actors.Add(Zone);
	}
	const TCHAR* Verb = Command == EOnyxZoneCommand::Fire ? TEXT("Fire") : Command == EOnyxZoneCommand::TurnOn ? TEXT("Turn on") : TEXT("Turn off");
	return FString::Printf(TEXT("%s %s"), Verb, *NamesOf(Actors));
}

FText UOnyxAction_ControlZones::CheckSetup() const
{
	const bool bAny = Zones.ContainsByPredicate([](const AOnyxTriggerZone* Zone) { return Zone != nullptr; });
	return bAny ? FText::GetEmpty() : INVTEXT("Control Zones has no Zones: add the trigger zones to control.");
}
