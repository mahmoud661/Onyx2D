// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/OnyxHitboxComponent.h"
#include "OnyxContactDamageComponent.generated.h"

/**
 * Hurts whoever touches the character (characters of other teams): spikes on a slime, a bat's body. Always on while
 * the owner is alive; sizes itself to the owner's collision capsule unless Fit To Capsule is off.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Contact Damage"))
class ONYX2DRUNTIME_API UOnyxContactDamageComponent : public UOnyxHitboxComponent
{
	GENERATED_BODY()

public:
	UOnyxContactDamageComponent();

	/** Match the owner's capsule (slightly larger) when play starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Hitbox")
	bool bFitToCapsule = true;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOwnerDeath(AActor* Killer);
};
