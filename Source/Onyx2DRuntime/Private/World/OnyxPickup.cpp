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
	const FLinearColor Glow = Type == EOnyxPickupType::Coin ? FLinearColor(1.f, 0.72f, 0.12f)
		: Type == EOnyxPickupType::Health ? FLinearColor(1.f, 0.22f, 0.32f)
		: FLinearColor(0.3f, 0.85f, 1.f);
	UOnyx2DLibrary::ApplyGlowColor(Visual, Glow, 1.1f);
}

void AOnyxPickup::BeginPlay()
{
	Super::BeginPlay();
	Phase = FMath::FRandRange(0.f, UE_TWO_PI);
	VisualBase = GetBobbingComponent()->GetRelativeLocation();
	Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AOnyxPickup::HandleOverlap);
}

void AOnyxPickup::Pop(FVector2D Velocity)
{
	PopVelocity = Velocity;
	bPopping = true;
	bTopDownPop = UOnyx2DLibrary::IsTopDown(this);
	if (bTopDownPop)
	{
		// From above: across the ground (sideways as thrown, scattered up and down the screen), hopping as it goes.
		PopVelocity = FVector2D(Velocity.X, FMath::FRandRange(-0.6f, 0.6f) * Velocity.Y);
		PopRise = Velocity.Y * 0.7f;
		PopHop = 0.f;
	}
}

void AOnyxPickup::TickPopTopDown(float DeltaSeconds)
{
	PopRise -= 1600.f * DeltaSeconds;
	PopHop = FMath::Max(PopHop + PopRise * DeltaSeconds, 0.f);
	PopVelocity *= FMath::Exp(-2.5f * DeltaSeconds); // the ground takes the speed off
	const FVector From = GetActorLocation();
	const FVector Step(PopVelocity.X * DeltaSeconds, 0.f, PopVelocity.Y * DeltaSeconds);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, From, From + Step + Step.GetSafeNormal() * Trigger->GetScaledSphereRadius() * 0.5f,
		FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(OnyxPickupPop), false, this)))
	{
		// Off a wall, losing most of its speed.
		const FVector2D Normal = FVector2D(Hit.ImpactNormal.X, Hit.ImpactNormal.Z).GetSafeNormal();
		PopVelocity = (PopVelocity - 2.f * FVector2D::DotProduct(PopVelocity, Normal) * Normal) * 0.4f;
	}
	else
	{
		SetActorLocation(From + Step);
	}
	GetBobbingComponent()->SetRelativeLocation(VisualBase + FVector(0.f, 0.f, PopHop));
	if (PopHop <= 0.f && PopRise < 0.f)
	{
		bPopping = false;
		CollectIfTouching();
	}
}

void AOnyxPickup::TickPop(float DeltaSeconds)
{
	PopVelocity.Y -= 1600.f * DeltaSeconds; // snappier than world gravity: a coin, not a feather
	const FVector From = GetActorLocation();
	const FVector Step(PopVelocity.X * DeltaSeconds, 0.f, PopVelocity.Y * DeltaSeconds);
	// It rests on its middle half a coin above the ground (the picture is centred on the actor).
	const float Rest = Trigger->GetScaledSphereRadius() * 0.8f;
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, From, From + Step + Step.GetSafeNormal() * Rest, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionQueryParams(SCENE_QUERY_STAT(OnyxPickupPop), false, this)))
	{
		if (Hit.ImpactNormal.Z > 0.5f && PopVelocity.Y <= 0.f)
		{
			SetActorLocation(Hit.ImpactPoint + FVector(0.f, 0.f, Rest));
			bPopping = false;
			CollectIfTouching();
		}
		else if (FMath::Abs(Hit.ImpactNormal.X) > 0.5f)
		{
			PopVelocity.X = -PopVelocity.X * 0.4f; // off a wall
		}
		else
		{
			PopVelocity.Y = FMath::Min(PopVelocity.Y, 0.f); // under a ceiling
		}
		return;
	}
	SetActorLocation(From + Step);
}

void AOnyxPickup::CollectIfTouching()
{
	TArray<AActor*> Touching;
	Trigger->GetOverlappingActors(Touching, APawn::StaticClass());
	for (AActor* Actor : Touching)
	{
		HandleOverlap(Trigger, Actor, nullptr, 0, false, FHitResult());
		if (IsActorBeingDestroyed())
		{
			return;
		}
	}
}

void AOnyxPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPopping)
	{
		bTopDownPop ? TickPopTopDown(DeltaSeconds) : TickPop(DeltaSeconds);
		return;
	}
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
		if (Type == EOnyxPickupType::Item)
		{
			Score->AddItem(ItemName, FMath::Max(Amount, 1));
		}
		else
		{
			Score->AddCoins(Amount);
		}
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
