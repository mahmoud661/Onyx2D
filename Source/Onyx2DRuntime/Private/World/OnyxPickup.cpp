// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxPickup.h"
#include "Sound/SoundBase.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Combat/OnyxHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Core/OnyxScoreSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PaperFlipbookComponent.h"
#include "World/OnyxFlipbookEffect.h"
#include "UObject/ConstructorHelpers.h"

AOnyxPickup::AOnyxPickup()
{
	PrimaryActorTick.bCanEverTick = true;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(32.f);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	RootComponent = Trigger;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Trigger);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);
	Visual->SetRelativeScale3D(FVector(0.3f, 0.1f, 0.3f));
	Visual->SetRelativeRotation(FRotator(45.f, 0.f, 0.f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Visual->SetStaticMesh(Cube.Object);

	Animated = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Animated"));
	Animated->SetupAttachment(Trigger);
	Animated->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Animated->SetCastShadow(false);
}

void AOnyxPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const bool bAnimated = Flipbook != nullptr;
	Animated->SetFlipbook(Flipbook);
	Animated->SetVisibility(bAnimated);
	Visual->SetVisibility(!bAnimated);
	if (bAnimated)
	{
		UOnyx2DLibrary::CenterFlipbook(Animated);
		UOnyx2DLibrary::ConfigureSpriteLighting(Animated, false);
		return;
	}
	UOnyx2DLibrary::ApplyGlowColor(Visual, Type == EOnyxPickupType::Coin ? FLinearColor(1.f, 0.72f, 0.12f) : FLinearColor(1.f, 0.22f, 0.32f), 1.1f);
}

void AOnyxPickup::BeginPlay()
{
	Super::BeginPlay();
	Phase = FMath::FRandRange(0.f, UE_TWO_PI);
	VisualBase = GetBobbingComponent()->GetRelativeLocation();
	Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AOnyxPickup::HandleOverlap);
}

void AOnyxPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Phase += DeltaSeconds * BobSpeed;
	float Bob = FMath::Sin(Phase) * BobHeight;
	// Animated pickups bob in whole art pixels, so the pixel art stays crisp.
	const UPaperFlipbook* Look = Animated->GetFlipbook();
	const UPaperSprite* Frame = Flipbook && Look && Look->GetNumKeyFrames() > 0 ? Look->GetSpriteAtFrame(0) : nullptr;
	if (Frame && Frame->GetPixelsPerUnrealUnit() > KINDA_SMALL_NUMBER)
	{
		const float Pixel = Animated->GetComponentScale().Z / Frame->GetPixelsPerUnrealUnit();
		const float WorldZ = GetActorLocation().Z + VisualBase.Z + Bob;
		Bob += FMath::GridSnap(WorldZ, Pixel) - WorldZ;
	}
	GetBobbingComponent()->SetRelativeLocation(VisualBase + FVector(0.f, 0.f, Bob));
}

void AOnyxPickup::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Player = Cast<APawn>(OtherActor);
	if (!Player || !Player->IsPlayerControlled() || IsActorBeingDestroyed())
	{
		return;
	}

	if (Type == EOnyxPickupType::Health)
	{
		UOnyxHealthComponent* Health = Player->FindComponentByClass<UOnyxHealthComponent>();
		if (!Health || Health->GetHealth() >= Health->MaxHealth)
		{
			return; // leave it for later
		}
		Health->Heal(Amount);
	}
	else if (UOnyxScoreSubsystem* Score = UOnyxScoreSubsystem::Get(this))
	{
		Score->AddCoins(Amount);
	}

	UOnyxAudioSubsystem::PlaySoundAt(this, CollectSound, GetActorLocation());
	AOnyxFlipbookEffect::Spawn(this, CollectEffect, GetBobbingComponent()->GetComponentLocation(), Animated->GetComponentScale().X);
	OnCollected.Broadcast(Player);
	Destroy();
}

USceneComponent* AOnyxPickup::GetBobbingComponent() const
{
	return Flipbook ? static_cast<USceneComponent*>(Animated) : static_cast<USceneComponent*>(Visual);
}
