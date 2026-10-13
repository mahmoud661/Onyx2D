// Copyright Onyx2D. All Rights Reserved.

#include "Character/OnyxPlayerControlComponent.h"
#include "Character/OnyxCharacter.h"
#include "Combat/OnyxCombatComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/OnyxInputConfig.h"
#include "Logic/OnyxDialogueSubsystem.h"
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
	if (Config->Vertical)
	{
		InputComponent->BindAction(Config->Vertical, ETriggerEvent::Triggered, this, &UOnyxPlayerControlComponent::HandleVertical);
		InputComponent->BindAction(Config->Vertical, ETriggerEvent::Completed, this, &UOnyxPlayerControlComponent::HandleVertical);
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
	const UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(this);
	return bControlEnabled && !(Health && Health->IsDead()) && !(Dialogue && Dialogue->IsTalking());
}

bool UOnyxPlayerControlComponent::AdvanceDialogue() const
{
	UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(this);
	if (!Dialogue || !Dialogue->IsTalking())
	{
		return false;
	}
	Dialogue->Advance();
	return true;
}

void UOnyxPlayerControlComponent::ReleaseHeldInput()
{
	MoveInput = 0.f;
	UOnyxMovementComponent* Movement = GetOwner() ? GetOwner()->FindComponentByClass<UOnyxMovementComponent>() : nullptr;
	if (Movement)
	{
		Movement->SetVerticalInput(0.f);
	}
	if (bDownHeld)
	{
		bDownHeld = false;
		if (Movement)
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

void UOnyxPlayerControlComponent::HandleVertical(const FInputActionValue& Value)
{
	// Completed events carry zero: the stick is back in the middle.
	if (UOnyxMovementComponent* Movement = GetOwner()->FindComponentByClass<UOnyxMovementComponent>())
	{
		Movement->SetVerticalInput(CanAct() ? Value.Get<float>() : 0.f);
	}
}

void UOnyxPlayerControlComponent::HandleJumpPressed(const FInputActionValue& Value)
{
	if (AdvanceDialogue())
	{
		return;
	}
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

UObject* UOnyxPlayerControlComponent::FindInteractable() const
{
	AActor* Owner = GetOwner();
	TArray<UPrimitiveComponent*> Touching;
	Owner->GetOverlappingComponents(Touching);
	UObject* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (UPrimitiveComponent* Component : Touching)
	{
		AActor* Other = Component ? Component->GetOwner() : nullptr;
		if (!Other || Other == Owner)
		{
			continue;
		}
		// The component itself (an NPC's Talk range), else its actor (a lever, a sign, a door).
		UObject* Candidate = Component->Implements<UOnyxInteractable>() ? static_cast<UObject*>(Component) : static_cast<UObject*>(Other);
		if (!Candidate->Implements<UOnyxInteractable>() || IOnyxInteractable::Execute_IsInteractionLocked(Candidate, Owner))
		{
			continue;
		}
		const double Distance = FVector::DistSquared(Component->GetComponentLocation(), Owner->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Candidate;
		}
	}
	return Best;
}

void UOnyxPlayerControlComponent::HandleInteract(const FInputActionValue& Value)
{
	if (AdvanceDialogue() || !CanAct())
	{
		return;
	}
	if (UObject* Target = FindInteractable())
	{
		IOnyxInteractable::Execute_Interact(Target, GetOwner());
		return;
	}
	OnInteract.Broadcast();
}
