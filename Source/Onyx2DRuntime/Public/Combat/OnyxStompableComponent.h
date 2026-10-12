// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OnyxStompableComponent.generated.h"

/**
 * Jumping on the character's head defeats it and bounces the jumper up (Mario style) instead of the jumper getting
 * hurt by its Contact Damage.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Stompable"))
class ONYX2DRUNTIME_API UOnyxStompableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxStompableComponent();

	/** Upward speed given to the jumper after a stomp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Stomp", meta = (ClampMin = "0"))
	float Bounce = 700.f;

	/** True (and handles it) when Stomper landed on top of the owner. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Stomp")
	bool TryStomp(AActor* Stomper);

protected:
	virtual void BeginPlay() override;
};
