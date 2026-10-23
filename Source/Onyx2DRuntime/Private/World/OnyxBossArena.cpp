// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxBossArena.h"
#include "AI/OnyxBossComponent.h"
#include "Character/OnyxCharacter.h"
#include "Character/OnyxRespawnComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "World/OnyxGate.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxArena, Log, All);

AOnyxBossArena::AOnyxBossArena()
{
	PrimaryActorTick.bCanEverTick = false;
	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	Area->InitBoxExtent(FVector(900.f, 100.f, 450.f));
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->ShapeColor = FColor(255, 120, 60);
	RootComponent = Area;
	StartTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("StartTrigger"));
	StartTrigger->SetupAttachment(Area);
	StartTrigger->InitBoxExtent(FVector(60.f, 100.f, 300.f));
	StartTrigger->SetCollisionProfileName(UCollisionProfile::DefaultProjectile_ProfileName);
	StartTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	StartTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	StartTrigger->SetGenerateOverlapEvents(true);
}

void AOnyxBossArena::BeginPlay()
{
	Super::BeginPlay();
	StartTrigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AOnyxBossArena::HandleEnter);
	if (UOnyxBossComponent* Brain = GetBossBrain())
	{
		Brain->OnDefeated.AddUniqueDynamic(this, &AOnyxBossArena::HandleBossDefeated);
	}
}

UOnyxBossComponent* AOnyxBossArena::GetBossBrain() const
{
	return Boss ? Boss->FindComponentByClass<UOnyxBossComponent>() : nullptr;
}

void AOnyxBossArena::HandleEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Player = Cast<APawn>(OtherActor);
	const UOnyxHealthComponent* Health = Player ? Player->FindComponentByClass<UOnyxHealthComponent>() : nullptr;
	if (Player && Player->IsPlayerControlled() && !(Health && Health->IsDead()))
	{
		StartFight();
	}
}

void AOnyxBossArena::StartFight()
{
	UOnyxBossComponent* Brain = GetBossBrain();
	if (bFighting || bWon || !Brain || Brain->IsDefeated())
	{
		return;
	}
	bFighting = true;
	GetWorldTimerManager().ClearTimer(Timer);
	for (AOnyxGate* Gate : Gates)
	{
		if (Gate)
		{
			Gate->Close();
		}
	}
	if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (UOnyxHealthComponent* Health = Player->FindComponentByClass<UOnyxHealthComponent>())
		{
			Health->OnDeath.AddUniqueDynamic(this, &AOnyxBossArena::HandlePlayerDeath);
		}
	}
	Brain->StartFight(Area->Bounds.GetBox());
	UE_LOG(LogOnyxArena, Log, TEXT("%s: the gates shut, the fight is on"), *GetName());
	OnFightStarted.Broadcast();
}

void AOnyxBossArena::HandlePlayerDeath(AActor* Killer)
{
	if (!bFighting)
	{
		return;
	}
	// Reset while the screen is dark, just before the player comes back at the last checkpoint.
	float Delay = 1.5f;
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (const UOnyxRespawnComponent* Respawn = Player->FindComponentByClass<UOnyxRespawnComponent>(); Respawn && Respawn->RespawnDelay > 0.f)
		{
			Delay = FMath::Max(Respawn->RespawnDelay - 0.05f, 0.1f);
		}
	}
	GetWorldTimerManager().SetTimer(Timer, this, &AOnyxBossArena::ResetArena, Delay, false);
}

void AOnyxBossArena::ResetArena()
{
	bFighting = false;
	if (UOnyxBossComponent* Brain = GetBossBrain())
	{
		Brain->ResetFight();
	}
	for (AOnyxGate* Gate : Gates)
	{
		if (Gate)
		{
			Gate->ResetGate();
		}
	}
	UE_LOG(LogOnyxArena, Log, TEXT("%s: reset (the player fell)"), *GetName());
	OnFightReset.Broadcast();
}

void AOnyxBossArena::HandleBossDefeated()
{
	bFighting = false;
	bWon = true;
	GetWorldTimerManager().SetTimer(Timer, this, &AOnyxBossArena::OpenGates, FMath::Max(OpenDelay, 0.01f), false);
	UE_LOG(LogOnyxArena, Log, TEXT("%s: won"), *GetName());
	OnFightWon.Broadcast();
}

void AOnyxBossArena::OpenGates()
{
	for (AOnyxGate* Gate : Gates)
	{
		if (Gate)
		{
			Gate->Open();
		}
	}
}
