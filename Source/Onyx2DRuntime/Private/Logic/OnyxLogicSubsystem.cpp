// Copyright Onyx2D. All Rights Reserved.

#include "Logic/OnyxLogicSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UOnyxLogicSubsystem* UOnyxLogicSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UOnyxLogicSubsystem>() : nullptr;
}

bool UOnyxLogicSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UOnyxLogicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOnyxLogicSubsystem, STATGROUP_Tickables);
}

void UOnyxLogicSubsystem::Run(const TArray<TObjectPtr<UOnyxAction>>& Actions, const FOnyxActionContext& Context)
{
	TSharedRef<FSequence> Sequence = MakeShared<FSequence>();
	for (const UOnyxAction* Action : Actions)
	{
		if (Action)
		{
			Sequence->Actions.Add(Action);
		}
	}
	Sequence->Instigator = Context.Instigator;
	Sequence->Source = Context.Source;
	Continue(Sequence);
	if (!Sequence->IsDone())
	{
		Sequences.Add(Sequence);
	}
}

void UOnyxLogicSubsystem::RunActions(const TArray<UOnyxAction*>& Actions, AActor* Instigator, AActor* Source)
{
	FOnyxActionContext Context;
	Context.Instigator = Instigator;
	Context.Source = Source;
	Run(TArray<TObjectPtr<UOnyxAction>>(Actions), Context);
}

void UOnyxLogicSubsystem::Continue(const TSharedRef<FSequence>& Sequence)
{
	while (!Sequence->bHeld && Sequence->Next < Sequence->Actions.Num())
	{
		const UOnyxAction* Action = Sequence->Actions[Sequence->Next].Get();
		if (Action && Sequence->Waited < Action->Delay)
		{
			return; // Tick counts the rest of the delay
		}
		++Sequence->Next;
		Sequence->Waited = 0.f;
		if (!Action)
		{
			continue; // its zone or sign is gone
		}

		FOnyxActionContext Context;
		Context.Instigator = Sequence->Instigator.Get();
		Context.Source = Sequence->Source.Get();
		Sequence->bHeld = true;
		TWeakPtr<FSequence> WeakSequence = Sequence;
		const bool bWaits = Action->RunAndWait(Context, [WeakSequence]()
		{
			if (const TSharedPtr<FSequence> Held = WeakSequence.Pin())
			{
				Held->bHeld = false; // Tick carries on with the next action
			}
		});
		if (!bWaits)
		{
			Sequence->bHeld = false;
			Action->Run(Context);
		}
	}
}

void UOnyxLogicSubsystem::MoveActor(AActor* Actor, const FVector& Location, float Time, TFunction<void()> OnArrived)
{
	if (!Actor)
	{
		if (OnArrived)
		{
			OnArrived();
		}
		return;
	}
	// A new move of the same actor replaces the old one (from where it is now).
	Moves.RemoveAll([Actor](const FMove& Move) { return Move.Actor.Get() == Actor; });
	if (Time <= 0.f)
	{
		Actor->SetActorLocation(Location);
		if (OnArrived)
		{
			OnArrived();
		}
		return;
	}
	FMove& Move = Moves.AddDefaulted_GetRef();
	Move.Actor = Actor;
	Move.From = Actor->GetActorLocation();
	Move.To = Location;
	Move.Time = Time;
	Move.OnArrived = MoveTemp(OnArrived);
}

void UOnyxLogicSubsystem::Tick(float DeltaTime)
{
	// Moves first, so an action waiting on one carries on this frame.
	TArray<TFunction<void()>> Arrived;
	for (int32 Index = Moves.Num() - 1; Index >= 0; --Index)
	{
		FMove& Move = Moves[Index];
		AActor* Actor = Move.Actor.Get();
		Move.Elapsed += DeltaTime;
		const float Alpha = FMath::Clamp(Move.Elapsed / Move.Time, 0.f, 1.f);
		if (Actor)
		{
			Actor->SetActorLocation(FMath::Lerp(Move.From, Move.To, FMath::SmoothStep(0.f, 1.f, Alpha)));
		}
		if (Alpha >= 1.f || !Actor)
		{
			if (Move.OnArrived)
			{
				Arrived.Add(MoveTemp(Move.OnArrived));
			}
			Moves.RemoveAtSwap(Index);
		}
	}
	for (const TFunction<void()>& Callback : Arrived)
	{
		Callback();
	}

	// A copy: actions may start new sequences.
	const TArray<TSharedRef<FSequence>> Running = Sequences;
	for (const TSharedRef<FSequence>& Sequence : Running)
	{
		if (!Sequence->bHeld)
		{
			Sequence->Waited += DeltaTime;
			Continue(Sequence);
		}
	}
	Sequences.RemoveAll([](const TSharedRef<FSequence>& Sequence) { return Sequence->IsDone(); });
}
