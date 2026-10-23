// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxBed.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Core/OnyxSaveSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "TimerManager.h"
#include "Time/OnyxCalendarSubsystem.h"

AOnyxBed::AOnyxBed()
{
	PrimaryActorTick.bCanEverTick = false;
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->InitBoxExtent(FVector(40.f, 100.f, 40.f));
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetCanEverAffectNavigation(false);
	Trigger->bDrawOnlyIfSelected = true;
	RootComponent = Trigger;

	Sprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Trigger);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AOnyxBed::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Sprite->SetSprite(Look);
	Sprite->SetVisibility(Look != nullptr);
	if (Look)
	{
		UOnyx2DLibrary::ConfigureSpriteLighting(Sprite, false);
	}
}

void AOnyxBed::BeginPlay()
{
	Super::BeginPlay();
	if (UOnyxCalendarSubsystem* Calendar = UOnyxCalendarSubsystem::Get(this))
	{
		Calendar->OnPassedOut.AddUniqueDynamic(this, &AOnyxBed::HandlePassedOut);
	}
}

void AOnyxBed::Interact_Implementation(AActor* User)
{
	Sleep(User);
}

void AOnyxBed::HandlePassedOut()
{
	// Out on their feet at the end of the day: carried home to bed.
	Sleep(UGameplayStatics::GetPlayerPawn(this, 0));
}

void AOnyxBed::Sleep(AActor* Sleeper)
{
	if (bSleeping)
	{
		return;
	}
	bSleeping = true;
	SleepingActor = Sleeper;
	if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
	{
		Controller->PlayerCameraManager->StartCameraFade(0.f, 1.f, FadeTime, FLinearColor::Black, false, true);
		if (APawn* Pawn = Controller->GetPawn())
		{
			Pawn->DisableInput(Controller);
		}
	}
	GetWorldTimerManager().SetTimer(WakeTimer, this, &AOnyxBed::Wake, FadeTime + DarkTime * 0.5f, false);
}

void AOnyxBed::Wake()
{
	// The night passes while the screen is dark: a new day, the game saved, back on your feet by the bed.
	if (UOnyxCalendarSubsystem* Calendar = UOnyxCalendarSubsystem::Get(this))
	{
		Calendar->StartNewDay();
	}
	if (AActor* Sleeper = SleepingActor.Get())
	{
		const ACharacter* Character = Cast<ACharacter>(Sleeper);
		const float HalfHeight = Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
		Sleeper->SetActorLocation(GetActorLocation() + FVector(0.f, Sleeper->GetActorLocation().Y - GetActorLocation().Y, HalfHeight - 40.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (bSaveOnSleep)
	{
		if (UOnyxSaveSubsystem* Save = UOnyxSaveSubsystem::Get(this))
		{
			Save->SaveGame();
		}
	}
	if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
	{
		Controller->PlayerCameraManager->StartCameraFade(1.f, 0.f, FadeTime, FLinearColor::Black, false, false);
		if (APawn* Pawn = Controller->GetPawn())
		{
			Pawn->EnableInput(Controller);
		}
	}
	bSleeping = false;
}
