// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/OnyxInteractable.h"
#include "OnyxLightSwitch.generated.h"

class UBoxComponent;
class UPaperSprite;
class UPaperSpriteComponent;
class USoundBase;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxSwitchSignature, bool, bOn);

/**
 * A lever / switch / pressure plate that turns lights on and off (any actor with an Onyx Light 2D: torches, lamps,
 * crystals) with a fade. Used with the Interact input, or by just walking over it. Shows a sprite per state (or a
 * coloured stand-in until it has art).
 */
UCLASS(meta = (DisplayName = "Onyx Light Switch"))
class ONYX2DRUNTIME_API AOnyxLightSwitch : public AActor, public IOnyxInteractable
{
	GENERATED_BODY()

public:
	AOnyxLightSwitch();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperSpriteComponent> Sprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UStaticMeshComponent> Placeholder;

	/** The lights it controls (Onyx Light 2D actors, or anything with Light 2D components). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Switch")
	TArray<TObjectPtr<AActor>> Lights;

	/** The lights' state when the level starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Switch")
	bool bStartOn = true;

	/** Walking over it switches it (a pressure plate) instead of needing the Interact input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Switch")
	bool bSwitchOnTouch = false;

	/** Seconds the lights take to fade on / off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Switch", meta = (ClampMin = "0", Units = "s"))
	float FadeTime = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperSprite> OnSprite;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperSprite> OffSprite;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<USoundBase> SwitchSound;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Switch")
	FOnyxSwitchSignature OnSwitched;

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Switch")
	void SetOn(bool bOn, bool bInstant = false);

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Switch")
	bool IsOn() const { return bIsOn; }

	// IOnyxInteractable
	virtual void Interact_Implementation(AActor* User) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ApplyLook();

	bool bIsOn = true;
};
