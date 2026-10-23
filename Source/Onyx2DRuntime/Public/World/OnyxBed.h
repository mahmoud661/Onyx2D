// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/OnyxInteractable.h"
#include "OnyxBed.generated.h"

class UBoxComponent;
class UPaperSprite;
class UPaperSpriteComponent;

/**
 * Where the day ends: Interact (prompt "Sleep") fades the screen out, starts the next day on the Onyx Calendar (crops
 * grow, the clock goes back to the morning), saves the game (Onyx Save) and fades back in. Put it on a bed, or on the
 * farmhouse door. When the player passes out at the end of the day, they wake here too.
 */
UCLASS(meta = (DisplayName = "Onyx Bed"))
class ONYX2DRUNTIME_API AOnyxBed : public AActor, public IOnyxInteractable
{
	GENERATED_BODY()

public:
	AOnyxBed();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperSpriteComponent> Sprite;

	/** How it looks (none: only its trigger, for a door). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Bed")
	TObjectPtr<UPaperSprite> Look;

	/** Seconds the screen takes to go dark, stays dark, and comes back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Bed", meta = (ClampMin = "0.05", Units = "s"))
	float FadeTime = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Bed", meta = (ClampMin = "0", Units = "s"))
	float DarkTime = 1.2f;

	/** Saves the game when the night passes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Bed")
	bool bSaveOnSleep = true;

	/** Sleeps now (the same as the player using it). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Bed")
	void Sleep(AActor* Sleeper);

	// IOnyxInteractable
	virtual void Interact_Implementation(AActor* User) override;
	virtual bool IsInteractionLocked_Implementation(AActor* User) const override { return bSleeping; }
	virtual FText GetInteractionPrompt_Implementation() const override { return INVTEXT("Sleep"); }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandlePassedOut();

	void Wake();

	TWeakObjectPtr<AActor> SleepingActor;
	FTimerHandle WakeTimer;
	bool bSleeping = false;
};
