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

	/** The Interact input was pressed with nothing to use nearby. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Control")
	FOnyxInteractSignature OnInteract;

	/** The nearest interactable (lever, door, NPC...) the character touches that can be used now, or null. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Control")
	AActor* FindInteractable() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	void BindTo(AController* Controller);
	void Unbind();
	bool CanAct() const;
	void ReleaseHeldInput();

	void HandleMove(const FInputActionValue& Value);
	void HandleMoveReleased(const FInputActionValue& Value);
	void HandleJumpPressed(const FInputActionValue& Value);
	void HandleJumpReleased(const FInputActionValue& Value);
	void HandleDash(const FInputActionValue& Value);
	void HandleAttack(const FInputActionValue& Value);
	void HandleDownPressed(const FInputActionValue& Value);
	void HandleDownReleased(const FInputActionValue& Value);
	void HandleInteract(const FInputActionValue& Value);

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputComponent> InputComponent;

	TWeakObjectPtr<APlayerController> BoundController;
	float MoveInput = 0.f;
	bool bDownHeld = false;
	bool bControlEnabled = true;
};
