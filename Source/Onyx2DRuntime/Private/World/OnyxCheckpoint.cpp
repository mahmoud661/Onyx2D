// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxCheckpoint.h"
#include "Sound/SoundBase.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Character/OnyxRespawnComponent.h"
#include "GameFramework/Pawn.h"
#include "Combat/OnyxHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Lighting/OnyxLight2DComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Core/Onyx2DTypes.h"
#include "Engine/StaticMesh.h"
#include "Feedback/OnyxFeedbackSubsystem.h"
#include "Lighting/OnyxLight2D.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "World/OnyxFlipbookEffect.h"

AOnyxCheckpoint::AOnyxCheckpoint()
{
	PrimaryActorTick.bCanEverTick = true; // only while its glow settles after it lights
	PrimaryActorTick.bStartWithTickEnabled = false;

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
	Glow->bOwnGlow = true; // its picture turns most of it away (Light Guard): it lights what is around it

	Look = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Look"));
	Look->SetupAttachment(Trigger);
	Look->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Look->SetGenerateOverlapEvents(false);
	Look->SetRelativeLocation(FVector(0.f, 0.f, -Trigger->GetUnscaledBoxExtent().Z)); // on the floor; the draw order keeps it behind the player

	GlowFlicker = CreateDefaultSubobject<UOnyxLightFlickerComponent>(TEXT("GlowFlicker"));
	GlowFlicker->Animation = EOnyxLightAnimation::Flicker;
	GlowFlicker->Strength = 0.12f;
	GlowFlicker->Speed = 7.f;
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
	Look->SetCustomPrimitiveDataFloat(Onyx2D::LightGuardDataIndex, FMath::Clamp(LightGuard, 0.f, 1.f));
}

void AOnyxCheckpoint::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Player = Cast<APawn>(OtherActor);
	if (!Player || !Player->IsPlayerControlled())
	{
		return;
	}
	if (bActivated)
	{
		// Back at a resting place: whole again.
		UOnyxHealthComponent* Health = Player->FindComponentByClass<UOnyxHealthComponent>();
		if (bHealOnActivate && bHealOnEveryVisit && Health && !Health->IsDead() && Health->GetHealth() < Health->MaxHealth)
		{
			Health->Heal(Health->MaxHealth);
			if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
			{
				Feedback->FlashActor(Player, ActiveColor, 0.2f);
			}
			UOnyxAudioSubsystem::PlaySoundAt(this, ActivateSound, GetActorLocation());
			PlayEffect(RestEffect);
		}
		return;
	}
	bActivated = true;
	if (UOnyxRespawnComponent* Respawn = Player->FindComponentByClass<UOnyxRespawnComponent>())
	{
		Respawn->SetRespawnLocation(GetActorLocation() + FVector(0.f, 0.f, 20.f));
	}
	if (UOnyxHealthComponent* Health = Player->FindComponentByClass<UOnyxHealthComponent>(); Health && bHealOnActivate)
	{
		Health->Heal(Health->MaxHealth);
	}
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
	{
		Feedback->AddTrauma(0.15f);
	}
	UOnyxAudioSubsystem::PlaySoundAt(this, ActivateSound, GetActorLocation());
	ApplyLook();
	// It catches light: a burst, its glow flaring and settling.
	PlayEffect(ActivateEffect);
	FlareLeft = KindleTime;
	SetActorTickEnabled(true);
	Tick(0.f);
	OnActivated.Broadcast(Player);
}

void AOnyxCheckpoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FlareLeft = FMath::Max(FlareLeft - DeltaSeconds, 0.f);
	const float Alpha = FlareLeft / FMath::Max(KindleTime, 0.05f);
	Glow->Intensity = ActiveGlow * (1.f + (KindleFlare - 1.f) * Alpha * Alpha);
	if (FlareLeft <= 0.f)
	{
		SetActorTickEnabled(false);
	}
}

void AOnyxCheckpoint::PlayEffect(UPaperFlipbook* Effect)
{
	if (Effect)
	{
		AOnyxFlipbookEffect::Spawn(this, Effect, Glow->GetComponentLocation());
	}
}

void AOnyxCheckpoint::ApplyLook()
{
	// Drawn, or the plain post.
	UPaperFlipbook* Shown = bActivated && ActiveLook ? ActiveLook.Get() : DormantLook.Get();
	Look->SetFlipbook(Shown);
	Look->SetVisibility(Shown != nullptr);
	Visual->SetVisibility(Shown == nullptr);
	if (!Shown)
	{
		UOnyx2DLibrary::ApplySolidColor(Visual, bActivated ? ActiveColor : InactiveColor);
	}
	Glow->Color = ActiveColor;
	Glow->Intensity = bActivated ? ActiveGlow : DormantGlow;
}
