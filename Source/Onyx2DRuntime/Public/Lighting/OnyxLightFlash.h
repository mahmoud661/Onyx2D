// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxLightFlash.generated.h"

class UOnyxLight2DComponent;

/** A 2D light that appears for a moment and fades out (muzzle flash, sword sparks, an explosion). Removes itself. */
UCLASS(NotPlaceable)
class ONYX2DRUNTIME_API AOnyxLightFlash : public AActor
{
	GENERATED_BODY()

public:
	AOnyxLightFlash();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxLight2DComponent> Light2D;

	/** Spawns a flash at Location that fades out over Duration seconds. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Lighting", meta = (WorldContext = "WorldContext"))
	static AOnyxLightFlash* Spawn(UObject* WorldContext, FVector Location, FLinearColor Color = FLinearColor(1.f, 0.8f, 0.5f),
		float Intensity = 3.f, float Radius = 350.f, float Duration = 0.2f, bool bCastShadows = false);

	virtual void Tick(float DeltaSeconds) override;

private:
	float Duration = 0.2f;
	float Elapsed = 0.f;
	float StartIntensity = 3.f;
};
