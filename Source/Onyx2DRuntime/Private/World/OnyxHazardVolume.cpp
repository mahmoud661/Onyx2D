// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxHazardVolume.h"
#include "Combat/OnyxHitboxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AOnyxHazardVolume::AOnyxHazardVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Hitbox = CreateDefaultSubobject<UOnyxHitboxComponent>(TEXT("Hitbox"));
	Hitbox->bContinuous = true;
	Hitbox->Damage = 1.f;
	Hitbox->Knockback = FVector2D(300.f, 700.f);
	Hitbox->HitStopDuration = 0.f;
	Hitbox->DamageType = TEXT("Hazard");
	RootComponent = Hitbox;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Hitbox);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Visual->SetStaticMesh(Cube.Object);
}

void AOnyxHazardVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const FVector Extent(Size.X * 0.5f, 60.f, Size.Y * 0.5f);
	Hitbox->SetBoxExtent(Extent);
	Visual->SetRelativeScale3D(Extent * 2.f / 100.f);

	UOnyx2DLibrary::ApplySolidColor(Visual, Color);
}
