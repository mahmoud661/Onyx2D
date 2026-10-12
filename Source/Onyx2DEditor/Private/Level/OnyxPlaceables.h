// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** An actor offered in the Level tool and in the Place Actors > Onyx2D category. */
struct FOnyxPlaceable
{
	UClass* Class = nullptr;
	FText Label;
	FText Description;
};

/** Every placeable Onyx2D actor, in display order. */
const TArray<FOnyxPlaceable>& GetOnyxPlaceables();
