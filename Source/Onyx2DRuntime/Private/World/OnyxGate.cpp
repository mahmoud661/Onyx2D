// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxGate.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Components/BoxComponent.h"
#include "Feedback/OnyxFeedbackSubsystem.h"
#include "PaperSpriteComponent.h"
#include "World/OnyxFlipbookEffect.h"

AOnyxGate::AOnyxGate()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Leaf = CreateDefaultSubobject<USceneComponent>(TEXT("Leaf"));
	Leaf->SetupAttachment(RootComponent);
	Block = CreateDefaultSubobject<UBoxComponent>(TEXT("Block"));
	Block->SetupAttachment(Leaf);
	Block->InitBoxExtent(FVector(16.f, 100.f, 96.f));
	Block->SetCollisionObjectType(ECC_WorldStatic);
	Block->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Block->SetCollisionResponseToAllChannels(ECR_Block);
	Block->SetCanEverAffectNavigation(false);
	Look = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Look"));
	Look->SetupAttachment(Leaf);
	Look->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Look->SetRelativeLocation(FVector(0.f, -12.f, 0.f)); // just behind the walls: raised, it hides in the stone above the doorway
}

void AOnyxGate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	bWantClosed = bStartsClosed;
	SetShut(bStartsClosed ? 1.f : 0.f);
}

void AOnyxGate::BeginPlay()
{
	Super::BeginPlay();
	ResetGate();
}

void AOnyxGate::Open()
{
	if (bWantClosed)
	{
		bWantClosed = false;
		SetActorTickEnabled(true);
		UOnyxAudioSubsystem::PlaySoundAt(this, OpenSound, GetActorLocation(), EOnyxSoundCategory::Effects);
	}
}

void AOnyxGate::Close()
{
	if (!bWantClosed)
	{
		bWantClosed = true;
		SetActorTickEnabled(true);
	}
}

void AOnyxGate::ResetGate()
{
	bWantClosed = bStartsClosed;
	SetShut(bStartsClosed ? 1.f : 0.f);
	SetActorTickEnabled(false);
}

void AOnyxGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Target = bWantClosed ? 1.f : 0.f;
	const float Rate = 1.f / FMath::Max(bWantClosed ? CloseTime : OpenTime, 0.02f);
	const float Before = Shut;
	SetShut(FMath::FInterpConstantTo(Shut, Target, DeltaSeconds, Rate));
	if (Shut == Target)
	{
		SetActorTickEnabled(false);
		if (bWantClosed && Before < 1.f)
		{
			// Slammed shut: the floor shakes, dust at its foot.
			if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
			{
				Feedback->AddTrauma(SlamTrauma);
			}
			if (SlamEffect)
			{
				AOnyxFlipbookEffect::Spawn(this, SlamEffect, GetActorLocation() - FVector(0.f, 0.f, Block->GetUnscaledBoxExtent().Z - 8.f), 2.f);
			}
			UOnyxAudioSubsystem::PlaySoundAt(this, CloseSound, GetActorLocation(), EOnyxSoundCategory::Effects);
		}
	}
}

void AOnyxGate::SetShut(float NewShut)
{
	Shut = FMath::Clamp(NewShut, 0.f, 1.f);
	// It slams with a little weight: eased in as it drops.
	const float Eased = bWantClosed ? Shut * Shut : Shut;
	Leaf->SetRelativeLocation(FVector(0.f, 0.f, (1.f - Eased) * Travel));
	// It blocks once it is mostly down: nothing slips under a gate as it lands.
	Block->SetCollisionEnabled(Shut > 0.5f ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}
