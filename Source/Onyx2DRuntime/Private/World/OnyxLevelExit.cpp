// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxLevelExit.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/OnyxPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

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
	Visual->SetRelativeLocation(FVector(0.f, -60.f, 0.f));
	Visual->SetRelativeScale3D(FVector(0.9f, 0.1f, 1.8f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Visual->SetStaticMesh(Cube.Object);
}

void AOnyxLevelExit::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UOnyx2DLibrary::ApplySolidColor(Visual, FLinearColor(0.55f, 0.35f, 0.9f));
}

void AOnyxLevelExit::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AOnyxLevelExit::HandleOverlap);
}

void AOnyxLevelExit::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AOnyxPlayerCharacter* Player = Cast<AOnyxPlayerCharacter>(OtherActor);
	if (!Player || bTriggered)
	{
		return;
	}
	bTriggered = true;
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

void AOnyxLevelExit::Travel()
{
	if (NextLevel.IsNull())
	{
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
		return;
	}
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, NextLevel);
}
