// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/OnyxClipEvent.h"
#include "OnyxClipWindow.generated.h"

/**
 * A span of a Clip, from Time to Time + Length: it Opens when playback enters, Updates every frame inside and Closes
 * when playback leaves (or the clip changes). Drawn as a bar on the timeline. Make your own in C++ (override Open /
 * Update / Close) or Blueprint (On Open / On Update / On Close).
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Window"))
class ONYX2DRUNTIME_API UOnyxClipWindow : public UOnyxClipEvent
{
	GENERATED_BODY()

public:
	UOnyxClipWindow();

	/** How long it stays open (at speed 1). Stored in seconds, shown in milliseconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timeline", meta = (ClampMin = "0.01", Units = "s", ForceUnits = "ms"))
	float Length = 0.25f;

	static constexpr float ShortestLength = 0.01f;

	virtual bool IsWindow() const override { return true; }
	virtual void Fire(AActor* Owner, UOnyxMovesetComponent* Moveset) const override {}

	virtual void Open(AActor* Owner, UOnyxMovesetComponent* Moveset) const;
	virtual void Update(AActor* Owner, UOnyxMovesetComponent* Moveset, float DeltaTime) const;
	virtual void Close(AActor* Owner, UOnyxMovesetComponent* Moveset) const;

	float GetEndTime() const { return Time + Length; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Window", meta = (DisplayName = "On Open"))
	void OnOpen(AActor* Owner, UOnyxMovesetComponent* Moveset) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Window", meta = (DisplayName = "On Update"))
	void OnUpdate(AActor* Owner, UOnyxMovesetComponent* Moveset, float DeltaTime) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Window", meta = (DisplayName = "On Close"))
	void OnClose(AActor* Owner, UOnyxMovesetComponent* Moveset) const;
};

/** The character's attack hitbox hurts only while this window is open (the frames where the sword actually swings). */
UCLASS(meta = (DisplayName = "Hitbox"))
class ONYX2DRUNTIME_API UOnyxClipWindow_Hitbox : public UOnyxClipWindow
{
	GENERATED_BODY()

public:
	UOnyxClipWindow_Hitbox();

	/** Name of the Onyx Hitbox component to open (empty = the character's attack hitbox). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window")
	FName Hitbox;

	virtual void Open(AActor* Owner, UOnyxMovesetComponent* Moveset) const override;
	virtual void Close(AActor* Owner, UOnyxMovesetComponent* Moveset) const override;
	virtual FText GetLabel() const override { return INVTEXT("Hitbox"); }
};

/** The character can't be hurt while this window is open (dodge rolls, dashes, a super move). */
UCLASS(meta = (DisplayName = "Invulnerable"))
class ONYX2DRUNTIME_API UOnyxClipWindow_Invulnerable : public UOnyxClipWindow
{
	GENERATED_BODY()

public:
	UOnyxClipWindow_Invulnerable();

	virtual void Open(AActor* Owner, UOnyxMovesetComponent* Moveset) const override;
	virtual FText GetLabel() const override { return INVTEXT("Invulnerable"); }
};

/** A sound that plays while the window is open and fades after (a charge-up hum, a spin attack whoosh). */
UCLASS(meta = (DisplayName = "Loop Sound"))
class ONYX2DRUNTIME_API UOnyxClipWindow_LoopSound : public UOnyxClipWindow
{
	GENERATED_BODY()

public:
	UOnyxClipWindow_LoopSound();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window", meta = (ClampMin = "0"))
	float Volume = 1.f;

	/** Seconds of fade out when the window closes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window", meta = (ClampMin = "0", Units = "s"))
	float FadeOut = 0.1f;

	virtual void Open(AActor* Owner, UOnyxMovesetComponent* Moveset) const override;
	virtual void Close(AActor* Owner, UOnyxMovesetComponent* Moveset) const override;
	virtual FText GetLabel() const override;
};
