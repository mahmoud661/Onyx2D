// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "OnyxButton.generated.h"

/** Transparent button with native callbacks; hovering gives it keyboard focus so mouse and pad share one selection. */
UCLASS()
class ONYX2DUI_API UOnyxButton : public UButton
{
	GENERATED_BODY()

public:
	TFunction<void()> OnClickedFn;
	TFunction<void()> OnHoveredFn;
	bool bFocusOnHover = true;

	/** Must be called before the Slate widget is built. */
	void InitFocusable(bool bInFocusable) { InitIsFocusable(bInFocusable); }

	/** Hooks the dynamic delegates up to the native callbacks. Call once after construction. */
	void BindNative();

private:
	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();
};
