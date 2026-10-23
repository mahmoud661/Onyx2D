// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OnyxHUDWidget.generated.h"

class UCanvasPanelSlot;
class UOverlay;
class UOverlaySlot;
class UHorizontalBox;
class UImage;
class USizeBox;
class UTextBlock;
class UTexture2D;

/**
 * Gameplay HUD: health pips, energy cells, dash cooldown, coin counter and held items, read from the possessed pawn's Onyx
 * components, plus the Interact prompt ("E  Read") over the player when something usable is in reach. During a boss
 * fight (an Onyx Boss component) its title card shows as it begins and its health bar along the bottom.
 *
 * A Widget Blueprint child with no layout of its own gets the built-in one. With a layout of its own (the Designer),
 * name its widgets like the properties below (PipRow, EnergyRow, DashBar, DashFill, CoinPanel, CoinText, ItemsText,
 * Prompt, PromptKey, PromptVerb) and they are filled in the same way; any of them can be left out.
 */
UCLASS(Blueprintable)
class ONYX2DUI_API UOnyxHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	/** A picture for each health pip (a heart, a drop...) instead of the diamond. A lost pip shows it dimmed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Look")
	TObjectPtr<UTexture2D> PipTexture;

	/** How big the pip picture is on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Look")
	FVector2D PipTextureSize = FVector2D(24.f, 30.f);

	/** Colour of a lost pip's picture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Look")
	FLinearColor EmptyPipColor = FLinearColor(0.1f, 0.12f, 0.18f, 0.6f);

	/** Colour of the energy in the energy cells (one cell per mend). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Look")
	FLinearColor EnergyColor = FLinearColor(0.72f, 0.95f, 1.f);

	/** Colour of an energy cell's empty part. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Look")
	FLinearColor EmptyEnergyColor = FLinearColor(0.08f, 0.12f, 0.18f, 0.7f);

	/** Size of one energy cell on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Look")
	FVector2D EnergyCellSize = FVector2D(40.f, 9.f);

	/** Colour of a boss's health on its bar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Look")
	FLinearColor BossBarColor = FLinearColor(0.78f, 0.16f, 0.2f);

	/** Size of a boss's health bar on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Look")
	FVector2D BossBarSize = FVector2D(640.f, 10.f);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Gets one diamond pip per point of the player's Max Health; a pip empties when that point is lost. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> PipRow;

	/**
	 * Gets one cell per mend the player's Onyx Energy can hold (Max Energy / Mend Cost), filling as blows land. A full
	 * cell glows: a mend is ready. Hidden without Onyx Energy.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> EnergyRow;

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
	void RebuildEnergyCells(int32 Count);
	void UpdateEnergy(float DeltaTime);
	/** The boss bar and title card, made the first time a boss fight starts, in whatever layout this HUD has. */
	void EnsureBossUI();
	void UpdateBoss(float DeltaTime);
	void UpdatePrompt(float DeltaTime);
	void UpdateItems();
	/** The hotbar (bottom middle) for a player with an Onyx Inventory, and the clock (top right) with the Onyx Calendar on. */
	void UpdateHotbar();
	void RebuildHotbar(int32 Count);
	/** Builds the hotbar and the clock into a layer, and lays such a layer over a HUD with a layout of its own. */
	void BuildFarmUI(UOverlay* Layer);
	void EnsureFarmUI();
	void UpdateClock();
	/** The key the player presses to Interact, as shown on the prompt. */
	FText GetInteractKeyName() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> PipFills;

	/** The prompt's slot when it is in a Canvas Panel (moved to follow the player). */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> PromptSlot;

	TArray<float> PipScale;

	/** Inside each energy cell, aligned left: its width is how full that cell is. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> EnergyFills;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> EnergyFillImages;

	TArray<float> EnergyGlow;
	float LastEnergy = -1.f;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> BossPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossNameText;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> BossFill;

	/** The health just lost, lingering white behind the bar before it drains away. */
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> BossTrail;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> BossCard;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossCardTitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossCardName;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> HotbarRow;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> HotbarBacks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> HotbarIcons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> HotbarCounts;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> ClockPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClockDate;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClockTime;

	/** The coin counter's slot: it moves down under the clock when the clock shows. */
	UPROPERTY(Transient)
	TObjectPtr<UOverlaySlot> CoinOverlaySlot;

	float BossShown = 0.f;
	float BossTrailFraction = 1.f;
	float BossTrailHold = 0.f;
	float LastBossFraction = 1.f;
	float PromptShown = 0.f;
	float KeyNameAge = 10.f;
	FString ShownItems;
	float LastHealth = -1.f;
	float DashFlash = 0.f;
	float LastDashFraction = 0.f;
	int32 LastCoins = -1;
	float CoinPop = 1.f;
};
