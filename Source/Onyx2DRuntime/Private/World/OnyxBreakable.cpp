// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxBreakable.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Combat/OnyxCombatLibrary.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Core/Onyx2DLibrary.h"
#include "Engine/World.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "Sound/SoundBase.h"
#include "World/OnyxFlipbookEffect.h"
#include "World/OnyxPickup.h"

namespace OnyxBreakable
{
	/** Thickness of the body across the level's plane. */
	constexpr float BodyThickness = 40.f;
	constexpr float DefaultSize = 48.f;
	/** How long a struck breakable rocks, and how fast. */
	constexpr float RockDuration = 0.35f;
	constexpr float RockSpeed = 38.f;
}

AOnyxBreakable::AOnyxBreakable()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Struck like a character (attack hitboxes look for pawns), but it never stands in anyone's way.
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->InitBoxExtent(FVector(OnyxBreakable::DefaultSize * 0.5f, OnyxBreakable::BodyThickness * 0.5f, OnyxBreakable::DefaultSize * 0.5f));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_Pawn);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Body->SetGenerateOverlapEvents(true);
	Body->SetCanEverAffectNavigation(false);
	Body->ShapeColor = FColor(220, 180, 90);

	USceneComponent* Feet = CreateDefaultSubobject<USceneComponent>(TEXT("Feet"));
	RootComponent = Feet;
	Body->SetupAttachment(Feet);

	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	Blocker->SetupAttachment(Feet);
	Blocker->InitBoxExtent(FVector(OnyxBreakable::DefaultSize * 0.5f, 150.f, OnyxBreakable::DefaultSize * 0.5f));
	Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Blocker->SetCanEverAffectNavigation(false);
	Blocker->ShapeColor = FColor(200, 90, 60);

	SpriteComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Sprite"));
	SpriteComponent->SetupAttachment(Feet);
	SpriteComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpriteComponent->SetCastShadow(false);
}

void AOnyxBreakable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SpriteComponent->SetSprite(Sprite);
	SpriteComponent->SetVisibility(Sprite != nullptr);
	FVector2D Size(OnyxBreakable::DefaultSize);
	FVector Middle(0.f, 0.f, OnyxBreakable::DefaultSize * 0.5f);
	if (Sprite)
	{
		// The body covers the picture: blows land where it is drawn.
		const FBoxSphereBounds Bounds = Sprite->GetRenderBounds();
		Size = FVector2D(FMath::Max(Bounds.BoxExtent.X * 2.f, 1.f), FMath::Max(Bounds.BoxExtent.Z * 2.f, 1.f));
		Middle = FVector(Bounds.Origin.X, 0.f, Bounds.Origin.Z);
		UOnyx2DLibrary::ConfigureSpriteLighting(SpriteComponent, false);
	}
	Body->SetBoxExtent(FVector(Size.X * 0.5f, OnyxBreakable::BodyThickness * 0.5f, Size.Y * 0.5f));
	Body->SetRelativeLocation(Middle);
	// Solid: as deep across the plane as the tiles' collision, so nothing gets round it. The body (struck by blows) sits a
	// little proud of it on every side, so swings reach it from where a character stands.
	Blocker->SetBoxExtent(FVector(FMath::Max(Size.X * 0.5f - 4.f, 1.f), 150.f, FMath::Max(Size.Y * 0.5f - 2.f, 1.f)));
	Blocker->SetRelativeLocation(Middle);
	Blocker->SetCollisionProfileName(bSolid ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	if (bSolid)
	{
		Body->SetBoxExtent(FVector(Size.X * 0.5f + 12.f, OnyxBreakable::BodyThickness * 0.5f, Size.Y * 0.5f));
	}
}

bool AOnyxBreakable::ReceiveDamage_Implementation(const FOnyxDamageInfo& Damage)
{
	if (bBroken)
	{
		return false;
	}
	if (bOnlyPlayerBreaks && UOnyxCombatLibrary::GetTeam(Damage.Instigator) != OnyxTeams::Player)
	{
		return false;
	}
	++HitsTaken;
	if (HitsTaken >= Hits)
	{
		Break(Damage.Instigator);
		return true;
	}
	// Not yet: it rocks away from the blow.
	UOnyxAudioSubsystem::PlaySoundAt(this, HitSound, Body->GetComponentLocation(), EOnyxSoundCategory::Effects, 1.f, FMath::FRandRange(0.95f, 1.08f));
	RockSide = Damage.Instigator && Damage.Instigator->GetActorLocation().X > GetActorLocation().X ? -1.f : 1.f;
	RockTime = OnyxBreakable::RockDuration;
	SetActorTickEnabled(true);
	return true;
}

void AOnyxBreakable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RockTime = FMath::Max(RockTime - DeltaSeconds, 0.f);
	// A swing that dies away, turning on its feet (the sprite's pivot) in the level's plane.
	const float Fade = RockTime / OnyxBreakable::RockDuration;
	const float Angle = RockAngle * RockSide * Fade * Fade * FMath::Sin((OnyxBreakable::RockDuration - RockTime) * OnyxBreakable::RockSpeed);
	SpriteComponent->SetRelativeRotation(FRotator(Angle, 0.f, 0.f));
	if (RockTime <= 0.f)
	{
		SetActorTickEnabled(false);
	}
}

void AOnyxBreakable::Break(AActor* Breaker)
{
	if (bBroken)
	{
		return;
	}
	bBroken = true;
	const FVector Feet = GetActorLocation();
	UOnyxAudioSubsystem::PlaySoundAt(this, BreakSound, Body->GetComponentLocation(), EOnyxSoundCategory::Effects, 1.f, FMath::FRandRange(0.94f, 1.06f));
	if (AOnyxFlipbookEffect* Burst = AOnyxFlipbookEffect::Spawn(this, BreakEffect, Feet))
	{
		// Lit like the prop it was, so the pieces match it.
		UOnyx2DLibrary::ConfigureSpriteLighting(Burst->Sprite, false);
	}
	UWorld* World = GetWorld();
	if (World && CoinLook)
	{
		const FTransform Where(Body->GetComponentLocation());
		for (int32 Coin = 0; Coin < Coins; ++Coin)
		{
			// Set up before it finishes spawning, so its construction shows the coin.
			AOnyxPickup* Pickup = World->SpawnActorDeferred<AOnyxPickup>(AOnyxPickup::StaticClass(), Where, nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Pickup)
			{
				continue;
			}
			Pickup->Type = EOnyxPickupType::Coin;
			Pickup->Amount = 1;
			Pickup->Flipbook = CoinLook;
			Pickup->CollectEffect = CoinCollectEffect;
			Pickup->CollectSound = CoinSound;
			Pickup->BobHeight = 4.f;
			Pickup->FinishSpawning(Where);
			// Spilled out and up, a little to each side.
			Pickup->Pop(FVector2D(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(340.f, 560.f)));
		}
	}
	OnBroken.Broadcast(Breaker);
	Destroy();
}
