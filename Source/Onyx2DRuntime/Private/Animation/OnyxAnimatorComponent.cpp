// Copyright Onyx2D. All Rights Reserved.

#include "Animation/OnyxAnimatorComponent.h"
#include "GameFramework/Actor.h"
#include "Movement/OnyxMovementComponent.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"

UOnyxAnimatorComponent::UOnyxAnimatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UOnyxAnimatorComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Target && GetOwner())
	{
		Target = GetOwner()->FindComponentByClass<UPaperFlipbookComponent>();
	}
}

bool UOnyxAnimatorComponent::HasAnimation(FName Name) const
{
	const FOnyxAnimation* Animation = Animations.Find(Name);
	return Animation && Animation->Flipbook;
}

float UOnyxAnimatorComponent::GetAnimationLength(FName Name) const
{
	const FOnyxAnimation* Animation = Animations.Find(Name);
	if (!Animation || !Animation->Flipbook)
	{
		return 0.f;
	}
	return Animation->Flipbook->GetTotalDuration() / FMath::Max(Animation->PlayRate, 0.01f);
}

bool UOnyxAnimatorComponent::PlayAnimation(FName Name)
{
	return !bOneShot && ApplyAnimation(Name, false);
}

float UOnyxAnimatorComponent::PlayOneShot(FName Name)
{
	if (!ApplyAnimation(Name, true))
	{
		return 0.f;
	}
	bOneShot = true;
	OneShotRemaining = GetAnimationLength(Name);
	Target->SetLooping(false);
	return OneShotRemaining;
}

void UOnyxAnimatorComponent::StopOneShot()
{
	if (!bOneShot)
	{
		return;
	}
	const FName Finished = CurrentName;
	bOneShot = false;
	OneShotRemaining = 0.f;
	CurrentName = NAME_None; // let locomotion pick the next animation
	OnOneShotFinished.Broadcast(Finished);
}

bool UOnyxAnimatorComponent::ApplyAnimation(FName Name, bool bForceRestart)
{
	const FOnyxAnimation* Animation = Animations.Find(Name);
	if (!Target || !Animation || !Animation->Flipbook)
	{
		return false;
	}
	if (Name == CurrentName && !bForceRestart)
	{
		return true;
	}

	Target->SetFlipbook(Animation->Flipbook);
	Target->SetPlayRate(Animation->PlayRate);
	Target->SetLooping(Animation->bLoop);
	Target->PlayFromStart();
	CurrentName = Name;
	LastFrame = INDEX_NONE;
	return true;
}

FName UOnyxAnimatorComponent::ResolveLocomotionAnimation_Implementation(EOnyxLocomotion State) const
{
	static const FName Idle(TEXT("Idle")), Run(TEXT("Run")), Jump(TEXT("Jump")), Fall(TEXT("Fall")), WallSlide(TEXT("WallSlide")), Dash(TEXT("Dash")), Crouch(TEXT("Crouch"));

	auto FirstAvailable = [this](std::initializer_list<FName> Candidates)
	{
		for (const FName& Candidate : Candidates)
		{
			if (HasAnimation(Candidate))
			{
				return Candidate;
			}
		}
		return FName(TEXT("Idle"));
	};

	switch (State)
	{
	case EOnyxLocomotion::Run:       return FirstAvailable({ Run, Idle });
	case EOnyxLocomotion::Jump:      return FirstAvailable({ Jump, Fall, Idle });
	case EOnyxLocomotion::Fall:      return FirstAvailable({ Fall, Jump, Idle });
	case EOnyxLocomotion::WallSlide: return FirstAvailable({ WallSlide, Fall, Idle });
	case EOnyxLocomotion::Dash:      return FirstAvailable({ Dash, Run, Idle });
	case EOnyxLocomotion::Crouch:    return FirstAvailable({ Crouch, Idle });
	default:                           return Idle;
	}
}

void UOnyxAnimatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Target)
	{
		return;
	}

	FireFrameEvents();

	if (bOneShot)
	{
		OneShotRemaining -= DeltaTime;
		if (OneShotRemaining <= 0.f)
		{
			StopOneShot();
		}
		return;
	}

	if (bAutoLocomotion)
	{
		if (const UOnyxMovementComponent* Movement = GetOwner()->FindComponentByClass<UOnyxMovementComponent>())
		{
			ApplyAnimation(ResolveLocomotionAnimation(Movement->GetLocomotionState()), false);
		}
	}
}

void UOnyxAnimatorComponent::FireFrameEvents()
{
	if (FrameEvents.IsEmpty() || CurrentName.IsNone())
	{
		return;
	}

	const int32 Frame = Target->GetPlaybackPositionInFrames();
	if (Frame == LastFrame)
	{
		return;
	}

	for (const FOnyxFrameEvent& Event : FrameEvents)
	{
		// Fire for every frame crossed since last tick (handles low frame rates and loops).
		const bool bCrossed = LastFrame == INDEX_NONE || Frame < LastFrame
			? Event.Frame <= Frame
			: Event.Frame > LastFrame && Event.Frame <= Frame;
		if (Event.Animation == CurrentName && bCrossed)
		{
			OnFrameEvent.Broadcast(Event.EventName);
		}
	}
	LastFrame = Frame;
}
