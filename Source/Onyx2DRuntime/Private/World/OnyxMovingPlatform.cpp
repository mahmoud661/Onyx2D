// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxMovingPlatform.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AOnyxMovingPlatform::AOnyxMovingPlatform()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics; // move before characters so they ride smoothly

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Platform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Platform"));
	Platform->SetupAttachment(RootComponent);
	Platform->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Platform->SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Platform->SetStaticMesh(Cube.Object);
}

void AOnyxMovingPlatform::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Platform->SetRelativeScale3D(FVector(Size.X / 100.f, 2.f, Size.Y / 100.f));
	UOnyx2DLibrary::ApplySolidColor(Platform, Color);
}

void AOnyxMovingPlatform::BeginPlay()
{
	Super::BeginPlay();
	Stops.Reset();
	Stops.Add(GetActorLocation());
	for (const FVector& Point : Waypoints)
	{
		Stops.Add(GetActorTransform().TransformPosition(Point));
	}
	TargetIndex = Stops.Num() > 1 ? 1 : 0;
	Step = 1;
}

void AOnyxMovingPlatform::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Stops.Num() < 2)
	{
		return;
	}
	if (WaitRemaining > 0.f)
	{
		WaitRemaining -= DeltaSeconds;
		return;
	}

	const FVector Current = GetActorLocation();
	const FVector Target = Stops[TargetIndex];
	const FVector ToTarget = Target - Current;
	const float StepLength = Speed * DeltaSeconds;

	if (ToTarget.Size() <= StepLength)
	{
		SetActorLocation(Target, false);
		WaitRemaining = PauseTime;
		if (bPingPong)
		{
			if (TargetIndex + Step >= Stops.Num() || TargetIndex + Step < 0)
			{
				Step = -Step;
			}
			TargetIndex += Step;
		}
		else
		{
			TargetIndex = (TargetIndex + 1) % Stops.Num();
		}
		return;
	}
	SetActorLocation(Current + ToTarget.GetSafeNormal() * StepLength, false);
}
