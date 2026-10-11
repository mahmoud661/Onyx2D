// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnyxScreen.h"
#include "OnyxPauseMenu.generated.h"

class UBackgroundBlur;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnyxPauseClosedSignature);

/** Pause screen: Resume / Settings / Main Menu / Quit. Pauses the game while open. */
UCLASS(Blueprintable)
class ONYX2DUI_API UOnyxPauseMenu : public UOnyxScreen
{
	GENERATED_BODY()

public:
	UOnyxPauseMenu(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void Resume();

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|UI")
	FOnyxPauseClosedSignature OnResumed;

protected:
	virtual void BuildUI() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void OnBackPressed() override { Resume(); }

private:
	UPROPERTY(Transient)
	TObjectPtr<UBackgroundBlur> Blur;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> Backdrop;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Heading;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Divider;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> Footer;
};
