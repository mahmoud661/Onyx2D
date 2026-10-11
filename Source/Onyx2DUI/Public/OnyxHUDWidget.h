// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OnyxHUDWidget.generated.h"

class UHorizontalBox;
class UImage;
class USizeBox;
class UTextBlock;

/** Gameplay HUD: health pips, dash cooldown and coin counter, read from the possessed pawn's Onyx components. */
UCLASS(Blueprintable)
class ONYX2DUI_API UOnyxHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildUI();
	void RebuildPips(int32 Count);

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> PipRow;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> PipFills;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> DashFill;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> DashBar;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> CoinPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CoinText;

	TArray<float> PipScale;
	float LastHealth = -1.f;
	float DashFlash = 0.f;
	float LastDashFraction = 0.f;
	int32 LastCoins = -1;
	float CoinPop = 1.f;
};
