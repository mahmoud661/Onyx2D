// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxFlipbookEffect.h"
#include "Components/SceneComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Engine/World.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"

namespace
{
	/** Effects sit just in front of the gameplay plane so they draw over characters and tiles. */
	constexpr float EffectDepth = 20.f;
}

AOnyxFlipbookEffect::AOnyxFlipbookEffect()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(RootComponent);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sprite->SetCastShadow(false);
	Sprite->SetLooping(false);
}

AOnyxFlipbookEffect* AOnyxFlipbookEffect::Spawn(UObject* WorldContext, UPaperFlipbook* Flipbook, FVector Location, float Scale)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || !Flipbook || Flipbook->GetNumFrames() == 0)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Location.Y = EffectDepth;
	AOnyxFlipbookEffect* Effect = World->SpawnActor<AOnyxFlipbookEffect>(Location, FRotator::ZeroRotator, Params);
	if (Effect)
	{
		Effect->SetActorScale3D(FVector(Scale));
		Effect->Sprite->SetFlipbook(Flipbook);
		UOnyx2DLibrary::CenterFlipbook(Effect->Sprite); // plays centred on the spot, whatever the sprites' pivot
		Effect->Sprite->OnFinishedPlaying.AddUniqueDynamic(Effect, &AOnyxFlipbookEffect::HandleFinished);
		Effect->Sprite->PlayFromStart();
		Effect->SetLifeSpan(Flipbook->GetTotalDuration() + 1.f); // safety net
	}
	return Effect;
}

void AOnyxFlipbookEffect::HandleFinished()
{
	Destroy();
}
