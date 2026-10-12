// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "OnyxInteractable.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UOnyxInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something the player can use with the Interact input while touching it: a lever, a door, a sign, a chest, an NPC.
 * Implement it on any actor (C++ or Blueprint: Class Settings > Interfaces); its collision must overlap the character.
 * Only Interact needs implementing.
 */
class ONYX2DRUNTIME_API IOnyxInteractable
{
	GENERATED_BODY()

public:
	/** The player used it. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Onyx2D|Interaction")
	void Interact(AActor* User);

	/** True while it can't be used (a locked door, a lever on cooldown). Usable by default. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Onyx2D|Interaction")
	bool IsInteractionLocked(AActor* User) const;

	/** Short verb for prompts ("Open", "Pull", "Talk"); empty = "Use". */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Onyx2D|Interaction")
	FText GetInteractionPrompt() const;
};
