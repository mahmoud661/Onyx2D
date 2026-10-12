// Copyright Onyx2D. All Rights Reserved.

#include "Character/OnyxPlayerControlComponent.h"
#include "Character/OnyxCharacter.h"
#include "Combat/OnyxCombatComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/OnyxInputConfig.h"
#include "Movement/OnyxMovementComponent.h"
#include "World/OnyxInteractable.h"

UOnyxPlayerControlComponent::UOnyxPlayerControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOnyxPlayerControlComponent::BeginPlay()
{
	Super::BeginPlay();
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &UOnyxPlayerControlComponent::HandleControllerChanged);
		BindTo(Pawn->GetController());
	}
}

void UOnyxPlayerControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Unbind();
	Super::EndPlay(EndPlayReason);
}

void UOnyxPlayerControlComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	Unbind();
	BindTo(NewController);
}

void UOnyxPlayerControlComponent::BindTo(AController* Controller)
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}
	UOnyxInputSubsystem* InputSubsystem = UOnyxInputSubsystem::Get(PlayerController);
	const UOnyxInputConfig* Config = InputSubsystem ? InputSubsystem->GetInputConfig() : nullptr;
	if (!Config)
	{
		return;
	}
	InputSubsystem->ApplyMappingContext();

	// Our own input component on the controller's stack: the character needs no input code of its own.
	InputComponent = NewObject<UEnhancedInputComponent>(GetOwner(), TEXT("OnyxPlayerControlInput"));
	InputComponent->RegisterComponent();
	if (Config->Move)
	{
		InputComponent->BindAction(Config->Move, ETriggerEvent::Triggered, this, &UOnyxPlayerControlComponent::HandleMove);
		InputComponent->BindAction(Config->Move, ETriggerEvent::Completed, this, &UOnyxPlayerControlComponent::HandleMoveReleased);
	}
	if (Config->Jump)
	{
		InputComponent->BindAction(Config->Jump, ETriggerEvent::Started, this, &UOnyxPlayerControlComponent::HandleJumpPressed);
		InputComponent->BindAction(Config->Jump, ETriggerEvent::Completed, this, &UOnyxPlayerControlComponent::HandleJumpReleased);
	}
	if (Config->Dash)
	{
		InputComponent->BindAction(Config->Dash, ETriggerEvent::Started, this, &UOnyxPlayerControlComponent::HandleDash);
	}
	if (Config->Attack)
	{
		InputComponent->BindAction(Config->Attack, ETriggerEvent::Started, this, &UOnyxPlayerControlComponent::HandleAttack);
	}
	if (Config->Down)
	{
		InputComponent->BindAction(Config->Down, ETriggerEvent::Started, this, &UOnyxPlayerControlComponent::HandleDownPressed);
		InputComponent->BindAction(Config->Down, ETriggerEvent::Completed, this, &UOnyxPlayerControlComponent::HandleDownReleased);
	}
	if (Config->Interact)
	{
		InputComponent->BindAction(Config->Interact, ETriggerEvent::Started, this, &UOnyxPlayerControlComponent::HandleInteract);
	}
	PlayerController->PushInputComponent(InputComponent);
	BoundController = PlayerController;
}

void UOnyxPlayerControlComponent::Unbind()
{
	if (InputComponent)
	{
		if (APlayerController* PlayerController = BoundController.Get())
		{
			PlayerController->PopInputComponent(InputComponent);
		}
		InputComponent->DestroyComponent();
		InputComponent = nullptr;
	}
	BoundController.Reset();
	ReleaseHeldInput();
}

bool UOnyxPlayerControlComponent::CanAct() const
{
	const UOnyxHealthComponent* Health = GetOwner()->FindComponentByClass<UOnyxHealthComponent>();
	return bControlEnabled && !(Health && Health->IsDead());
}

void UOnyxPlayerControlComponent::ReleaseHeldInput()
{
	MoveInput = 0.f;
	if (bDownHeld)
	{
		bDownHeld = false;
		if (UOnyxMovementComponent* Movement = GetOwner() ? GetOwner()->FindComponentByClass<UOnyxMovementComponent>() : nullptr)
		{
			Movement->SetCrouchHeld(false);
		}
	}
}

void UOnyxPlayerControlComponent::HandleMove(const FInputActionValue& Value)
{
	MoveInput = CanAct() ? Value.Get<float>() : 0.f;
	if (APawn* Pawn = Cast<APawn>(GetOwner()); Pawn && MoveInput != 0.f)
	{
		Pawn->AddMovementInput(FVector::ForwardVector, MoveInput);
	}
}

void UOnyxPlayerControlComponent::HandleMoveReleased(const FInputActionValue& Value)
{
	MoveInput = 0.f;
}

void UOnyxPlayerControlComponent::HandleJumpPressed(const FInputActionValue& Value)
{
	UOnyxMovementComponent* Movement = GetOwner()->FindComponentByClass<UOnyxMovementComponent>();
	if (!Movement || !CanAct() || (bDownHeld && Movement->TryDropThroughPlatform()))
	{
		return;
	}
	Movement->JumpPressed();
}

void UOnyxPlayerControlComponent::HandleJumpReleased(const FInputActionValue& Value)
{
	if (UOnyxMovementComponent* Movement = GetOwner()->FindComponentByClass<UOnyxMovementComponent>())
	{
		Movement->JumpReleased();
	}
}

void UOnyxPlayerControlComponent::HandleDash(const FInputActionValue& Value)
{
	UOnyxMovementComponent* Movement = GetOwner()->FindComponentByClass<UOnyxMovementComponent>();
	if (!Movement || !CanAct())
	{
		return;
	}
	// Dash where the stick points, else where the character faces.
	const AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	const float Facing = Character ? Onyx2D::FacingSign(Character->GetFacing()) : 1.f;
	Movement->TryDash(FMath::Abs(MoveInput) > 0.1f ? MoveInput : Facing);
}

void UOnyxPlayerControlComponent::HandleAttack(const FInputActionValue& Value)
{
	if (UOnyxCombatComponent* Combat = GetOwner()->FindComponentByClass<UOnyxCombatComponent>(); Combat && CanAct())
	{
		Combat->RequestAttack();
	}
}

void UOnyxPlayerControlComponent::HandleDownPressed(const FInputActionValue& Value)
{
	bDownHeld = true;
	if (UOnyxMovementComponent* Movement = GetOwner()->FindComponentByClass<UOnyxMovementComponent>(); Movement && CanAct())
	{
		Movement->SetCrouchHeld(true);
	}
}

void UOnyxPlayerControlComponent::HandleDownReleased(const FInputActionValue& Value)
{
	bDownHeld = false;
	if (UOnyxMovementComponent* Movement = GetOwner()->FindComponentByClass<UOnyxMovementComponent>())
	{
		Movement->SetCrouchHeld(false);
	}
}

AActor* UOnyxPlayerControlComponent::FindInteractable() const
{
	AActor* Owner = GetOwner();
	TArray<AActor*> Touching;
	Owner->GetOverlappingActors(Touching);
	AActor* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (AActor* Candidate : Touching)
	{
		if (Candidate && Candidate->Implements<UOnyxInteractable>() && !IOnyxInteractable::Execute_IsInteractionLocked(Candidate, Owner))
		{
			const double Distance = FVector::DistSquared(Candidate->GetActorLocation(), Owner->GetActorLocation());
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Candidate;
			}
		}
	}
	return Best;
}

void UOnyxPlayerControlComponent::HandleInteract(const FInputActionValue& Value)
{
	if (!CanAct())
	{
		return;
	}
	if (AActor* Target = FindInteractable())
	{
		IOnyxInteractable::Execute_Interact(Target, GetOwner());
		return;
	}
	OnInteract.Broadcast();
}
