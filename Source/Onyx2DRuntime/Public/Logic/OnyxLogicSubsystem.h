// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Logic/OnyxAction.h"
#include "OnyxLogicSubsystem.generated.h"

/**
 * Runs the level's action lists (from Trigger Zones, Signs, the Talk behaviour) in order, waiting each action's Delay
 * and for actions that take a while, and slides actors for the Move action. Everything stops with the level.
 */
UCLASS()
class ONYX2DRUNTIME_API UOnyxLogicSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UOnyxLogicSubsystem* Get(const UObject* WorldContext);

	/** Runs Actions top to bottom for Context. Actions without a delay before them happen right away. */
	void Run(const TArray<TObjectPtr<UOnyxAction>>& Actions, const FOnyxActionContext& Context);

	/** Runs a list of actions (from Blueprint: an action list variable of your own). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Actions", meta = (DisplayName = "Run Actions"))
	void RunActions(const TArray<UOnyxAction*>& Actions, AActor* Instigator, AActor* Source);

	/** Slides Actor to Location over Time seconds, easing in and out. OnArrived runs at the end. */
	void MoveActor(AActor* Actor, const FVector& Location, float Time, TFunction<void()> OnArrived = nullptr);

	/** True while any action list is still running. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Actions")
	bool IsBusy() const { return Sequences.Num() > 0; }

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FSequence
	{
		TArray<TWeakObjectPtr<const UOnyxAction>> Actions;
		TWeakObjectPtr<AActor> Instigator;
		TWeakObjectPtr<AActor> Source;
		int32 Next = 0;
		/** Seconds spent waiting for the next action's Delay. */
		float Waited = 0.f;
		/** An action that takes a while is running. */
		bool bHeld = false;

		bool IsDone() const { return !bHeld && Next >= Actions.Num(); }
	};

	struct FMove
	{
		TWeakObjectPtr<AActor> Actor;
		FVector From = FVector::ZeroVector;
		FVector To = FVector::ZeroVector;
		float Time = 0.f;
		float Elapsed = 0.f;
		TFunction<void()> OnArrived;
	};

	/** Runs the sequence's actions until one has a delay left or holds it. */
	void Continue(const TSharedRef<FSequence>& Sequence);

	TArray<TSharedRef<FSequence>> Sequences;
	TArray<FMove> Moves;
};
