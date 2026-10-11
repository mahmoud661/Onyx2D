// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxHazardVolume.generated.h"

class UOnyxHitboxComponent;
class UStaticMeshComponent;

/** Spikes, lava, saw blades: hurts anything that touches it. */
UCLASS(meta = (DisplayName = "Onyx Hazard"))
class ONYX2DRUNTIME_API AOnyxHazardVolume : public AActor
{
	GENERATED_BODY()

public:
	AOnyxHazardVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxHitboxComponent> Hitbox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Width (X) and height (Z). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	FVector2D Size = FVector2D(100.f, 40.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	FLinearColor Color = FLinearColor(0.9f, 0.15f, 0.1f);

	virtual void OnConstruction(const FTransform& Transform) override;
};
