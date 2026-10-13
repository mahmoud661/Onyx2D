// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Onyx2DTypes.h"
#include "OnyxDrawOrderComponent.generated.h"

/**
 * Sets where this actor's sprites draw, instead of the layer Onyx2D picks by itself. Only needed for special cases: a
 * pickup that should show behind the tiles, a boss that should draw over the player, a vine in front of everything.
 * Sprites inside the actor are ordered by their Translucency Sort Priority.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Draw Order"))
class ONYX2DRUNTIME_API UOnyxDrawOrderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxDrawOrderComponent();

	/** Which layer the actor's sprites draw in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Draw Order")
	EOnyxDrawLayer Layer = EOnyxDrawLayer::Props;

	/** Higher draws in front of the rest of its layer, lower behind. 0 is the middle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Draw Order", meta = (ClampMin = "-24", ClampMax = "23"))
	int32 Order = 0;

	/** Changes the layer and order now. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Draw Order")
	void SetDrawOrder(EOnyxDrawLayer NewLayer, int32 NewOrder);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void OnRegister() override;
};
