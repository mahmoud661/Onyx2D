// Copyright Onyx2D. All Rights Reserved.

#include "OnyxUISubsystem.h"
#include "Core/Onyx2DTypes.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "OnyxHUDWidget.h"
#include "OnyxMainMenu.h"
#include "OnyxPauseMenu.h"
#include "OnyxUISettings.h"
#include "TimerManager.h"

namespace
{
	template <class TWidget>
	TSubclassOf<TWidget> ResolveClass(const TSoftClassPtr<TWidget>& Configured)
	{
		UClass* Loaded = Configured.LoadSynchronous();
		return Loaded ? TSubclassOf<TWidget>(Loaded) : TSubclassOf<TWidget>(TWidget::StaticClass());
	}
}

void UOnyxUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PauseHandle = FOnyxGameDelegates::OnPauseRequested.AddUObject(this, &UOnyxUISubsystem::HandlePauseRequested);
}

void UOnyxUISubsystem::Deinitialize()
{
	FOnyxGameDelegates::OnPauseRequested.Remove(PauseHandle);
	Super::Deinitialize();
}

APlayerController* UOnyxUISubsystem::GetPlayerController() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()) : nullptr;
}

void UOnyxUISubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	HUD = nullptr;
	PauseMenu = nullptr;
	MainMenu = nullptr;

	UWorld* World = NewPlayerController ? NewPlayerController->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	// Wait a tick so the pawn is possessed and the viewport is ready.
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UOnyxUISubsystem::SetupForCurrentLevel));
}

bool UOnyxUISubsystem::IsInMainMenuLevel() const
{
	const APlayerController* PC = GetPlayerController();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const TSoftObjectPtr<UWorld>& MenuLevel = UOnyxUISettings::Get()->MainMenuLevel;
	if (!World || MenuLevel.IsNull())
	{
		return false;
	}
	return UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == MenuLevel.ToSoftObjectPath().GetLongPackageName();
}

void UOnyxUISubsystem::SetupForCurrentLevel()
{
	const UOnyxUISettings* Settings = UOnyxUISettings::Get();
	if (IsInMainMenuLevel())
	{
		if (Settings->bAutoShowMainMenu)
		{
			ShowMainMenu();
		}
	}
	else if (Settings->bAutoShowHUD)
	{
		ShowHUD();
	}
}

void UOnyxUISubsystem::ShowMainMenu()
{
	APlayerController* PC = GetPlayerController();
	if (!PC || (MainMenu && MainMenu->IsInViewport()))
	{
		return;
	}
	HideHUD();
	MainMenu = CreateWidget<UOnyxMainMenu>(PC, ResolveClass(UOnyxUISettings::Get()->MainMenuClass));
	if (MainMenu)
	{
		MainMenu->AddToViewport(10);
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(MainMenu->TakeWidget());
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}
}

void UOnyxUISubsystem::ShowHUD()
{
	APlayerController* PC = GetPlayerController();
	if (!PC || (HUD && HUD->IsInViewport()))
	{
		return;
	}
	HUD = CreateWidget<UOnyxHUDWidget>(PC, ResolveClass(UOnyxUISettings::Get()->HUDClass));
	if (HUD)
	{
		HUD->AddToViewport(0);
	}
}

void UOnyxUISubsystem::HideHUD()
{
	if (HUD)
	{
		HUD->RemoveFromParent();
		HUD = nullptr;
	}
}

bool UOnyxUISubsystem::IsPauseMenuOpen() const
{
	return PauseMenu && PauseMenu->IsInViewport();
}

void UOnyxUISubsystem::HandlePauseRequested(APlayerController* Player)
{
	if (Player == GetPlayerController() && UOnyxUISettings::Get()->bHandlePause)
	{
		TogglePauseMenu();
	}
}

void UOnyxUISubsystem::TogglePauseMenu()
{
	if (IsPauseMenuOpen())
	{
		PauseMenu->Resume();
		return;
	}

	APlayerController* PC = GetPlayerController();
	if (!PC || IsInMainMenuLevel())
	{
		return;
	}
	PauseMenu = CreateWidget<UOnyxPauseMenu>(PC, ResolveClass(UOnyxUISettings::Get()->PauseMenuClass));
	if (!PauseMenu)
	{
		return;
	}
	PauseMenu->AddToViewport(50);
	UGameplayStatics::SetGamePaused(PC, true);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(PauseMenu->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
}
