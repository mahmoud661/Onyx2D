// Copyright Onyx2D. All Rights Reserved.

#include "Animation/OnyxMovesetComponent.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxClipEvent.h"
#include "Animation/OnyxClipWindow.h"
#include "Animation/OnyxMoveset.h"
#include "GameFramework/Actor.h"
#include "Movement/OnyxMovementComponent.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"

UOnyxMovesetComponent::UOnyxMovesetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UOnyxMovesetComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Target && GetOwner())
	{
		Target = GetOwner()->FindComponentByClass<UPaperFlipbookComponent>();
	}
}

UOnyxClip* UOnyxMovesetComponent::GetCurrentClip() const
{
	return CurrentClip;
}

UOnyxClip* UOnyxMovesetComponent::FindPlayableClip(FName Role) const
{
	UOnyxClip* Clip = Moveset ? Moveset->FindClip(Role) : nullptr;
	return Clip && Clip->Flipbook ? Clip : nullptr;
}

bool UOnyxMovesetComponent::HasClip(FName Role) const
{
	return FindPlayableClip(Role) != nullptr;
}

float UOnyxMovesetComponent::GetClipLength(FName Role) const
{
	const UOnyxClip* Clip = FindPlayableClip(Role);
	return Clip ? Clip->GetLength() / FMath::Max(Clip->PlayRate, 0.01f) : 0.f;
}

bool UOnyxMovesetComponent::PlayClip(FName Role)
{
	return !bOneShot && ApplyClip(Role, false);
}

float UOnyxMovesetComponent::PlayOneShot(FName Role)
{
	if (!ApplyClip(Role, true))
	{
		return 0.f;
	}
	bOneShot = true;
	OneShotRemaining = GetClipLength(Role);
	Target->SetLooping(false);
	return OneShotRemaining;
}

void UOnyxMovesetComponent::StopOneShot()
{
	if (!bOneShot)
	{
		return;
	}
	const FName Finished = CurrentRole;
	bOneShot = false;
	OneShotRemaining = 0.f;
	CurrentRole = NAME_None; // let locomotion pick the next clip
	OnOneShotFinished.Broadcast(Finished);
}

bool UOnyxMovesetComponent::ApplyClip(FName Role, bool bForceRestart)
{
	UOnyxClip* Clip = FindPlayableClip(Role);
	if (!Target || !Clip)
	{
		return false;
	}
	if (Role == CurrentRole && !bForceRestart)
	{
		return true;
	}

	CloseOpenWindows(); // the old clip's windows close before the new one starts
	Target->SetFlipbook(Clip->Flipbook);
	Target->SetPlayRate(Clip->PlayRate);
	Target->SetLooping(Clip->bLoop);
	Target->PlayFromStart();
	CurrentRole = Role;
	CurrentClip = Clip;
	LastEventTime = -1.f; // events at time 0 fire too
	return true;
}

FName UOnyxMovesetComponent::ResolveLocomotionClip_Implementation(EOnyxLocomotion State) const
{
	static const FName Idle(TEXT("Idle")), Run(TEXT("Run")), Jump(TEXT("Jump")), Fall(TEXT("Fall")), WallSlide(TEXT("WallSlide")), Dash(TEXT("Dash")), Crouch(TEXT("Crouch"));

	auto FirstAvailable = [this](std::initializer_list<FName> Candidates)
	{
		for (const FName& Candidate : Candidates)
		{
			if (HasClip(Candidate))
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
	default:                         return Idle;
	}
}

void UOnyxMovesetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Target)
	{
		return;
	}

	FireEvents(DeltaTime);

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
			ApplyClip(ResolveLocomotionClip(Movement->GetLocomotionState()), false);
		}
	}
}

void UOnyxMovesetComponent::CloseOpenWindows()
{
	TArray<TWeakObjectPtr<const UOnyxClipWindow>> Closing = MoveTemp(OpenWindows);
	OpenWindows.Reset();
	for (const TWeakObjectPtr<const UOnyxClipWindow>& Window : Closing)
	{
		if (const UOnyxClipWindow* Open = Window.Get())
		{
			Open->Close(GetOwner(), this);
		}
	}
}

void UOnyxMovesetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseOpenWindows();
	Super::EndPlay(EndPlayReason);
}

void UOnyxMovesetComponent::FireEvents(float DeltaTime)
{
	if (!CurrentClip || CurrentClip->Events.IsEmpty())
	{
		return;
	}
	const float Now = Target->GetPlaybackPosition();

	// Windows: close those we left, open those we entered, update the rest.
	TArray<const UOnyxClipWindow*> Inside;
	CurrentClip->GetWindowsAt(Now, Inside);
	for (int32 Index = OpenWindows.Num() - 1; Index >= 0; --Index)
	{
		const UOnyxClipWindow* Window = OpenWindows[Index].Get();
		if (!Window || !Inside.Contains(Window))
		{
			OpenWindows.RemoveAt(Index);
			if (Window)
			{
				Window->Close(GetOwner(), this);
			}
		}
	}
	for (const UOnyxClipWindow* Window : Inside)
	{
		if (OpenWindows.Contains(Window))
		{
			Window->Update(GetOwner(), this, DeltaTime);
		}
		else
		{
			OpenWindows.Add(Window);
			Window->Open(GetOwner(), this);
		}
	}

	if (FMath::IsNearlyEqual(Now, LastEventTime))
	{
		return;
	}
	// Playback jumped back to the start: the loop wrapped, fire the rest of the end and the start.
	const bool bWrapped = LastEventTime >= 0.f && Now < LastEventTime;
	TArray<const UOnyxClipEvent*> Due;
	CurrentClip->GetEventsBetween(LastEventTime, Now, bWrapped, Due);
	LastEventTime = Now;
	for (const UOnyxClipEvent* Event : Due)
	{
		Event->Fire(GetOwner(), this);
	}
}
