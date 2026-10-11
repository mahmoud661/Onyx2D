// Copyright Onyx2D. All Rights Reserved.

#include "Core/OnyxPlayerController.h"
#include "Core/Onyx2DTypes.h"
#include "EnhancedInputComponent.h"
#include "Input/OnyxInputConfig.h"
#include "InputAction.h"

void AOnyxPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (UOnyxInputSubsystem* InputSubsystem = UOnyxInputSubsystem::Get(this))
	{
		InputSubsystem->SetInputConfig(InputConfig);
	}
}

void AOnyxPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UOnyxInputSubsystem* InputSubsystem = UOnyxInputSubsystem::Get(this);
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!InputSubsystem || !Input)
	{
		return;
	}
	if (InputConfig)
	{
		InputSubsystem->SetInputConfig(InputConfig);
	}
	if (UInputAction* PauseAction = InputSubsystem->GetInputConfig()->Pause)
	{
		PauseAction->bTriggerWhenPaused = true;
		Input->BindAction(PauseAction, ETriggerEvent::Started, this, &AOnyxPlayerController::HandlePause);
	}
}

void AOnyxPlayerController::HandlePause()
{
	FOnyxGameDelegates::OnPauseRequested.Broadcast(this);
}
