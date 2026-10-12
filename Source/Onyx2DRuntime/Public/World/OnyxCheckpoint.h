// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxCheckpoint.generated.h"

class UBoxComponent;
class USoundBase;
class UOnyxLight2DComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxCheckpointSignature, AActor*, Player);

/** Touching it makes it the player's respawn point and heals them (optional). */
UCLASS(meta = (DisplayName = "Onyx Checkpoint"))
class ONYX2DRUNTIME_API AOnyxCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	AOnyxCheckpoint();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxLight2DComponent> Glow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	bool bHealOnActivate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	FLinearColor InactiveColor = FLinearColor(0.25f, 0.27f, 0.32f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	FLinearColor ActiveColor = FLinearColor(0.3f, 0.95f, 0.9f);

	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	bool IsActivated() const { return bActivated; }

	/** Played when the player reaches it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	TObjectPtr<USoundBase> ActivateSound;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D")
	FOnyxCheckpointSignature OnActivated;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ApplyLook();

	bool bActivated = false;
};
