// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OnyxUISettings.generated.h"

class UFont;
class UOnyxDialogueWidget;
class UOnyxHUDWidget;
class UOnyxMainMenu;
class UOnyxPauseMenu;
class USoundBase;
class UTexture2D;

/** Colors and fonts shared by every Onyx screen. */
USTRUCT(BlueprintType)
struct ONYX2DUI_API FOnyxUITheme
{
	GENERATED_BODY()

	FOnyxUITheme();

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor Accent;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor Danger;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor TextBright;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor Text;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor TextDim;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor Panel;

	/** Big logo font (title screen). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Fonts", meta = (AllowedClasses = "/Script/Engine.Font"))
	FSoftObjectPath TitleFont;

	/** Menu entries and headings. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Fonts", meta = (AllowedClasses = "/Script/Engine.Font"))
	FSoftObjectPath DisplayFont;

	/** Labels, hints, descriptions. Composite font with Regular/Bold/Light typefaces works best. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Fonts", meta = (AllowedClasses = "/Script/Engine.Font"))
	FSoftObjectPath BodyFont;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TSoftObjectPtr<USoundBase> NavigateSound;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TSoftObjectPtr<USoundBase> ConfirmSound;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0", ClampMax = "2"))
	float SoundVolume = 0.5f;
};

/** Project Settings > Plugins > Onyx2D UI. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Onyx2D UI"))
class ONYX2DUI_API UOnyxUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UOnyxUISettings* Get() { return GetDefault<UOnyxUISettings>(); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	UPROPERTY(Config, EditAnywhere, Category = "Theme")
	FOnyxUITheme Theme;

	UPROPERTY(Config, EditAnywhere, Category = "Game")
	FText GameTitle = INVTEXT("ONYX");

	UPROPERTY(Config, EditAnywhere, Category = "Game")
	FText GameSubtitle = INVTEXT("A 2D ADVENTURE");

	/** Level that shows the title screen. */
	UPROPERTY(Config, EditAnywhere, Category = "Game")
	TSoftObjectPtr<UWorld> MainMenuLevel;

	/** Level opened by Play. */
	UPROPERTY(Config, EditAnywhere, Category = "Game")
	TSoftObjectPtr<UWorld> FirstGameLevel;

	/** Show the title screen automatically in the main menu level. */
	UPROPERTY(Config, EditAnywhere, Category = "Behaviour")
	bool bAutoShowMainMenu = true;

	/** Show the HUD automatically in gameplay levels. */
	UPROPERTY(Config, EditAnywhere, Category = "Behaviour")
	bool bAutoShowHUD = true;

	/** Open the pause menu when the Onyx Pause input is pressed. */
	UPROPERTY(Config, EditAnywhere, Category = "Behaviour")
	bool bHandlePause = true;

	/** Swap in Blueprint subclasses to restyle or extend the screens. */
	UPROPERTY(Config, EditAnywhere, Category = "Screens")
	TSoftClassPtr<UOnyxMainMenu> MainMenuClass;

	UPROPERTY(Config, EditAnywhere, Category = "Screens")
	TSoftClassPtr<UOnyxPauseMenu> PauseMenuClass;

	UPROPERTY(Config, EditAnywhere, Category = "Screens")
	TSoftClassPtr<UOnyxHUDWidget> HUDClass;

	/** Messages and the conversation box (in every level but the main menu). */
	UPROPERTY(Config, EditAnywhere, Category = "Screens")
	TSoftClassPtr<UOnyxDialogueWidget> DialogueClass;

	/**
	 * The hotbar's look (Onyx Inventory): a slot, the selected slot, and the clock's panel (Onyx Calendar). Small
	 * pixel-art textures, drawn at Pixel Art Scale; without them the HUD draws plain panels.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Hotbar")
	TSoftObjectPtr<UTexture2D> SlotTexture;

	UPROPERTY(Config, EditAnywhere, Category = "Hotbar")
	TSoftObjectPtr<UTexture2D> SelectedSlotTexture;

	UPROPERTY(Config, EditAnywhere, Category = "Hotbar")
	TSoftObjectPtr<UTexture2D> ClockTexture;

	/** Screen pixels per art pixel for the hotbar, the clock and item icons. */
	UPROPERTY(Config, EditAnywhere, Category = "Hotbar", meta = (ClampMin = "1", ClampMax = "8"))
	int32 PixelArtScale = 4;
};
