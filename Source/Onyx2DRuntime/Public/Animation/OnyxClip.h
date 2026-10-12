// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OnyxClip.generated.h"

class UOnyxClipEvent;
class UOnyxClipWindow;
class UOnyxMoveset;
class UPaperFlipbook;

/** One row of a clip's timeline. Lanes only organise events; every lane plays. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxClipLane
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lane")
	FName Name;

	/** Tint of the lane's row on the timeline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lane")
	FLinearColor Color = FLinearColor::White;
};

/**
 * One animation of a character: its frames (a Paper2D flipbook; each frame has its own duration), how it plays (speed,
 * loop) and its events: things that happen at exact times (Messages, sounds, effects) and Windows that stay open for a
 * while (hitbox, invulnerable). A Moveset gives each clip its role (Idle, Run, Attack1...). Edit it in the Moveset Editor.
 */
UCLASS(BlueprintType)
class ONYX2DRUNTIME_API UOnyxClip : public UObject
{
	GENERATED_BODY()

public:
	/** The frames. Frame durations and frames per second live in the flipbook. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clip")
	TObjectPtr<UPaperFlipbook> Flipbook;

	/** Playback speed multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clip", meta = (ClampMin = "0.01"))
	float PlayRate = 1.f;

	/** Loops (idle, run) or plays once and holds the last frame (attack, hurt, death). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clip")
	bool bLoop = true;

	/** The Moveset this clip belongs to: double-clicking the clip opens that Moveset's editor, and its Messages fill the Message list. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clip", AssetRegistrySearchable)
	TSoftObjectPtr<UOnyxMoveset> Moveset;

	/** Events and Windows, sorted by time. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Events")
	TArray<TObjectPtr<UOnyxClipEvent>> Events;

	/** The timeline's event lanes (at least one). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	TArray<FOnyxClipLane> Lanes;

	static constexpr int32 MaxLanes = 8;

	UOnyxClip();

	/** Length in seconds at speed 1. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Clip")
	float GetLength() const;

	/** Number of frame units (a frame shown for 3 units counts 3). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Clip")
	int32 GetFrameUnits() const;

	/** Frames per second of the flipbook (10 without one). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Clip")
	float GetFramesPerSecond() const;

	/** Keeps events in time order (edits call it). */
	void SortEvents();

	/** Every instant event with Time in (From, To], wrapping around the end when looping. */
	void GetEventsBetween(float From, float To, bool bWrapped, TArray<const UOnyxClipEvent*>& OutEvents) const;

	/** Every window open at Time. */
	void GetWindowsAt(float Time, TArray<const UOnyxClipWindow*>& OutWindows) const;

	/** True if the clip has a window of this kind (e.g. a Hitbox window). */
	bool HasWindow(TSubclassOf<UOnyxClipWindow> WindowClass) const;

	/** Name for a new lane: "Lane 1", "Lane 2"... (the lowest free number). */
	FName MakeLaneName() const;

	/** The colour a new lane gets: purple, amber, teal, coral, blue, green, pink, grey, then around again. */
	static FLinearColor GetLanePaletteColor(int32 Index);

#if WITH_EDITOR
	/** Inserts a lane at Index (events on lanes >= Index move down one). Returns the index, or INDEX_NONE when full. */
	int32 InsertLane(int32 Index);

	/** Removes a lane and its events (the last lane can't be removed). */
	bool RemoveLane(int32 Index);

	/** Creates an event of Class at Time on Lane. Call inside a transaction. */
	UOnyxClipEvent* AddEvent(TSubclassOf<UOnyxClipEvent> Class, float Time, int32 Lane);

	void RemoveEvent(UOnyxClipEvent* Event);
#endif

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** At least one lane, and every event on a valid lane. */
	void FixLanes();
};
