// Copyright Onyx2D. All Rights Reserved.

#include "OnyxButton.h"

void UOnyxButton::BindNative()
{
	OnClicked.AddUniqueDynamic(this, &UOnyxButton::HandleClicked);
	OnHovered.AddUniqueDynamic(this, &UOnyxButton::HandleHovered);
}

void UOnyxButton::HandleClicked()
{
	if (OnClickedFn)
	{
		OnClickedFn();
	}
}

void UOnyxButton::HandleHovered()
{
	if (bFocusOnHover && GetIsFocusable() && GetIsEnabled())
	{
		SetKeyboardFocus();
	}
	if (OnHoveredFn)
	{
		OnHoveredFn();
	}
}
