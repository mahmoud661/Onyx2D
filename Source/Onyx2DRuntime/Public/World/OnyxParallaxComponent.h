// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "OnyxParallaxComponent.generated.h"

/**
 * Parallax layer: attach background sprites under it. Factor 0 = moves with the world,
 * 1 = sticks to the camera (infinitely far), 0.5 = halfway. Works with any camera.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent))
class ONYX2DRUNTIME_API UOnyxParallaxComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UOnyxParallaxComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Parallax")
	FVector2D Factor = FVector2D(0.5f, 0.2f);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool GetCameraLocation(FVector& OutLocation) const;

	FVector StartLocation = FVector::ZeroVector;
	FVector CameraStart = FVector::ZeroVector;
	bool bHasCameraStart = false;
};
