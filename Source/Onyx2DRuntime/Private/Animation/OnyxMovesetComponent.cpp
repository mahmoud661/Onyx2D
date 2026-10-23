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

namespace
{
	uint32 StateBit(EOnyxLocomotion State)
	{
		return 1u << uint32(State);
	}

	bool IsAirborne(EOnyxLocomotion State)
	{
		return State == EOnyxLocomotion::Jump || State == EOnyxLocomotion::Fall;
	}
}

void UOnyxMovesetComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Target && GetOwner())
	{
		Target = GetOwner()->FindComponentByClass<UPaperFlipbookComponent>();
	}
	if (UOnyxMovementComponent* Movement = GetOwner() ? GetOwner()->FindComponentByClass<UOnyxMovementComponent>() : nullptr)
	{
		Movement->OnAirJumped.AddUniqueDynamic(this, &UOnyxMovesetComponent::HandleAirJumped);
	}
}

void UOnyxMovesetComponent::HandleAirJumped()
{
	PlayTransition(TEXT("AirJump"), { EOnyxLocomotion::Jump, EOnyxLocomotion::Fall });
}

bool UOnyxMovesetComponent::PlayTransition(FName Role, std::initializer_list<EOnyxLocomotion> States)
{
	if (bOneShot || !bAutoLocomotion || !ApplyClip(Role, true))
	{
		return false;
	}
	Target->SetLooping(false);
	bTransition = true;
	Target->SetPlayRate(PlayRateOf(*CurrentClip));
	TransitionRemaining = GetClipLength(Role);
	TransitionStates = 0;
	for (const EOnyxLocomotion State : States)
	{
		TransitionStates |= StateBit(State);
	}
	return true;
}

void UOnyxMovesetComponent::StartTransition(EOnyxLocomotion From, EOnyxLocomotion To, const UOnyxMovementComponent& Movement)
{
	if (IsAirborne(From) && (To == EOnyxLocomotion::Idle || To == EOnyxLocomotion::Crouch) && Movement.GetLandingSpeed() >= LandClipMinSpeed)
	{
		PlayTransition(TEXT("Land"), { EOnyxLocomotion::Idle, EOnyxLocomotion::Crouch }); // until it moves
	}
	else if (IsAirborne(From) && To == EOnyxLocomotion::WallSlide)
	{
		PlayTransition(TEXT("WallGrab"), { EOnyxLocomotion::WallSlide });
	}
}

UOnyxClip* UOnyxMovesetComponent::GetCurrentClip() const
{
	return CurrentClip;
}

UOnyxClip* UOnyxMovesetComponent::FindPlayableClip(FName Role, bool* bOutVariant) const
{
	auto Find = [this](FName Name) -> UOnyxClip*
	{
		UOnyxClip* Clip = Moveset ? (Direction == EOnyxDirection::None ? Moveset->FindClip(Name) : Moveset->FindClip(Name, Direction, nullptr)) : nullptr;
		return Clip && Clip->Flipbook ? Clip : nullptr;
	};
	if (bOutVariant)
	{
		*bOutVariant = false;
	}
	if (!ClipVariant.IsNone() && !Role.IsNone())
	{
		if (UOnyxClip* Variant = Find(FName(*(Role.ToString() + ClipVariant.ToString()))))
		{
			if (bOutVariant)
			{
				*bOutVariant = true;
			}
			return Variant;
		}
	}
	return Find(Role);
}

float UOnyxMovesetComponent::PlayRateOf(const UOnyxClip& Clip) const
{
	return bBackwards && Clip.bLoop && !bOneShot && !bTransition ? -Clip.PlayRate : Clip.PlayRate;
}

void UOnyxMovesetComponent::SetPlayBackwards(bool bInBackwards)
{
	if (bInBackwards == bBackwards)
	{
		return;
	}
	bBackwards = bInBackwards;
	if (Target && CurrentClip)
	{
		Target->SetPlayRate(PlayRateOf(*CurrentClip));
	}
}

void UOnyxMovesetComponent::SetClipVariant(FName Suffix)
{
	if (Suffix == ClipVariant)
	{
		return;
	}
	ClipVariant = Suffix;
	if (bOneShot || CurrentRole.IsNone() || !Target)
	{
		return;
	}
	// The role playing now, drawn the other way, from the same place in its cycle (a transition stays a one-time play).
	bool bVariant = false;
	UOnyxClip* Clip = FindPlayableClip(CurrentRole, &bVariant);
	bPlayingVariant = Clip && bVariant;
	if (!Clip || Clip == CurrentClip)
	{
		return;
	}
	const float Position = Target->GetPlaybackPosition();
	CloseOpenWindows();
	Target->SetFlipbook(Clip->Flipbook);
	Target->SetPlayRate(PlayRateOf(*Clip));
	Target->SetLooping(Clip->bLoop && !bTransition);
	Target->SetPlaybackPosition(FMath::Min(Position, Target->GetFlipbookLength()), false);
	Target->Play();
	CurrentClip = Clip;
	LastEventTime = Target->GetPlaybackPosition();
	OnClipApplied.Broadcast();
}

bool UOnyxMovesetComponent::HasDiagonalClips() const
{
	return Moveset && Moveset->HasDiagonals();
}

void UOnyxMovesetComponent::SetDirection(EOnyxDirection InDirection)
{
	if (InDirection == Direction)
	{
		return;
	}
	Direction = InDirection;
	// A looping movement clip turns at once, at the same place in its cycle; a one-shot (a blow) keeps the way it began.
	if (bOneShot || bTransition || CurrentRole.IsNone() || !Target || !Moveset)
	{
		return;
	}
	EOnyxDirection Shown = EOnyxDirection::None;
	UOnyxClip* Clip = Moveset->FindClip(CurrentRole, Direction, &Shown);
	if (!Clip || !Clip->Flipbook || Clip == CurrentClip)
	{
		return;
	}
	const float Position = Target->GetPlaybackPosition();
	CloseOpenWindows();
	Target->SetFlipbook(Clip->Flipbook);
	Target->SetPlayRate(Clip->PlayRate);
	Target->SetLooping(Clip->bLoop);
	Target->SetPlaybackPosition(FMath::Min(Position, Target->GetFlipbookLength()), false);
	Target->Play();
	CurrentClip = Clip;
	ShownDirection = Shown;
	LastEventTime = Target->GetPlaybackPosition();
	OnClipApplied.Broadcast();
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
	bTransition = false;
	bOneShot = true;
	OneShotRemaining = GetClipLength(Role);
	Target->SetLooping(false);
	Target->SetPlayRate(PlayRateOf(*CurrentClip));
	return OneShotRemaining;
}

void UOnyxMovesetComponent::StopOneShot()
{
	if (!bOneShot)
	{
		return;
	}
	const FName Finished = CurrentRole;
	CloseOpenWindows(); // a cancelled attack's hitbox closes now, not when the next clip starts
	bOneShot = false;
	OneShotRemaining = 0.f;
	CurrentRole = NAME_None; // let locomotion pick the next clip
	OnOneShotFinished.Broadcast(Finished);
}

bool UOnyxMovesetComponent::ApplyClip(FName Role, bool bForceRestart)
{
	bool bVariant = false;
	UOnyxClip* Clip = FindPlayableClip(Role, &bVariant);
	if (!Target || !Clip)
	{
		return false;
	}
	if (Role == CurrentRole && !bForceRestart)
	{
		return true;
	}
	bPlayingVariant = bVariant;
	ShownDirection = EOnyxDirection::None;
	if (Direction != EOnyxDirection::None && Moveset)
	{
		Moveset->FindClip(Role, Direction, &ShownDirection);
	}

	CloseOpenWindows(); // the old clip's windows close before the new one starts
	Target->SetFlipbook(Clip->Flipbook);
	Target->SetLooping(Clip->bLoop);
	Target->PlayFromStart();
	Target->SetPlayRate(PlayRateOf(*Clip));
	CurrentRole = Role;
	CurrentClip = Clip;
	LastEventTime = -1.f; // events at time 0 fire too
	OnClipApplied.Broadcast();
	return true;
}

bool UOnyxMovesetComponent::SwimDirectionOf(FName Role, float& OutDegrees)
{
	static const FName Swim(TEXT("Swim"));
	if (Role == Swim)
	{
		OutDegrees = 0.f;
		return true;
	}
	const FString Name = Role.ToString();
	for (const TCHAR* Way : { TEXT("Swim_Up"), TEXT("Swim_Down") })
	{
		if (Name.StartsWith(Way) && Name.Len() > FCString::Strlen(Way) && FChar::IsDigit(Name[FCString::Strlen(Way)]))
		{
			const float Degrees = FCString::Atof(*Name.Mid(FCString::Strlen(Way)));
			OutDegrees = Way[5] == TEXT('U') ? Degrees : -Degrees;
			return true;
		}
	}
	return false;
}

FName UOnyxMovesetComponent::ResolveSwimDirection(const FVector& Velocity) const
{
	static const FName Swim(TEXT("Swim"));
	if (FVector2D(Velocity.X, Velocity.Z).Size() < 60.f || !HasClip(Swim))
	{
		return NAME_None;
	}
	// The way it is steered (the water's lift carries it up a little as it swims along: that is not climbing), else the way
	// it moves.
	const UOnyxMovementComponent* Movement = GetOwner() ? GetOwner()->FindComponentByClass<UOnyxMovementComponent>() : nullptr;
	const FVector2D Steering = Movement ? Movement->GetSwimSteering() : FVector2D::ZeroVector;
	const float Going = Steering.SizeSquared() > 0.04f ? FMath::RadiansToDegrees(FMath::Atan2(Steering.Y, FMath::Abs(Steering.X)))
		: FMath::RadiansToDegrees(FMath::Atan2(Velocity.Z, FMath::Abs(Velocity.X)));
	FName Best = Swim;
	float BestGap = FMath::Abs(Going);
	bool bAny = false;
	for (int32 Degrees = 15; Degrees <= 90; Degrees += 15)
	{
		for (const float Signed : { float(Degrees), float(-Degrees) })
		{
			const FName Drawn(*FString::Printf(TEXT("Swim_%s%d"), Signed > 0.f ? TEXT("Up") : TEXT("Down"), Degrees));
			if (HasClip(Drawn))
			{
				bAny = true;
				if (FMath::Abs(Going - Signed) < BestGap)
				{
					BestGap = FMath::Abs(Going - Signed);
					Best = Drawn;
				}
			}
		}
	}
	if (!bAny)
	{
		return NAME_None;
	}
	// Turning a little past halfway to the next drawing before it changes.
	float Shown = 0.f;
	if (Best != CurrentRole && SwimDirectionOf(CurrentRole, Shown) && FMath::Abs(Going - Shown) < BestGap + 10.f)
	{
		return CurrentRole;
	}
	return Best;
}

bool UOnyxMovesetComponent::SwapKeepingPlace(FName Role)
{
	bool bVariant = false;
	UOnyxClip* Clip = FindPlayableClip(Role, &bVariant);
	if (!Clip || !Target || bOneShot || bTransition)
	{
		return false;
	}
	const float Position = Target->GetPlaybackPosition();
	CloseOpenWindows();
	Target->SetFlipbook(Clip->Flipbook);
	Target->SetLooping(Clip->bLoop);
	Target->SetPlaybackPosition(FMath::Min(Position, Target->GetFlipbookLength()), false);
	Target->Play();
	Target->SetPlayRate(PlayRateOf(*Clip));
	CurrentRole = Role;
	CurrentClip = Clip;
	bPlayingVariant = bVariant;
	LastEventTime = Target->GetPlaybackPosition();
	OnClipApplied.Broadcast();
	return true;
}

FName UOnyxMovesetComponent::ResolveLocomotionClip_Implementation(EOnyxLocomotion State) const
{
	static const FName Idle(TEXT("Idle")), Run(TEXT("Run")), Jump(TEXT("Jump")), Fall(TEXT("Fall")), WallSlide(TEXT("WallSlide")), Dash(TEXT("Dash")), Crouch(TEXT("Crouch")), Swim(TEXT("Swim"));

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
	case EOnyxLocomotion::Swim:
	{
		// Hardly moving in the water: treading water (a "SwimIdle" clip), when there is one. The swim starts again a little
		// above the speed where it stopped, so drifting doesn't flick between the two.
		static const FName SwimIdle(TEXT("SwimIdle"));
		const AActor* Owner = GetOwner();
		const bool bFromAbove = Direction != EOnyxDirection::None;
		if (Owner && HasClip(SwimIdle))
		{
			const float Calm = CurrentRole == SwimIdle ? 90.f : 45.f; // cm/s across
			const FVector Velocity = Owner->GetVelocity();
			// From above both ways are across; from the side it drifts up and down more before it counts as swimming.
			const bool bCalm = bFromAbove ? FVector2D(Velocity.X, Velocity.Z).Size() < Calm : FMath::Abs(Velocity.X) < Calm && FMath::Abs(Velocity.Z) < Calm * 3.f;
			if (bCalm)
			{
				return SwimIdle;
			}
		}
		// From the side, swimming some way: the drawing nearest the way it goes (not while something is laid over the legs).
		if (!bFromAbove && Owner && ClipVariant.IsNone())
		{
			const FName Directed = ResolveSwimDirection(Owner->GetVelocity());
			if (!Directed.IsNone())
			{
				return Directed;
			}
		}
		// From above a swimmer with no swim clip runs on the spot rather than falling.
		return bFromAbove ? FirstAvailable({ Swim, Run, Idle }) : FirstAvailable({ Swim, Fall, Idle });
	}
	case EOnyxLocomotion::Walk:       return FirstAvailable({ TEXT("Walk"), Run, Idle });
	case EOnyxLocomotion::CrouchWalk: return FirstAvailable({ TEXT("CrouchWalk"), Crouch, Idle });
	case EOnyxLocomotion::Slide:      return FirstAvailable({ TEXT("Slide"), Crouch, Run, Idle });
	case EOnyxLocomotion::Roll:       return FirstAvailable({ TEXT("Roll"), Dash, Run, Idle });
	case EOnyxLocomotion::LedgeHang:  return FirstAvailable({ TEXT("LedgeHang"), WallSlide, Fall, Idle });
	case EOnyxLocomotion::LedgeClimb: return FirstAvailable({ TEXT("LedgeClimb"), Jump, Idle });
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
			const EOnyxLocomotion State = Movement->GetLocomotionState();
			if (bHasLastState && State != LastState)
			{
				StartTransition(LastState, State, *Movement);
			}
			LastState = State;
			bHasLastState = true;
			if (bTransition)
			{
				TransitionRemaining -= DeltaTime;
				if (TransitionRemaining > 0.f && (TransitionStates & StateBit(State)) != 0)
				{
					return; // the transition plays on
				}
				bTransition = false;
				CurrentRole = NAME_None; // the movement clip starts again
			}
			const FName Next = ResolveLocomotionClip(State);
			float Was = 0.f, Will = 0.f;
			// Turning while swimming: the stroke goes on at the same place, drawn the new way.
			if (Next != CurrentRole && SwimDirectionOf(CurrentRole, Was) && SwimDirectionOf(Next, Will) && SwapKeepingPlace(Next))
			{
				return;
			}
			ApplyClip(Next, false);
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
