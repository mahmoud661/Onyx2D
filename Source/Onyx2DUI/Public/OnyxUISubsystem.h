// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "OnyxUISubsystem.generated.h"

class APlayerController;
class UOnyxHUDWidget;
class UOnyxMainMenu;
class UOnyxPauseMenu;

/**
 * Shows the right screen for each level with zero setup: title screen in the main menu level,
 * HUD elsewhere, and the pause menu when the Onyx Pause input fires.
 */
UCLASS()
class ONYX2DUI_API UOnyxUISubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void ShowMainMenu();

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void ShowHUD();

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void HideHUD();

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void TogglePauseMenu();

	UFUNCTION(BlueprintPure, Category = "Onyx2D|UI")
	bool IsPauseMenuOpen() const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|UI")
	bool IsInMainMenuLevel() const;

private:
	void SetupForCurrentLevel();
	void HandlePauseRequested(APlayerController* Player);
	APlayerController* GetPlayerController() const;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxHUDWidget> HUD;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxPauseMenu> PauseMenu;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxMainMenu> MainMenu;

	FDelegateHandle PauseHandle;
};
