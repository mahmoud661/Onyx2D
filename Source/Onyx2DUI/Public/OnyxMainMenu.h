// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnyxScreen.h"
#include "OnyxMainMenu.generated.h"

/**
 * Title screen: Play / Settings / Quit. Title, subtitle and levels come from Onyx2D UI settings.
 *
 * A Widget Blueprint child with no layout of its own gets the built-in one. With a layout of its own (the Designer),
 * its buttons call Start Game, Open Settings and Request Quit, and widgets named like the properties below (Title,
 * Subtitle, Divider, Footer, FadeOverlay) get the same fade-in; any of them can be left out.
 */
UCLASS(Blueprintable)
class ONYX2DUI_API UOnyxMainMenu : public UOnyxScreen
{
	GENERATED_BODY()

public:
	/** Fades out, then opens the First Game Level from the Onyx2D UI settings. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void StartGame();

	/** Asks "Quit?" and quits to the desktop (straight away with a layout of your own). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void RequestQuit();

protected:
	virtual void BuildUI() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	/** Esc asks to quit; with a layout of its own there is no confirm box to ask with, so Esc does nothing. */
	virtual void OnBackPressed() override;

	/** The game's name: fades and rises in, its glow pulses. */
	UPROPERTY(BlueprintReadOnly, Category = "Title Screen", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Title;

	/** The line above the name: fades in from above. */
	UPROPERTY(BlueprintReadOnly, Category = "Title Screen", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Subtitle;

	/** The line under the name: grows from its left edge (set its pivot to 0, 0.5). */
	UPROPERTY(BlueprintReadOnly, Category = "Title Screen", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Divider;

	/** The key hints and version at the bottom: fade in last. */
	UPROPERTY(BlueprintReadOnly, Category = "Title Screen", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Footer;

	/** A black image over everything: fades out when the screen opens and back in before the game level opens. */
	UPROPERTY(BlueprintReadOnly, Category = "Title Screen", meta = (BindWidgetOptional))
	TObjectPtr<UImage> FadeOverlay;

private:
	void TravelToGame();

	float LeaveTime = -1.f;
	bool bTravelled = false;
};
