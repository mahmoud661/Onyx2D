// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxParallaxComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

UOnyxParallaxComponent::UOnyxParallaxComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork; // after cameras have moved
}

void UOnyxParallaxComponent::BeginPlay()
{
	Super::BeginPlay();
	StartLocation = GetComponentLocation();
}

bool UOnyxParallaxComponent::GetCameraLocation(FVector& OutLocation) const
{
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager)
	{
		return false;
	}
	OutLocation = CameraManager->GetCameraLocation();
	return true;
}

void UOnyxParallaxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FVector Camera;
	if (!GetCameraLocation(Camera))
	{
		return;
	}
	if (!bHasCameraStart)
	{
		CameraStart = Camera;
		bHasCameraStart = true;
	}

	const FVector Delta = Camera - CameraStart;
	SetWorldLocation(FVector(StartLocation.X + Delta.X * Factor.X, StartLocation.Y, StartLocation.Z + Delta.Z * Factor.Y));
}
