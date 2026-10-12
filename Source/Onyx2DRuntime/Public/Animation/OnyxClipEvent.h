// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OnyxClipEvent.generated.h"

class UOnyxClip;
class UOnyxMovesetComponent;
class UPaperFlipbook;
class USoundBase;

/**
 * Something that happens at an exact moment of a Clip: send a Message, play a sound, spawn an effect, shake the camera...
 * Events live inside the clip, sit on one of its timeline lanes and fire when playback crosses their Time. Make your own
 * kind by subclassing in C++ (override Fire) or Blueprint (implement On Fired): it shows up in the timeline's Add Event menu.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class ONYX2DRUNTIME_API UOnyxClipEvent : public UObject
{
	GENERATED_BODY()

public:
	/** When it fires, from the start of the clip (at speed 1). Stored in seconds, shown in milliseconds: place it on any millisecond. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timeline", meta = (ClampMin = "0", Units = "s", ForceUnits = "ms"))
	float Time = 0.f;

	/** The timeline lane it sits on (lanes only organise; every lane plays). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timeline", meta = (ClampMin = "0"))
	int32 Lane = 0;

	/** Colour of its marker on the timeline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timeline")
	FLinearColor TimelineColor = FLinearColor(0.75f, 0.75f, 0.8f);

	/** True for Windows (they last a while: Open, Update, Close) rather than firing once. */
	virtual bool IsWindow() const { return false; }

	/** Fires the event for an actor. Calls On Fired for Blueprint events. */
	virtual void Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const;

	/** Label on the timeline. */
	virtual FText GetLabel() const;

	/** Whether the timeline's Add Event / Add Window menu lists this kind for Clip. */
	virtual bool ShowInAddMenu(const UOnyxClip* Clip) const { return true; }

	/** The clip this event belongs to. */
	UOnyxClip* GetClip() const;

protected:
	/** Blueprint events: what happens when playback reaches this event. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Event", meta = (DisplayName = "On Fired"))
	void OnFired(AActor* Owner, UOnyxMovesetComponent* Moveset) const;
};

/**
 * Sends one of the Moveset's Messages: the actor's function or custom event of that name runs (no parameters) and its
 * Moveset component broadcasts On Clip Message. Use it for gameplay at an exact frame: spawn a projectile, step dust...
 * Messages are named once on the Moveset (Messages tab) and placed from the timeline's Add Event > Messages list.
 */
UCLASS(meta = (DisplayName = "Message"))
class ONYX2DRUNTIME_API UOnyxClipEvent_Message : public UOnyxClipEvent
{
	GENERATED_BODY()

public:
	UOnyxClipEvent_Message();

	/** The message sent (one of the Moveset's Messages). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (GetOptions = "GetMessageOptions"))
	FName Message = TEXT("Message");

	virtual void Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const override;
	virtual FText GetLabel() const override { return FText::FromName(Message); }
	/** Messages are added from the menu's Messages section, by name. */
	virtual bool ShowInAddMenu(const UOnyxClip* Clip) const override { return false; }

	/** The Moveset's Messages, for the Message drop-down. */
	UFUNCTION()
	TArray<FString> GetMessageOptions() const;
};

/** Plays a sound at the actor (footsteps, swings, voice). */
UCLASS(meta = (DisplayName = "Sound"))
class ONYX2DRUNTIME_API UOnyxClipEvent_Sound : public UOnyxClipEvent
{
	GENERATED_BODY()

public:
	UOnyxClipEvent_Sound();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "0"))
	float Volume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "0.01"))
	float Pitch = 1.f;

	/** Random pitch change each time (+/-), so repeated sounds don't feel mechanical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "0", ClampMax = "0.5"))
	float PitchVariation = 0.05f;

	/** Follow the character while it plays (long sounds: a scream, a charge-up) instead of staying where it started. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	bool bFollowCharacter = false;

	virtual void Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const override;
	virtual FText GetLabel() const override;
};

/** Plays a flipbook once next to the actor (dust puff, slash trail, sparks). Offset follows the facing. */
UCLASS(meta = (DisplayName = "Effect"))
class ONYX2DRUNTIME_API UOnyxClipEvent_Effect : public UOnyxClipEvent
{
	GENERATED_BODY()

public:
	UOnyxClipEvent_Effect();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	TObjectPtr<UPaperFlipbook> Effect;

	/** Offset from the actor in cm: X forward (mirrored when facing left), Y up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FVector2D Offset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "0.01"))
	float Scale = 1.f;

	virtual void Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const override;
	virtual FText GetLabel() const override;
};

/** Shakes the camera (landing, heavy hit, explosion). */
UCLASS(meta = (DisplayName = "Camera Shake"))
class ONYX2DRUNTIME_API UOnyxClipEvent_CameraShake : public UOnyxClipEvent
{
	GENERATED_BODY()

public:
	UOnyxClipEvent_CameraShake();

	/** Shake strength added (0..1; trauma adds up and fades). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "0", ClampMax = "1"))
	float Strength = 0.3f;

	virtual void Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const override;
	virtual FText GetLabel() const override { return INVTEXT("Shake"); }
};

/** A burst of 2D light next to the character (muzzle flash, sword spark, magic cast). Offset follows the facing. */
UCLASS(meta = (DisplayName = "Light Flash"))
class ONYX2DRUNTIME_API UOnyxClipEvent_LightFlash : public UOnyxClipEvent
{
	GENERATED_BODY()

public:
	UOnyxClipEvent_LightFlash();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FLinearColor Color = FLinearColor(1.f, 0.8f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "0", UIMax = "8"))
	float Intensity = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "10", UIMax = "2000"))
	float Radius = 350.f;

	/** Seconds to fade out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "0.02", Units = "s"))
	float Duration = 0.2f;

	/** Offset from the actor in cm: X forward (mirrored when facing left), Y up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FVector2D Offset = FVector2D(60.f, 0.f);

	virtual void Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const override;
	virtual FText GetLabel() const override { return INVTEXT("Light"); }
};

/** Lights up the whole scene for a moment (lightning strike, explosion, a heavy slam). */
UCLASS(meta = (DisplayName = "Scene Flash"))
class ONYX2DRUNTIME_API UOnyxClipEvent_SceneFlash : public UOnyxClipEvent
{
	GENERATED_BODY()

public:
	UOnyxClipEvent_SceneFlash();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FLinearColor Color = FLinearColor(0.85f, 0.9f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "0", UIMax = "4"))
	float Intensity = 1.5f;

	/** Seconds to fade back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "0.02", Units = "s"))
	float Duration = 0.35f;

	virtual void Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const override;
	virtual FText GetLabel() const override { return INVTEXT("Flash"); }
};
