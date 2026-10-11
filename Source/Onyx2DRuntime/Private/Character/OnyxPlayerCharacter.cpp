// Copyright Onyx2D. All Rights Reserved.

#include "Character/OnyxPlayerCharacter.h"
#include "Camera/OnyxCameraComponent.h"
#include "Combat/OnyxCombatComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Input/OnyxInputConfig.h"
#include "Movement/OnyxMovementComponent.h"
#include "TimerManager.h"

AOnyxPlayerCharacter::AOnyxPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TeamId = OnyxTeams::Player;
	PlaceholderColor = FLinearColor(0.2f, 0.85f, 0.9f);

	Camera = CreateDefaultSubobject<UOnyxCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());

	AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

void AOnyxPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	RespawnLocation = GetActorLocation();
}

void AOnyxPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UOnyxInputSubsystem* InputSubsystem = UOnyxInputSubsystem::Get(Cast<APlayerController>(GetController()));
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	const UOnyxInputConfig* Config = InputSubsystem ? InputSubsystem->GetInputConfig() : nullptr;
	if (!Input || !Config)
	{
		return;
	}
	InputSubsystem->ApplyMappingContext();

	if (Config->Move)
	{
		Input->BindAction(Config->Move, ETriggerEvent::Triggered, this, &AOnyxPlayerCharacter::HandleMove);
		Input->BindAction(Config->Move, ETriggerEvent::Completed, this, &AOnyxPlayerCharacter::HandleMoveReleased);
	}
	if (Config->Jump)
	{
		Input->BindAction(Config->Jump, ETriggerEvent::Started, this, &AOnyxPlayerCharacter::HandleJumpPressed);
		Input->BindAction(Config->Jump, ETriggerEvent::Completed, this, &AOnyxPlayerCharacter::HandleJumpReleased);
	}
	if (Config->Dash)
	{
		Input->BindAction(Config->Dash, ETriggerEvent::Started, this, &AOnyxPlayerCharacter::HandleDash);
	}
	if (Config->Attack)
	{
		Input->BindAction(Config->Attack, ETriggerEvent::Started, this, &AOnyxPlayerCharacter::HandleAttack);
	}
	if (Config->Down)
	{
		Input->BindAction(Config->Down, ETriggerEvent::Started, this, &AOnyxPlayerCharacter::HandleDownPressed);
		Input->BindAction(Config->Down, ETriggerEvent::Completed, this, &AOnyxPlayerCharacter::HandleDownReleased);
	}
	if (Config->Interact)
	{
		Input->BindAction(Config->Interact, ETriggerEvent::Started, this, &AOnyxPlayerCharacter::HandleInteract);
	}
}

void AOnyxPlayerCharacter::HandleMove(const FInputActionValue& Value)
{
	MoveInput = Value.Get<float>();
	AddMovementInput(FVector::ForwardVector, MoveInput);
}

void AOnyxPlayerCharacter::HandleMoveReleased(const FInputActionValue& Value)
{
	MoveInput = 0.f;
}

void AOnyxPlayerCharacter::HandleJumpPressed(const FInputActionValue& Value)
{
	UOnyxMovementComponent* Movement = GetOnyxMovement();
	if (!Movement || (bDownHeld && Movement->TryDropThroughPlatform()))
	{
		return;
	}
	Movement->JumpPressed();
}

void AOnyxPlayerCharacter::HandleJumpReleased(const FInputActionValue& Value)
{
	if (UOnyxMovementComponent* Movement = GetOnyxMovement())
	{
		Movement->JumpReleased();
	}
}

void AOnyxPlayerCharacter::HandleDash(const FInputActionValue& Value)
{
	if (UOnyxMovementComponent* Movement = GetOnyxMovement())
	{
		const float Direction = FMath::Abs(MoveInput) > 0.1f ? MoveInput : Onyx2D::FacingSign(GetFacing());
		Movement->TryDash(Direction);
	}
}

void AOnyxPlayerCharacter::HandleAttack(const FInputActionValue& Value)
{
	Combat->RequestAttack();
}

void AOnyxPlayerCharacter::HandleDownPressed(const FInputActionValue& Value)
{
	bDownHeld = true;
	if (UOnyxMovementComponent* Movement = GetOnyxMovement())
	{
		Movement->SetCrouchHeld(true);
	}
}

void AOnyxPlayerCharacter::HandleDownReleased(const FInputActionValue& Value)
{
	bDownHeld = false;
	if (UOnyxMovementComponent* Movement = GetOnyxMovement())
	{
		Movement->SetCrouchHeld(false);
	}
}

void AOnyxPlayerCharacter::HandleInteract(const FInputActionValue& Value)
{
	OnInteract();
}

void AOnyxPlayerCharacter::HandleDeath(AActor* Killer)
{
	Super::HandleDeath(Killer);
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
	if (RespawnDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &AOnyxPlayerCharacter::Respawn, RespawnDelay, false);
	}
}

void AOnyxPlayerCharacter::Respawn()
{
	SetActorLocation(RespawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();
	Health->Revive();
	Health->SetInvulnerableFor(1.f);
	if (UOnyxMovementComponent* Movement = GetOnyxMovement())
	{
		Movement->SetMovementLocked(false);
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		EnableInput(PC);
	}
	Camera->SnapToTarget();
}
