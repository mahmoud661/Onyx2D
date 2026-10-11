// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/** Onyx2D editor icons (tool tabs, toolbar, class icons for the Onyx actors). */
class FOnyxEditorStyle
{
public:
	static void Register();
	static void Unregister();

	static FName GetStyleSetName();
	static const ISlateStyle& Get();

	/** Icon from this style set, e.g. Icon("Onyx2D.Tab.Tiles"). */
	static FSlateIcon Icon(FName Name);

private:
	static TSharedPtr<FSlateStyleSet> StyleSet;
};
