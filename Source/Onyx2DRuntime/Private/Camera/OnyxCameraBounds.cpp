// Copyright Onyx2D. All Rights Reserved.

#include "Camera/OnyxCameraBounds.h"
#include "Components/BoxComponent.h"
#include "Core/OnyxWorldSubsystem.h"
#include "Engine/World.h"

AOnyxCameraBounds::AOnyxCameraBounds()
{
	PrimaryActorTick.bCanEverTick = false;

	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	Area->InitBoxExtent(FVector(2000.f, 500.f, 1000.f));
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->ShapeColor = FColor(80, 200, 255);
	Area->SetLineThickness(4.f);
	RootComponent = Area;
}

FBox AOnyxCameraBounds::GetBounds() const
{
	return Area->Bounds.GetBox();
}

bool AOnyxCameraBounds::ContainsPoint(const FVector& Point) const
{
	const FBox Box = GetBounds();
	return Point.X >= Box.Min.X && Point.X <= Box.Max.X && Point.Z >= Box.Min.Z && Point.Z <= Box.Max.Z;
}

void AOnyxCameraBounds::BeginPlay()
{
	Super::BeginPlay();
	if (UOnyxWorldSubsystem* Registry = GetWorld()->GetSubsystem<UOnyxWorldSubsystem>())
	{
		Registry->RegisterCameraBounds(this);
	}
}

void AOnyxCameraBounds::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UOnyxWorldSubsystem* Registry = GetWorld()->GetSubsystem<UOnyxWorldSubsystem>())
	{
		Registry->UnregisterCameraBounds(this);
	}
	Super::EndPlay(EndPlayReason);
}
