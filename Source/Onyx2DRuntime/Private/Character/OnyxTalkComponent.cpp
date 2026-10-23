// Copyright Onyx2D. All Rights Reserved.

#include "Character/OnyxTalkComponent.h"
#include "AI/OnyxPatrolComponent.h"
#include "Character/OnyxCharacter.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Logic/OnyxAction.h"
#include "Logic/OnyxDialogueSubsystem.h"
#include "Sound/SoundBase.h"

UOnyxTalkComponent::UOnyxTalkComponent()
{
	InitSphereRadius(90.f);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SetCanEverAffectNavigation(false);
	ShapeColor = FColor(120, 220, 255);
	Lines.Add(INVTEXT("Hello! Nice day for an adventure."));
}

void UOnyxTalkComponent::Talk(AActor* Listener)
{
	UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(this);
	if (!Dialogue || bTalking)
	{
		return;
	}
	const TArray<FText>& Said = TimesTalked > 0 && LaterLines.Num() > 0 ? LaterLines : Lines;
	TArray<FOnyxDialogueLine> Conversation;
	for (const FText& Text : Said)
	{
		FOnyxDialogueLine& Line = Conversation.AddDefaulted_GetRef();
		Line.Speaker = SpeakerName;
		Line.Text = Text;
		Line.Portrait = Portrait;
		Line.Voice = Voice;
	}

	AActor* Owner = GetOwner();
	if (AOnyxCharacter* Character = Cast<AOnyxCharacter>(Owner); Character && Listener && bFacePlayer)
	{
		Character->SetFacing(Listener->GetActorLocation().X < Character->GetActorLocation().X ? EOnyxFacing::Left : EOnyxFacing::Right);
	}
	// Stand still while talking.
	if (UOnyxPatrolComponent* Patrol = Owner ? Owner->FindComponentByClass<UOnyxPatrolComponent>() : nullptr)
	{
		Patrol->SetComponentTickEnabled(false);
	}
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (UMovementComponent* Movement = Pawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
	}

	bTalking = true;
	OnTalkStarted.Broadcast(Listener);
	TWeakObjectPtr<UOnyxTalkComponent> WeakThis(this);
	TWeakObjectPtr<AActor> WeakListener(Listener);
	Dialogue->QueueDialogue(Conversation, 45.f, [WeakThis, WeakListener]()
	{
		if (UOnyxTalkComponent* Talker = WeakThis.Get())
		{
			Talker->FinishTalk(WeakListener.Get());
		}
	});
}

void UOnyxTalkComponent::FinishTalk(AActor* Listener)
{
	bTalking = false;
	if (UOnyxPatrolComponent* Patrol = GetOwner() ? GetOwner()->FindComponentByClass<UOnyxPatrolComponent>() : nullptr)
	{
		Patrol->SetComponentTickEnabled(true);
	}
	if (TimesTalked++ == 0)
	{
		FOnyxActionContext Context;
		Context.Instigator = Listener;
		Context.Source = GetOwner();
		UOnyxAction::RunAll(AfterFirstTalk, Context);
	}
	OnTalkFinished.Broadcast(Listener);
}

void UOnyxTalkComponent::Interact_Implementation(AActor* User)
{
	Talk(User);
}

bool UOnyxTalkComponent::IsInteractionLocked_Implementation(AActor* User) const
{
	return bTalking || Lines.Num() == 0 || User == GetOwner();
}

FText UOnyxTalkComponent::GetInteractionPrompt_Implementation() const
{
	return Prompt;
}
