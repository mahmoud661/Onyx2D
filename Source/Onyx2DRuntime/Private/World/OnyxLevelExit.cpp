// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxLevelExit.h"
#include "Sound/SoundBase.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/OnyxPlayerControlComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Lighting/OnyxLight2D.h"
#include "Lighting/OnyxLight2DComponent.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "World/OnyxFlipbookEffect.h"

AOnyxLevelExit::AOnyxLevelExit()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->InitBoxExtent(FVector(50.f, 100.f, 90.f));
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = Trigger;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Trigger);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// On the gameplay plane like everything else: the draw order keeps the player in front of the door.
	Visual->SetRelativeScale3D(FVector(0.9f, 0.1f, 1.8f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Visual->SetStaticMesh(Cube.Object);

	LookComponent = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Look"));
	LookComponent->SetupAttachment(Trigger);
	LookComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LookComponent->SetGenerateOverlapEvents(false);

	Glow = CreateDefaultSubobject<UOnyxLight2DComponent>(TEXT("Glow2D"));
	Glow->SetupAttachment(Trigger);
	Glow->Intensity = 0.f;
	Glow->bCastShadows = false;
	Glow->bOwnGlow = true; // its picture turns most of it away (Light Guard): it lights what is round it

	GlowPulse = CreateDefaultSubobject<UOnyxLightFlickerComponent>(TEXT("GlowPulse"));
	GlowPulse->Animation = EOnyxLightAnimation::Pulse;
	GlowPulse->Strength = 0.18f;
	GlowPulse->Speed = 1.2f;
}

void AOnyxLevelExit::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UOnyx2DLibrary::ApplySolidColor(Visual, FLinearColor(0.55f, 0.35f, 0.9f));
	const float Bottom = -Trigger->GetUnscaledBoxExtent().Z;
	Visual->SetVisibility(Look == nullptr);
	LookComponent->SetFlipbook(Look);
	LookComponent->SetVisibility(Look != nullptr);
	LookComponent->SetRelativeLocation(FVector(0.f, 0.f, Bottom)); // on the floor; the draw order keeps the player in front
	if (Look)
	{
		UOnyx2DLibrary::ConfigureSpriteLighting(LookComponent, false);
	}
	Glow->SetRelativeLocation(FVector(0.f, 0.f, Bottom + GlowHeight));
	Glow->Color = GlowColor;
	Glow->Radius = GlowRadius;
	Glow->Intensity = GlowIntensity;
	Glow->SetVisibility(GlowIntensity > 0.f);
}

void AOnyxLevelExit::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AOnyxLevelExit::HandleOverlap);
}

void AOnyxLevelExit::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Player = Cast<APawn>(OtherActor);
	if (!Player || !Player->IsPlayerControlled() || bTriggered)
	{
		return;
	}
	bTriggered = true;
	UOnyxAudioSubsystem::PlaySound2D(this, EnterSound);
	AOnyxFlipbookEffect::Spawn(this, EnterEffect, Trigger->GetComponentLocation());
	if (GlowIntensity > 0.f)
	{
		// The light swells as the screen goes dark.
		FlareTime = 0.f;
		GetWorldTimerManager().SetTimer(FlareTimer, this, &AOnyxLevelExit::Flare, 1.f / 60.f, true);
	}
	if (UOnyxPlayerControlComponent* Control = Player->FindComponentByClass<UOnyxPlayerControlComponent>())
	{
		Control->SetControlEnabled(false);
	}
	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		Player->DisableInput(PC);
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, FadeTime, FLinearColor::Black, false, true);
		}
	}
	GetWorldTimerManager().SetTimer(TravelTimer, this, &AOnyxLevelExit::Travel, FMath::Max(FadeTime, 0.01f), false);
}

void AOnyxLevelExit::Flare()
{
	FlareTime += 1.f / 60.f;
	const float Alpha = FMath::Clamp(FlareTime / FMath::Max(FadeTime, 0.05f), 0.f, 1.f);
	Glow->Intensity = GlowIntensity * (1.f + 2.f * Alpha);
	Glow->Radius = GlowRadius * (1.f + 0.6f * Alpha);
	if (Alpha >= 1.f)
	{
		GetWorldTimerManager().ClearTimer(FlareTimer);
	}
}

void AOnyxLevelExit::Travel()
{
	if (NextLevel.IsNull())
	{
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
		return;
	}
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, NextLevel);
}
