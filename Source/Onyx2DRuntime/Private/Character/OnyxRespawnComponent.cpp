// Copyright Onyx2D. All Rights Reserved.

#include "Character/OnyxRespawnComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/OnyxCharacter.h"
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
		if (FadeTime > 0.f && GetCameraManager())
		{
			GetWorld()->GetTimerManager().SetTimer(FadeTimer, this, &UOnyxRespawnComponent::FadeOut, FMath::Max(RespawnDelay - FadeTime, 0.01f), false);
		}
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
	if (AOnyxCharacter* Character = Cast<AOnyxCharacter>(Owner))
	{
		Character->ResetAfterDeath();
	}
	if (UOnyxCameraComponent* Camera = Owner->FindComponentByClass<UOnyxCameraComponent>())
	{
		Camera->SnapToTarget();
	}
	if (APlayerCameraManager* CameraManager = FadeTime > 0.f ? GetCameraManager() : nullptr)
	{
		CameraManager->StartCameraFade(1.f, 0.f, FadeTime, FLinearColor::Black, false, false);
	}
}

void UOnyxRespawnComponent::FadeOut()
{
	if (APlayerCameraManager* CameraManager = GetCameraManager())
	{
		CameraManager->StartCameraFade(0.f, 1.f, FadeTime, FLinearColor::Black, false, true);
	}
}

APlayerCameraManager* UOnyxRespawnComponent::GetCameraManager() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	return Controller ? Controller->PlayerCameraManager : nullptr;
}
