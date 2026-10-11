// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxOneWayPlatform.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Core/OnyxWorldSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AOnyxOneWayPlatform::AOnyxOneWayPlatform()
{
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	RootComponent = Collision;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Visual->SetStaticMesh(Cube.Object);
}

void AOnyxOneWayPlatform::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const FVector Extent(Size.X * 0.5f, Depth * 0.5f, Size.Y * 0.5f);
	Collision->SetBoxExtent(Extent);
	Visual->SetRelativeScale3D(Extent * 2.f / 100.f); // engine cube is 100 cm
	UOnyx2DLibrary::ApplySolidColor(Visual, Color);
}

float AOnyxOneWayPlatform::GetTopZ() const
{
	return Collision->GetComponentLocation().Z + Collision->GetScaledBoxExtent().Z;
}

void AOnyxOneWayPlatform::BeginPlay()
{
	Super::BeginPlay();
	if (UOnyxWorldSubsystem* Registry = GetWorld()->GetSubsystem<UOnyxWorldSubsystem>())
	{
		Registry->RegisterOneWaySurface(Collision);
	}
}

void AOnyxOneWayPlatform::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UOnyxWorldSubsystem* Registry = GetWorld()->GetSubsystem<UOnyxWorldSubsystem>())
	{
		Registry->UnregisterOneWaySurface(Collision);
	}
	Super::EndPlay(EndPlayReason);
}
