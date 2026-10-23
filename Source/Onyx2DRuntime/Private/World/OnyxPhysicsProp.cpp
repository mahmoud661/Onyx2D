// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxPhysicsProp.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Thickness of the body across the level's plane: only matters for its mass and how much water it displaces. */
	constexpr float BodyThickness = 40.f;
	constexpr float DefaultPropSize = 60.f;
}

AOnyxPhysicsProp::AOnyxPhysicsProp()
{
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->InitBoxExtent(FVector(DefaultPropSize * 0.5f, BodyThickness * 0.5f, DefaultPropSize * 0.5f));
	Body->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetSimulatePhysics(true);
	RootComponent = Body;

	SpriteComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Sprite"));
	SpriteComponent->SetupAttachment(Body);
	SpriteComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpriteComponent->SetMobility(EComponentMobility::Movable);

	Placeholder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Placeholder"));
	Placeholder->SetupAttachment(Body);
	Placeholder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Placeholder->SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Placeholder->SetStaticMesh(Cube.Object);
}

FVector2D AOnyxPhysicsProp::GetBodySize() const
{
	if (Size.X > 0.f && Size.Y > 0.f)
	{
		return Size;
	}
	if (Sprite)
	{
		const FBoxSphereBounds Bounds = Sprite->GetRenderBounds();
		return FVector2D(FMath::Max(Bounds.BoxExtent.X * 2.f, 1.f), FMath::Max(Bounds.BoxExtent.Z * 2.f, 1.f));
	}
	return FVector2D(DefaultPropSize);
}

void AOnyxPhysicsProp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const FVector2D BodySize = GetBodySize();
	Body->SetBoxExtent(FVector(BodySize.X * 0.5f, BodyThickness * 0.5f, BodySize.Y * 0.5f));
	Body->SetMassOverrideInKg(NAME_None, Density * BodySize.X * BodyThickness * BodySize.Y / 1000.f, true);

	SpriteComponent->SetSprite(Sprite);
	SpriteComponent->SetVisibility(Sprite != nullptr);
	if (Sprite)
	{
		// Sprites usually pivot on their feet: centre the picture on the body, which turns around its middle.
		SpriteComponent->SetRelativeLocation(-Sprite->GetRenderBounds().Origin * FVector(1.f, 0.f, 1.f));
		UOnyx2DLibrary::ConfigureSpriteLighting(SpriteComponent, false);
	}
	Placeholder->SetVisibility(Sprite == nullptr);
	Placeholder->SetRelativeScale3D(FVector(BodySize.X, BodyThickness * 0.5f, BodySize.Y) / 100.f);
	UOnyx2DLibrary::ApplySolidColor(Placeholder, PlaceholderColor);
}

void AOnyxPhysicsProp::BeginPlay()
{
	Super::BeginPlay();
	if (UOnyx2DLibrary::IsTopDown(this))
	{
		// From above it sits on the ground: nothing pulls it down the screen, it slides where it is pushed and stops, and it
		// never turns (a crate seen from above stays square on).
		Body->SetEnableGravity(false);
		Body->SetLinearDamping(GroundFriction);
		Body->BodyInstance.bLockXRotation = true;
		Body->BodyInstance.bLockYRotation = true;
		Body->BodyInstance.bLockZRotation = true;
		Body->BodyInstance.bLockYTranslation = true;
		Body->BodyInstance.SetDOFLock(EDOFMode::SixDOF);
		return;
	}
	// Moves and turns in the level's plane only (locked here: in the editor the body doesn't simulate, nothing to lock).
	Body->BodyInstance.SetDOFLock(EDOFMode::XZPlane);
}
