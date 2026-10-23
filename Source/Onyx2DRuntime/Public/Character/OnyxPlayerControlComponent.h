// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OnyxPlayerControlComponent.generated.h"

class AController;
class APawn;
class APlayerController;
class UEnhancedInputComponent;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnyxInteractSignature);

/**
 * Lets the player drive the character it is on: move, jump (hold for higher), dash, attack, drop through platforms,
 * crouch and interact, with the Onyx input scheme. Works on any pawn as soon as a player controller possesses it;
 * remove it (or disable it) and the same character can be driven by AI instead.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Player Control"))
class ONYX2DRUNTIME_API UOnyxPlayerControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxPlayerControlComponent();

	/** Turns the controls off (cutscenes, level exit) or back on. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Control")
	void SetControlEnabled(bool bEnabled) { bControlEnabled = bEnabled; if (!bEnabled) { ReleaseHeldInput(); } }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Control")
	bool IsControlEnabled() const { return bControlEnabled; }

	/** Does what the Interact input does: moves a conversation on, else uses what is nearby (talks, reads a sign). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Control")
	void Interact();

	/** The clip the character plays when it uses something in front (a door, a sign, a chest, a person); none: none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Control")
	FName InteractClip;

	/** The Interact input was pressed with nothing to use nearby. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Control")
	FOnyxInteractSignature OnInteract;

	/**
	 * The nearest thing the character touches that Interact would use now (a lever, a sign, a door, an NPC's Talk
	 * component), or null. It is an actor or a component that implements Onyx Interactable.
	 */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Control")
	UObject* FindInteractable() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	void BindTo(AController* Controller);
	void Unbind();
	bool CanAct() const;
	/** While a conversation is open, Jump and Interact turn its pages. True if one was open. */
	bool AdvanceDialogue() const;
	void ReleaseHeldInput();

	void HandleMove(const FInputActionValue& Value);
	void HandleMoveReleased(const FInputActionValue& Value);
	void HandleVertical(const FInputActionValue& Value);
	void HandleJumpPressed(const FInputActionValue& Value);
	void HandleJumpReleased(const FInputActionValue& Value);
	void HandleDash(const FInputActionValue& Value);
	void HandleAttack(const FInputActionValue& Value);
	void HandleDownPressed(const FInputActionValue& Value);
	void HandleDownReleased(const FInputActionValue& Value);
	void HandleInteract(const FInputActionValue& Value);
	void HandleUpPressed(const FInputActionValue& Value);
	void HandleUpReleased(const FInputActionValue& Value);
	void HandleMendPressed(const FInputActionValue& Value);
	void HandleMendReleased(const FInputActionValue& Value);
	void HandleShootPressed(const FInputActionValue& Value);
	void HandleShootReleased(const FInputActionValue& Value);
	void HandleThrowPressed(const FInputActionValue& Value);
	void HandleThrowReleased(const FInputActionValue& Value);
	void HandleAim(const FInputActionValue& Value);
	void HandleHotbarNext(const FInputActionValue& Value);
	void HandleHotbarPrevious(const FInputActionValue& Value);
	void HandleHotbarSlot(const FInputActionValue& Value);

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputComponent> InputComponent;

	TWeakObjectPtr<APlayerController> BoundController;
	float MoveInput = 0.f;
	bool bDownHeld = false;
	bool bControlEnabled = true;
};
