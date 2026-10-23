// Copyright Onyx2D. All Rights Reserved.

#include "OnyxEditorExtensions.h"

namespace OnyxEditorExtensions
{
	TArray<FOnyxHomeCard>& Cards()
	{
		static TArray<FOnyxHomeCard> Registered;
		return Registered;
	}
}

void FOnyxEditorExtensions::AddHomeCard(FOnyxHomeCard Card)
{
	TArray<FOnyxHomeCard>& Cards = OnyxEditorExtensions::Cards();
	if (FOnyxHomeCard* Existing = Cards.FindByPredicate([&Card](const FOnyxHomeCard& Other) { return Other.Name == Card.Name; }))
	{
		*Existing = MoveTemp(Card);
	}
	else
	{
		Cards.Add(MoveTemp(Card));
	}
	OnHomeCardsChanged().Broadcast();
}

void FOnyxEditorExtensions::RemoveHomeCard(FName Name)
{
	if (OnyxEditorExtensions::Cards().RemoveAll([Name](const FOnyxHomeCard& Card) { return Card.Name == Name; }) > 0)
	{
		OnHomeCardsChanged().Broadcast();
	}
}

const TArray<FOnyxHomeCard>& FOnyxEditorExtensions::GetHomeCards()
{
	return OnyxEditorExtensions::Cards();
}

FSimpleMulticastDelegate& FOnyxEditorExtensions::OnHomeCardsChanged()
{
	static FSimpleMulticastDelegate Changed;
	return Changed;
}
