// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OnyxHUDWidget.generated.h"

class UCanvasPanelSlot;
class UHorizontalBox;
class UImage;
class USizeBox;
class UTextBlock;

/**
 * Gameplay HUD: health pips, dash cooldown, coin counter and held items, read from the possessed pawn's Onyx components,
 * plus the Interact prompt ("E  Read") over the player when something usable is in reach.
 *
 * A Widget Blueprint child with no layout of its own gets the built-in one. With a layout of its own (the Designer),
 * name its widgets like the properties below (PipRow, DashBar, DashFill, CoinPanel, CoinText, ItemsText, Prompt,
 * PromptKey, PromptVerb) and they are filled in the same way; any of them can be left out.
 */
UCLASS(Blueprintable)
class ONYX2DUI_API UOnyxHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Gets one diamond pip per point of the player's Max Health; a pip empties when that point is lost. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> PipRow;

	/** The dash meter: shown only when the player can dash, brighter when the dash is ready. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DashBar;

	/** Inside the dash meter, aligned left: its width follows the cooldown, up to the meter's width. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> DashFill;

	/** Shown from the first coin; pops when a coin is picked up. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CoinPanel;

	/** The number of coins. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CoinText;

	/** The held items (keys...), one per line, hidden while there are none. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemsText;

	/** Shown while something usable is in reach. Inside a Canvas Panel it follows the player's head; anywhere else it
	 *  stays where it is placed. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Prompt;

	/** The key to press for Interact, such as "E". */
	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptKey;

	/** What Interact does, such as "Read" or "Open". */
	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptVerb;

private:
	void BuildUI();
	void RebuildPips(int32 Count);
	void UpdatePrompt(float DeltaTime);
	void UpdateItems();
	/** The key the player presses to Interact, as shown on the prompt. */
	FText GetInteractKeyName() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> PipFills;

	/** The prompt's slot when it is in a Canvas Panel (moved to follow the player). */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> PromptSlot;

	TArray<float> PipScale;
	float PromptShown = 0.f;
	float KeyNameAge = 10.f;
	FString ShownItems;
	float LastHealth = -1.f;
	float DashFlash = 0.f;
	float LastDashFraction = 0.f;
	int32 LastCoins = -1;
	float CoinPop = 1.f;
};
