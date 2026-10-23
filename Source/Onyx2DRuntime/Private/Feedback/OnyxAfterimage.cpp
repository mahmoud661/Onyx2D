// Copyright Onyx2D. All Rights Reserved.

#include "Feedback/OnyxAfterimage.h"
#include "Core/Onyx2DSettings.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"

AOnyxAfterimage::AOnyxAfterimage()
{
	PrimaryActorTick.bCanEverTick = true;
	Image = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Image"));
	Image->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Image->SetCastShadow(false);
	RootComponent = Image;
}

AOnyxAfterimage* AOnyxAfterimage::Spawn(UObject* WorldContext, UPaperSprite* Sprite, const FTransform& Transform, const FLinearColor& Color, float Lifetime)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || !Sprite)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AOnyxAfterimage* Ghost = World->SpawnActor<AOnyxAfterimage>(Transform.GetLocation(), Transform.Rotator(), Params);
	if (!Ghost)
	{
		return nullptr;
	}
	Ghost->SetActorScale3D(Transform.GetScale3D());
	Ghost->Image->SetSprite(Sprite);
	// A see-through silhouette filled with light: a trail, not a second body. (Without the Onyx2D material, Paper2D's
	// translucent one: a faded, tinted copy.)
	UMaterialInterface* Material = UOnyx2DSettings::Get()->AfterimageMaterial.LoadSynchronous();
	if (!Material)
	{
		Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Paper2D/TranslucentUnlitSpriteMaterial.TranslucentUnlitSpriteMaterial"));
	}
	if (Material)
	{
		Ghost->Image->SetMaterial(0, Material);
	}
	Ghost->Color = Color;
	Ghost->Lifetime = FMath::Max(Lifetime, 0.01f);
	Ghost->Image->SetSpriteColor(Color);
	return Ghost;
}

void AOnyxAfterimage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age >= Lifetime)
	{
		Destroy();
		return;
	}
	Image->SetSpriteColor(Color.CopyWithNewOpacity(Color.A * (1.f - Age / Lifetime)));
}
