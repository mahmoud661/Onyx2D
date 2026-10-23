// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxParallaxComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxParallax, Log, All);

namespace OnyxParallaxStart
{
	/** For this long after play starts, the start waits for the camera to follow a pawn (the player). */
	constexpr double WaitForPlayer = 1.0;
	/** For this long after the start was taken, a camera jump this big (a snap onto the player) takes the start again. */
	constexpr double SettleTime = 0.5;
	constexpr double SnapDistance = 150.0;
}

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
	// Not before the camera has been updated once: until then its view is where the player controller spawned, not where
	// the camera looks from (the first frame ticks this before the cameras), and starting from it would offset every layer.
	if (!CameraManager || CameraManager->GetCameraCacheTime() <= 0.f)
	{
		return false;
	}
	// Nor while the camera still looks from somewhere else than the player (Play in Editor starts it at the controller, before
	// the player is possessed): every layer would then be moved by the gap between the two, for the whole game.
	if (!bHasCameraStart && !Cast<APawn>(CameraManager->GetViewTarget()) && GetWorld()->GetTimeSeconds() < OnyxParallaxStart::WaitForPlayer)
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
	const double Now = GetWorld()->GetTimeSeconds();
	if (!bHasCameraStart)
	{
		CameraStart = Camera;
		CameraStartTime = Now;
		bHasCameraStart = true;
		UE_LOG(LogOnyxParallax, Log, TEXT("%s: the camera starts at X=%.0f Z=%.0f"), *GetName(), Camera.X, Camera.Z);
	}
	else if (Now - CameraStartTime < OnyxParallaxStart::SettleTime && FVector::Dist(Camera, LastCamera) > OnyxParallaxStart::SnapDistance)
	{
		// The camera snapped onto the player just after starting: the start is where it is now.
		CameraStart = Camera;
		UE_LOG(LogOnyxParallax, Log, TEXT("%s: the camera snapped, it starts at X=%.0f Z=%.0f"), *GetName(), Camera.X, Camera.Z);
	}
	LastCamera = Camera;

	const FVector Delta = Camera - CameraStart;
	SetWorldLocation(FVector(StartLocation.X + Delta.X * Factor.X, StartLocation.Y, StartLocation.Z + Delta.Z * Factor.Y));
}
