// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/OnyxCharacter.h"
#include "OnyxPlayerCharacter.generated.h"

class UOnyxCameraComponent;
struct FInputActionValue;

/** Player-controlled Onyx character: input, follow camera and respawning. */
UCLASS()
class ONYX2DRUNTIME_API AOnyxPlayerCharacter : public AOnyxCharacter
{
	GENERATED_BODY()

public:
	AOnyxPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxCameraComponent> Camera;

	/** Seconds after death before respawning (0 = no automatic respawn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Respawn", meta = (ClampMin = "0"))
	float RespawnDelay = 1.2f;

	/** Where the player respawns. Initialised to the spawn point; checkpoints update it. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Respawn")
	void SetRespawnLocation(const FVector& Location) { RespawnLocation = Location; }

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Respawn")
	void Respawn();

	/** Fired when the Interact input is pressed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Onyx2D")
	void OnInteract();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void HandleDeath(AActor* Killer) override;

private:
	void HandleMove(const FInputActionValue& Value);
	void HandleMoveReleased(const FInputActionValue& Value);
	void HandleJumpPressed(const FInputActionValue& Value);
	void HandleJumpReleased(const FInputActionValue& Value);
	void HandleDash(const FInputActionValue& Value);
	void HandleAttack(const FInputActionValue& Value);
	void HandleDownPressed(const FInputActionValue& Value);
	void HandleDownReleased(const FInputActionValue& Value);
	void HandleInteract(const FInputActionValue& Value);

	FVector RespawnLocation = FVector::ZeroVector;
	FTimerHandle RespawnTimer;
	float MoveInput = 0.f;
	bool bDownHeld = false;
};
