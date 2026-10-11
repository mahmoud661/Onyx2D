// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnyxScreen.h"
#include "OnyxMainMenu.generated.h"

/** Title screen: Play / Settings / Quit. Title, subtitle and levels come from Onyx2D UI settings. */
UCLASS(Blueprintable)
class ONYX2DUI_API UOnyxMainMenu : public UOnyxScreen
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void StartGame();

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void RequestQuit();

protected:
	virtual void BuildUI() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void OnBackPressed() override { RequestQuit(); }

private:
	void TravelToGame();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Title;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Subtitle;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Divider;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> Footer;

	UPROPERTY(Transient)
	TObjectPtr<UImage> FadeOverlay;

	float LeaveTime = -1.f;
	bool bTravelled = false;
};
