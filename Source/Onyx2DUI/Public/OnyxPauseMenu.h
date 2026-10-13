// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnyxScreen.h"
#include "OnyxPauseMenu.generated.h"

class UBackgroundBlur;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnyxPauseClosedSignature);

/**
 * Pause screen: Resume / Settings / Main Menu / Quit. Pauses the game while open.
 *
 * A Widget Blueprint child with no layout of its own gets the built-in one. With a layout of its own (the Designer),
 * its buttons call Resume, Open Settings, Return To Main Menu and Request Quit, and widgets named like the properties
 * below (Blur, Backdrop, Heading, Divider, Footer) get the same fade-in; any of them can be left out.
 */
UCLASS(Blueprintable)
class ONYX2DUI_API UOnyxPauseMenu : public UOnyxScreen
{
	GENERATED_BODY()

public:
	UOnyxPauseMenu(const FObjectInitializer& ObjectInitializer);

	/** Closes the pause screen and unpauses the game. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void Resume();

	/** Asks "Return to Main Menu?" and opens the Main Menu Level from the Onyx2D UI settings (straight away with a
	 *  layout of your own). Does nothing when no Main Menu Level is set. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void ReturnToMainMenu();

	/** Asks "Quit Game?" and quits to the desktop (straight away with a layout of your own). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void RequestQuit();

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|UI")
	FOnyxPauseClosedSignature OnResumed;

protected:
	virtual void BuildUI() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void OnBackPressed() override { Resume(); }

	/** Blurs the paused game behind the menu: its strength rises from 0 to 7. */
	UPROPERTY(BlueprintReadOnly, Category = "Pause Menu", meta = (BindWidgetOptional))
	TObjectPtr<UBackgroundBlur> Blur;

	/** The darkening over the game: fades in. */
	UPROPERTY(BlueprintReadOnly, Category = "Pause Menu", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Backdrop;

	/** The "Paused" heading: fades and rises in. */
	UPROPERTY(BlueprintReadOnly, Category = "Pause Menu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Heading;

	/** The line under the heading: grows from its left edge (set its pivot to 0, 0.5). */
	UPROPERTY(BlueprintReadOnly, Category = "Pause Menu", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Divider;

	/** The key hints at the bottom: fade in last. */
	UPROPERTY(BlueprintReadOnly, Category = "Pause Menu", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Footer;
};
