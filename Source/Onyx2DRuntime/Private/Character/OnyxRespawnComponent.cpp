// Copyright Onyx2D. All Rights Reserved.

#include "Character/OnyxRespawnComponent.h"
#include "Camera/OnyxCameraComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Movement/OnyxMovementComponent.h"
#include "TimerManager.h"

UOnyxRespawnComponent::UOnyxRespawnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOnyxRespawnComponent::BeginPlay()
{
	Super::BeginPlay();
	RespawnLocation = GetOwner()->GetActorLocation();
	if (UOnyxHealthComponent* Health = GetOwner()->FindComponentByClass<UOnyxHealthComponent>())
	{
		Health->OnDeath.AddUniqueDynamic(this, &UOnyxRespawnComponent::HandleDeath);
	}
}

void UOnyxRespawnComponent::HandleDeath(AActor* Killer)
{
	if (RespawnDelay > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &UOnyxRespawnComponent::Respawn, RespawnDelay, false);
	}
}

void UOnyxRespawnComponent::Respawn()
{
	AActor* Owner = GetOwner();
	Owner->SetActorLocation(RespawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
	if (UCharacterMovementComponent* Movement = Owner->FindComponentByClass<UCharacterMovementComponent>())
	{
		Movement->StopMovementImmediately();
	}
	if (UOnyxMovementComponent* OnyxMovement = Owner->FindComponentByClass<UOnyxMovementComponent>())
	{
		OnyxMovement->SetMovementLocked(false);
	}
	if (UOnyxHealthComponent* Health = Owner->FindComponentByClass<UOnyxHealthComponent>())
	{
		Health->Revive();
		Health->SetInvulnerableFor(InvulnerableTime);
	}
	if (UOnyxCameraComponent* Camera = Owner->FindComponentByClass<UOnyxCameraComponent>())
	{
		Camera->SnapToTarget();
	}
}
