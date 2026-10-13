// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Logic/OnyxDialogueSubsystem.h"
#include "OnyxAction.generated.h"

class AOnyxTriggerZone;
class APawn;
class UPaperFlipbook;
class USoundBase;

/** Who and what a list of actions runs for. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxActionContext
{
	GENERATED_BODY()

	/** Who set it off: the player who walked in or pressed Interact. Empty when nobody did (the level started). */
	UPROPERTY(BlueprintReadWrite, Category = "Action")
	TObjectPtr<AActor> Instigator = nullptr;

	/** What runs the actions: the trigger zone, sign or character. */
	UPROPERTY(BlueprintReadWrite, Category = "Action")
	TObjectPtr<AActor> Source = nullptr;

	/** The player the actions are about: the instigator when it is a player, else the first player. */
	APawn* GetPlayer() const;

	UWorld* GetWorld() const;
};

/** Where an action happens. */
UENUM(BlueprintType)
enum class EOnyxActionSpot : uint8
{
	/** At the zone, sign or character running the actions. */
	Here,
	/** At the player. */
	AtPlayer UMETA(DisplayName = "At the Player"),
};

/** On, off, or the other way round. */
UENUM(BlueprintType)
enum class EOnyxSwitchTo : uint8
{
	On,
	Off,
	Toggle UMETA(DisplayName = "Flip"),
};

/**
 * One thing that happens: show a message, open a door, give a key, play a sound... Trigger Zones, Signs and the Talk
 * behaviour run a list of them, top to bottom, with no Blueprint needed. Each one can wait a Delay first, and some (a
 * Dialogue, a Move that waits) hold the actions below them until they are done.
 *
 * Make your own kind by subclassing in Blueprint (implement On Run) or C++ (override Run): it appears in every action list.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class ONYX2DRUNTIME_API UOnyxAction : public UObject
{
	GENERATED_BODY()

public:
	/** Seconds to wait, after the action above it, before this one happens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0", Units = "s", DisplayPriority = "100"))
	float Delay = 0.f;

	/** Does the action. The base calls On Run (Blueprint actions). */
	virtual void Run(const FOnyxActionContext& Context) const;

	/**
	 * Actions that take a while override this: start, call OnFinished when done, and return true so the actions below
	 * wait. Return false (the default) to be run with Run instead.
	 */
	virtual bool RunAndWait(const FOnyxActionContext& Context, TFunction<void()> OnFinished) const { return false; }

	/** One line saying what it does ("Show message: The door is locked"), for logs and the Level Check. */
	virtual FString Describe() const;

	/** What is missing for it to work ("Teleport has no destination"), or empty when it is ready. */
	virtual FText CheckSetup() const { return FText::GetEmpty(); }

	virtual UWorld* GetWorld() const override;

	/** Runs a list of actions in order, waiting each one's Delay (and for actions that wait). */
	static void RunAll(const TArray<TObjectPtr<UOnyxAction>>& Actions, const FOnyxActionContext& Context);

	/** "Show Message, Open Level" style summary of a list, for tooltips and the canvas. */
	static FString DescribeAll(const TArray<TObjectPtr<UOnyxAction>>& Actions);

protected:
	/** Blueprint actions: what happens. Instigator is who set it off (may be empty); Source runs the actions. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Action", meta = (DisplayName = "On Run"))
	void OnRun(AActor* Instigator, AActor* Source) const;
};

/** Shows a short line across the screen that fades by itself: "The door is locked", "Checkpoint!". */
UCLASS(meta = (DisplayName = "Show Message"))
class ONYX2DRUNTIME_API UOnyxAction_ShowMessage : public UOnyxAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (MultiLine = true))
	FText Message = INVTEXT("Hello!");

	/** Seconds it stays on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0.5", Units = "s"))
	float Duration = 3.f;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
	virtual FText CheckSetup() const override;
};

/**
 * A conversation in a box at the bottom of the screen. Each line types out; the player presses Jump or Interact to go
 * on and can't move until it is over. The actions below it wait until the player has read the last line.
 */
UCLASS(meta = (DisplayName = "Dialogue"))
class ONYX2DRUNTIME_API UOnyxAction_Dialogue : public UOnyxAction
{
	GENERATED_BODY()

public:
	UOnyxAction_Dialogue();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TArray<FOnyxDialogueLine> Lines;

	/** How fast the lines type out, in letters per second (0 = each line appears at once). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0"), AdvancedDisplay)
	float LettersPerSecond = 45.f;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual bool RunAndWait(const FOnyxActionContext& Context, TFunction<void()> OnFinished) const override;
	virtual FString Describe() const override;
	virtual FText CheckSetup() const override;
};

/** Plays a sound here, at the player, or for the whole level. */
UCLASS(meta = (DisplayName = "Play Sound"))
class ONYX2DRUNTIME_API UOnyxAction_PlaySound : public UOnyxAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (EditCondition = "!bWholeLevel"))
	EOnyxActionSpot Where = EOnyxActionSpot::Here;

	/** Heard the same everywhere (a jingle, a voice) instead of coming from a place. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	bool bWholeLevel = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0"))
	float Volume = 1.f;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
	virtual FText CheckSetup() const override;
};

/** Plays a flipbook once (a puff of smoke, sparkles, an explosion). */
UCLASS(meta = (DisplayName = "Spawn Effect"))
class ONYX2DRUNTIME_API UOnyxAction_SpawnEffect : public UOnyxAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TObjectPtr<UPaperFlipbook> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	EOnyxActionSpot Where = EOnyxActionSpot::Here;

	/** Offset in cm: X right, Y up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	FVector2D Offset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0.01"))
	float Scale = 1.f;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
	virtual FText CheckSetup() const override;
};

/** Moves the player to another spot in the level (a door to another room, a pipe, a portal). */
UCLASS(meta = (DisplayName = "Teleport Player"))
class ONYX2DRUNTIME_API UOnyxAction_Teleport : public UOnyxAction
{
	GENERATED_BODY()

public:
	/** Where the player goes: any actor in the level (an empty Target Point, another zone, a door). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TObjectPtr<AActor> Destination;

	/** Keep moving at the same speed after arriving, instead of arriving standing still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	bool bKeepSpeed = false;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
	virtual FText CheckSetup() const override;
};

/** Makes actors appear or disappear, collision too: open a door by hiding it, reveal a secret bridge, remove a wall. */
UCLASS(meta = (DisplayName = "Show or Hide"))
class ONYX2DRUNTIME_API UOnyxAction_ShowHide : public UOnyxAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TArray<TObjectPtr<AActor>> Actors;

	/** On shows them, Off hides them, Flip swaps each one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	EOnyxSwitchTo Visible = EOnyxSwitchTo::Off;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
	virtual FText CheckSetup() const override;
};

/** Slides actors smoothly by an offset: a door rising, a bridge lowering, a wall sliding away. */
UCLASS(meta = (DisplayName = "Move"))
class ONYX2DRUNTIME_API UOnyxAction_Move : public UOnyxAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TArray<TObjectPtr<AActor>> Actors;

	/** How far they move, in cm: X right, Y up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	FVector2D Offset = FVector2D(0.f, 200.f);

	/** Seconds the move takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0", Units = "s"))
	float Time = 1.f;

	/** The actions below wait until the move is over. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	bool bWaitUntilDone = false;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual bool RunAndWait(const FOnyxActionContext& Context, TFunction<void()> OnFinished) const override;
	virtual FString Describe() const override;
	virtual FText CheckSetup() const override;
};

/** What Give hands out. */
UENUM(BlueprintType)
enum class EOnyxGiveWhat : uint8
{
	Coins,
	/** A named item: a key, a gem, a quest object. */
	Item,
	/** Health back to the player. */
	Health,
};

/** Gives the player coins, an item (a key) or health. */
UCLASS(meta = (DisplayName = "Give"))
class ONYX2DRUNTIME_API UOnyxAction_Give : public UOnyxAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	EOnyxGiveWhat What = EOnyxGiveWhat::Item;

	/** The item's name. Zones that need it use the same name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (EditCondition = "What == EOnyxGiveWhat::Item", EditConditionHides))
	FName Item = TEXT("Key");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "1"))
	int32 Amount = 1;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
	virtual FText CheckSetup() const override;
};

/** Hurts or defeats the player (spikes, a trap, falling out of the level). */
UCLASS(meta = (DisplayName = "Hurt Player"))
class ONYX2DRUNTIME_API UOnyxAction_Hurt : public UOnyxAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0", EditCondition = "!bDefeat"))
	float Damage = 1.f;

	/** Defeats the player at once, whatever their health. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	bool bDefeat = false;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
};

/** Fades out and opens another level (or restarts this one). */
UCLASS(meta = (DisplayName = "Open Level"))
class ONYX2DRUNTIME_API UOnyxAction_OpenLevel : public UOnyxAction
{
	GENERATED_BODY()

public:
	/** The level to open (empty = restart this level). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TSoftObjectPtr<UWorld> Level;

	/** Seconds the screen takes to fade to black first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0", Units = "s"))
	float FadeTime = 0.8f;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
};

/** Shakes the camera (an earthquake, a heavy door slamming). */
UCLASS(meta = (DisplayName = "Shake Camera"))
class ONYX2DRUNTIME_API UOnyxAction_ShakeCamera : public UOnyxAction
{
	GENERATED_BODY()

public:
	/** How strong, 0..1 (shakes add up and fade). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0", ClampMax = "1"))
	float Strength = 0.5f;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
};

/** Turns 2D lights (torches, lamps) or Light Switches on or off, with a fade. */
UCLASS(meta = (DisplayName = "Switch Lights"))
class ONYX2DRUNTIME_API UOnyxAction_SwitchLights : public UOnyxAction
{
	GENERATED_BODY()

public:
	/** Onyx 2D Lights, Light Switches, or any actor with 2D light components. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TArray<TObjectPtr<AActor>> Lights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	EOnyxSwitchTo SwitchTo = EOnyxSwitchTo::Toggle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0", Units = "s"))
	float FadeTime = 0.4f;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
	virtual FText CheckSetup() const override;
};

/** Throws the player into the air: a spring, a jump pad, a geyser. */
UCLASS(meta = (DisplayName = "Launch Player"))
class ONYX2DRUNTIME_API UOnyxAction_Launch : public UOnyxAction
{
	GENERATED_BODY()

public:
	/** Speed in cm/s: X right, Y up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	FVector2D Velocity = FVector2D(0.f, 1400.f);

	/** Replace the player's speed sideways instead of adding to it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	bool bReplaceSideways = false;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
};

/** Makes this spot (or the player's) where the player comes back after being defeated. */
UCLASS(meta = (DisplayName = "Set Checkpoint"))
class ONYX2DRUNTIME_API UOnyxAction_SetCheckpoint : public UOnyxAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	EOnyxActionSpot Where = EOnyxActionSpot::Here;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
};

/** Changes the level's music (boss fight, a calm area), fading between them. */
UCLASS(meta = (DisplayName = "Play Music"))
class ONYX2DRUNTIME_API UOnyxAction_PlayMusic : public UOnyxAction
{
	GENERATED_BODY()

public:
	/** The new music (empty = fade the music out). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TObjectPtr<USoundBase> Music;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0", Units = "s"))
	float FadeTime = 1.5f;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
};

/** Runs a function or custom event, with no inputs, on actors: the way into your own Blueprints. */
UCLASS(meta = (DisplayName = "Call Event"))
class ONYX2DRUNTIME_API UOnyxAction_CallEvent : public UOnyxAction
{
	GENERATED_BODY()

public:
	/** The function or custom event's name, exactly as in the Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	FName Event = TEXT("OnTriggered");

	/** The actors to call it on (empty = the zone, sign or character itself). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TArray<TObjectPtr<AActor>> Actors;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
	virtual FText CheckSetup() const override;
};

/** What Control Zones does to the zones. */
UENUM(BlueprintType)
enum class EOnyxZoneCommand : uint8
{
	/** Run their actions now, as if the player set them off. */
	Fire,
	/** Let them work again. */
	TurnOn UMETA(DisplayName = "Turn On"),
	/** Make them do nothing until turned on. */
	TurnOff UMETA(DisplayName = "Turn Off"),
};

/** Fires, turns on or turns off other trigger zones: chain zones together, or unlock one after another. */
UCLASS(meta = (DisplayName = "Control Zones"))
class ONYX2DRUNTIME_API UOnyxAction_ControlZones : public UOnyxAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TArray<TObjectPtr<AOnyxTriggerZone>> Zones;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	EOnyxZoneCommand Command = EOnyxZoneCommand::Fire;

	virtual void Run(const FOnyxActionContext& Context) const override;
	virtual FString Describe() const override;
	virtual FText CheckSetup() const override;
};
