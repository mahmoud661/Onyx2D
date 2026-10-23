// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** A button on an Onyx Home card, also listed in the Onyx2D menu. */
struct FOnyxHomeButton
{
	FText Label;
	FText Tooltip;
	TFunction<void()> OnClicked;
	/** Drawn as the card's main button. */
	bool bPrimary = false;
};

/** A card another module adds to the Onyx2D window's Home tab, after its own steps. */
struct FOnyxHomeCard
{
	/** Unique: adding a card with the same name replaces it. */
	FName Name;
	FText Title;
	FText Description;
	TArray<FOnyxHomeButton> Buttons;
	/** The section of the Onyx2D menu its buttons are listed in (empty: not in the menu). */
	FText MenuSection;
};

/**
 * Lets other editor modules add to Onyx2D's own tools without Onyx2D knowing them: the Onyx2D Examples plugin puts its
 * example levels on the Home tab and in the Onyx2D menu this way, and your game's editor module can too.
 */
class ONYX2DEDITOR_API FOnyxEditorExtensions
{
public:
	static void AddHomeCard(FOnyxHomeCard Card);
	static void RemoveHomeCard(FName Name);
	static const TArray<FOnyxHomeCard>& GetHomeCards();
	/** Cards were added or removed: an open Home tab shows them again. */
	static FSimpleMulticastDelegate& OnHomeCardsChanged();
};
