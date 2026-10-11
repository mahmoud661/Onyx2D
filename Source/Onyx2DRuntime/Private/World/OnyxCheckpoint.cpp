// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxCheckpoint.h"
#include "Character/OnyxPlayerCharacter.h"
#include "Combat/OnyxHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Lighting/OnyxLight2DComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Engine/StaticMesh.h"
#include "Feedback/OnyxFeedbackSubsystem.h"
#include "UObject/ConstructorHelpers.h"

AOnyxCheckpoint::AOnyxCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->InitBoxExtent(FVector(50.f, 100.f, 100.f));
	Trigger->SetCollisionProfileName(UCollisionProfile::DefaultProjectile_ProfileName);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = Trigger;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Trigger);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetRelativeLocation(FVector(0.f, 0.f, -40.f));
	Visual->SetRelativeScale3D(FVector(0.15f, 0.15f, 1.2f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Visual->SetStaticMesh(Cube.Object);

	Glow = CreateDefaultSubobject<UOnyxLight2DComponent>(TEXT("Glow2D"));
	Glow->SetupAttachment(Trigger);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	Glow->Intensity = 0.f;
	Glow->Radius = 380.f;
	Glow->bCastShadows = false;
}

void AOnyxCheckpoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook();
}

void AOnyxCheckpoint::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AOnyxCheckpoint::HandleOverlap);
}

void AOnyxCheckpoint::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AOnyxPlayerCharacter* Player = Cast<AOnyxPlayerCharacter>(OtherActor);
	if (!Player || bActivated)
	{
		return;
	}
	bActivated = true;
	Player->SetRespawnLocation(GetActorLocation() + FVector(0.f, 0.f, 20.f));
	if (bHealOnActivate)
	{
		Player->Health->Heal(Player->Health->MaxHealth);
	}
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
	{
		Feedback->AddTrauma(0.15f);
	}
	ApplyLook();
	OnActivated.Broadcast(Player);
}

void AOnyxCheckpoint::ApplyLook()
{
	UOnyx2DLibrary::ApplySolidColor(Visual, bActivated ? ActiveColor : InactiveColor);
	Glow->Color = ActiveColor;
	Glow->Intensity = bActivated ? 1.4f : 0.f;
}
