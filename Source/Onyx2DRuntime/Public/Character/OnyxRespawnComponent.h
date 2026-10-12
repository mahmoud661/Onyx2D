// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OnyxRespawnComponent.generated.h"

/**
 * Brings the character back after it dies: at its start point, or the last checkpoint it touched, healed and briefly
 * invulnerable. Characters with it keep their body when they die.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Respawn"))
class ONYX2DRUNTIME_API UOnyxRespawnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxRespawnComponent();

	/** Seconds after death before coming back (0 = only when Respawn is called). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Respawn", meta = (ClampMin = "0", Units = "s"))
	float RespawnDelay = 1.2f;

	/** Seconds of invulnerability after coming back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Respawn", meta = (ClampMin = "0", Units = "s"))
	float InvulnerableTime = 1.f;

	/** Where the character comes back. Starts at its spawn point; checkpoints move it. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Respawn")
	void SetRespawnLocation(const FVector& Location) { RespawnLocation = Location; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Respawn")
	FVector GetRespawnLocation() const { return RespawnLocation; }

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Respawn")
	void Respawn();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleDeath(AActor* Killer);

	FVector RespawnLocation = FVector::ZeroVector;
	FTimerHandle RespawnTimer;
};
