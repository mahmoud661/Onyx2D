// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxLevelExit.generated.h"

class UBoxComponent;
class USoundBase;
class UStaticMeshComponent;

/** Door / goal: the player touching it fades out and opens the next level. */
UCLASS(meta = (DisplayName = "Onyx Level Exit"))
class ONYX2DRUNTIME_API AOnyxLevelExit : public AActor
{
	GENERATED_BODY()

public:
	AOnyxLevelExit();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Level to open. Empty = restart the current level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	TSoftObjectPtr<UWorld> NextLevel;

	/** Played when the player steps in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	TObjectPtr<USoundBase> EnterSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (ClampMin = "0"))
	float FadeTime = 0.8f;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void Travel();

	FTimerHandle TravelTimer;
	bool bTriggered = false;
};
