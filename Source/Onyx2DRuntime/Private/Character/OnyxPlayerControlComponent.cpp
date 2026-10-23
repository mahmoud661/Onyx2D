// Copyright Onyx2D. All Rights Reserved.

#include "Character/OnyxPlayerControlComponent.h"
#include "Animation/OnyxMovesetComponent.h"
#include "Character/OnyxCharacter.h"
#include "Combat/OnyxCombatComponent.h"
#include "Combat/OnyxEnergyComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "Combat/OnyxRangedComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/OnyxInputConfig.h"
#include "Logic/OnyxDialogueSubsystem.h"
#include "Movement/OnyxMovementComponent.h"
#include "Items/OnyxInventoryComponent.h"
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
	// A character seen from above: W and Up walk up instead of jumping.
	const UOnyxMovementComponent* Movement = GetOwner()->FindComponentByClass<UOnyxMovementComponent>();
	InputSubsystem->SetTopDown(Movement && Movement->GetActiveGameStyle() == EOnyxGameStyle::TopDown); // whether or not it has started moving that way

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
	if (Config->Up)
	{
		InputComponent->BindAction(Config->Up, ETriggerEvent::Started, this, &UOnyxPlayerControlComponent::HandleUpPressed);
		InputComponent->BindAction(Config->Up, ETriggerEvent::Completed, this, &UOnyxPlayerControlComponent::HandleUpReleased);
	}
	if (Config->Mend)
	{
		InputComponent->BindAction(Config->Mend, ETriggerEvent::Started, this, &UOnyxPlayerControlComponent::HandleMendPressed);
		InputComponent->BindAction(Config->Mend, ETriggerEvent::Completed, this, &UOnyxPlayerControlComponent::HandleMendReleased);
	}
	if (Config->Shoot)
	{
		InputComponent->BindAction(Config->Shoot, ETriggerEvent::Started, this, &UOnyxPlayerControlComponent::HandleShootPressed);
		InputComponent->BindAction(Config->Shoot, ETriggerEvent::Completed, this, &UOnyxPlayerControlComponent::HandleShootReleased);
	}
	if (Config->Throw)
	{
		InputComponent->BindAction(Config->Throw, ETriggerEvent::Started, this, &UOnyxPlayerControlComponent::HandleThrowPressed);
		InputComponent->BindAction(Config->Throw, ETriggerEvent::Completed, this, &UOnyxPlayerControlComponent::HandleThrowReleased);
	}
	if (Config->Aim)
	{
		InputComponent->BindAction(Config->Aim, ETriggerEvent::Triggered, this, &UOnyxPlayerControlComponent::HandleAim);
		InputComponent->BindAction(Config->Aim, ETriggerEvent::Completed, this, &UOnyxPlayerControlComponent::HandleAim);
	}
	if (Config->HotbarNext)
	{
		InputComponent->BindAction(Config->HotbarNext, ETriggerEvent::Started, this, &UOnyxPlayerControlComponent::HandleHotbarNext);
	}
	if (Config->HotbarPrevious)
	{
		InputComponent->BindAction(Config->HotbarPrevious, ETriggerEvent::Started, this, &UOnyxPlayerControlComponent::HandleHotbarPrevious);
	}
	if (Config->HotbarSlot)
	{
		InputComponent->BindAction(Config->HotbarSlot, ETriggerEvent::Started, this, &UOnyxPlayerControlComponent::HandleHotbarSlot);
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
	const AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	const UOnyxEnergyComponent* Energy = GetOwner()->FindComponentByClass<UOnyxEnergyComponent>();
	// Thrown back by a hit, or kneeling to mend (letting go of Mend is the way out): nothing else until then.
	return bControlEnabled && !(Health && Health->IsDead()) && !(Dialogue && Dialogue->IsTalking()) && !(Character && Character->IsHurtStunned())
		&& !(Energy && Energy->IsMending());
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
	if (Movement)
	{
		Movement->SetUpHeld(false);
	}
	if (UOnyxEnergyComponent* Energy = GetOwner() ? GetOwner()->FindComponentByClass<UOnyxEnergyComponent>() : nullptr)
	{
		Energy->StopMend();
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
	if (UOnyxRangedComponent* Ranged = GetOwner()->FindComponentByClass<UOnyxRangedComponent>())
	{
		Ranged->Cancel(); // a dash breaks off a draw
	}
	UOnyxMovementComponent* Movement = GetOwner()->FindComponentByClass<UOnyxMovementComponent>();
	if (!Movement || !CanAct())
	{
		return;
	}
	// Dash where the stick points, else where the character faces (top down: any way it is steered, else where it looks).
	const AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	if (Movement->IsTopDown())
	{
		const FVector2D Steering = Movement->GetTopDownSteering();
		Movement->TryDashToward(!Steering.IsNearlyZero(0.1f) ? Steering : Character ? Character->GetAimVector() : FVector2D(1.f, 0.f));
		return;
	}
	const float Facing = Character ? Onyx2D::FacingSign(Character->GetFacing()) : 1.f;
	Movement->TryDash(FMath::Abs(MoveInput) > 0.1f ? MoveInput : Facing);
}

void UOnyxPlayerControlComponent::HandleAttack(const FInputActionValue& Value)
{
	if (!CanAct())
	{
		return;
	}
	// Drawing a bow or throwing: the blade waits.
	if (const UOnyxRangedComponent* Ranged = GetOwner()->FindComponentByClass<UOnyxRangedComponent>(); Ranged && Ranged->IsBusy())
	{
		return;
	}
	// Holding a tool or seeds (Onyx Inventory): Attack uses it instead of swinging.
	if (UOnyxInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UOnyxInventoryComponent>(); Inventory && Inventory->UseSelectedItem())
	{
		return;
	}
	if (UOnyxCombatComponent* Combat = GetOwner()->FindComponentByClass<UOnyxCombatComponent>())
	{
		Combat->RequestAttack();
	}
}

void UOnyxPlayerControlComponent::HandleShootPressed(const FInputActionValue& Value)
{
	UOnyxRangedComponent* Ranged = GetOwner()->FindComponentByClass<UOnyxRangedComponent>();
	if (Ranged && CanAct())
	{
		Ranged->Press(EOnyxRangedSlot::Shoot);
	}
}

void UOnyxPlayerControlComponent::HandleShootReleased(const FInputActionValue& Value)
{
	if (UOnyxRangedComponent* Ranged = GetOwner()->FindComponentByClass<UOnyxRangedComponent>())
	{
		Ranged->Release(EOnyxRangedSlot::Shoot);
	}
}

void UOnyxPlayerControlComponent::HandleThrowPressed(const FInputActionValue& Value)
{
	UOnyxRangedComponent* Ranged = GetOwner()->FindComponentByClass<UOnyxRangedComponent>();
	if (Ranged && CanAct())
	{
		Ranged->Press(EOnyxRangedSlot::Throw);
	}
}

void UOnyxPlayerControlComponent::HandleThrowReleased(const FInputActionValue& Value)
{
	if (UOnyxRangedComponent* Ranged = GetOwner()->FindComponentByClass<UOnyxRangedComponent>())
	{
		Ranged->Release(EOnyxRangedSlot::Throw);
	}
}

void UOnyxPlayerControlComponent::HandleAim(const FInputActionValue& Value)
{
	if (UOnyxRangedComponent* Ranged = GetOwner()->FindComponentByClass<UOnyxRangedComponent>())
	{
		Ranged->SetStickAim(Value.Get<FVector2D>());
	}
}

void UOnyxPlayerControlComponent::HandleHotbarNext(const FInputActionValue& Value)
{
	if (UOnyxInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UOnyxInventoryComponent>())
	{
		Inventory->SelectNext(1);
	}
}

void UOnyxPlayerControlComponent::HandleHotbarPrevious(const FInputActionValue& Value)
{
	if (UOnyxInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UOnyxInventoryComponent>())
	{
		Inventory->SelectNext(-1);
	}
}

void UOnyxPlayerControlComponent::HandleHotbarSlot(const FInputActionValue& Value)
{
	if (UOnyxInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UOnyxInventoryComponent>())
	{
		Inventory->SelectSlot(FMath::RoundToInt(Value.Get<float>()) - 1);
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

void UOnyxPlayerControlComponent::HandleUpPressed(const FInputActionValue& Value)
{
	if (UOnyxMovementComponent* Movement = GetOwner()->FindComponentByClass<UOnyxMovementComponent>())
	{
		Movement->SetUpHeld(true); // held through a hit or a mend too: what counts is the key
	}
}

void UOnyxPlayerControlComponent::HandleUpReleased(const FInputActionValue& Value)
{
	if (UOnyxMovementComponent* Movement = GetOwner()->FindComponentByClass<UOnyxMovementComponent>())
	{
		Movement->SetUpHeld(false);
	}
}

void UOnyxPlayerControlComponent::HandleMendPressed(const FInputActionValue& Value)
{
	UOnyxEnergyComponent* Energy = GetOwner()->FindComponentByClass<UOnyxEnergyComponent>();
	if (Energy && CanAct())
	{
		Energy->StartMend();
	}
}

void UOnyxPlayerControlComponent::HandleMendReleased(const FInputActionValue& Value)
{
	if (UOnyxEnergyComponent* Energy = GetOwner()->FindComponentByClass<UOnyxEnergyComponent>())
	{
		Energy->StopMend();
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
	Interact();
}

void UOnyxPlayerControlComponent::Interact()
{
	if (AdvanceDialogue() || !CanAct())
	{
		return;
	}
	if (UObject* Target = FindInteractable())
	{
		IOnyxInteractable::Execute_Interact(Target, GetOwner());
		// A hand goes out to it (unless using it started something else on the character, a sleep, a scene).
		AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
		if (!InteractClip.IsNone() && Character && Character->MovesetComponent && !Character->MovesetComponent->IsPlayingOneShot()
			&& Character->MovesetComponent->HasClip(InteractClip))
		{
			Character->MovesetComponent->PlayOneShot(InteractClip);
		}
		return;
	}
	OnInteract.Broadcast();
}
