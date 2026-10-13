// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "World/OnyxInteractable.h"
#include "OnyxTalkComponent.generated.h"

class UOnyxAction;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxTalkSignature, AActor*, Listener);

/**
 * Lets the player talk to this character: walk up, press Interact, and their lines show in the dialogue box with their
 * name. Later talks can say something else, and the first talk can do things afterwards (give a key, open a gate).
 * The sphere is the talking range. While talking the character faces the player and stops patrolling.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Talk"))
class ONYX2DRUNTIME_API UOnyxTalkComponent : public USphereComponent, public IOnyxInteractable
{
	GENERATED_BODY()

public:
	UOnyxTalkComponent();

	/** Their name, shown above what they say. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Talk")
	FText SpeakerName = INVTEXT("Villager");

	/** Their face next to the text: a sprite or texture (empty = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Talk", meta = (AllowedClasses = "/Script/Engine.Texture2D,/Script/Paper2D.PaperSprite"))
	TObjectPtr<UObject> Portrait;

	/** What they say the first time. Each entry is one line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Talk", meta = (MultiLine = true))
	TArray<FText> Lines;

	/** What they say from the second time on (empty = the same lines again). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Talk", meta = (MultiLine = true))
	TArray<FText> LaterLines;

	/** What happens after the first talk: give an item, open a door, start music. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Onyx2D|Talk")
	TArray<TObjectPtr<UOnyxAction>> AfterFirstTalk;

	/** A short blip played as the letters appear, like a voice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Talk")
	TObjectPtr<USoundBase> Voice;

	/** The word on the Interact prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Talk")
	FText Prompt = INVTEXT("Talk");

	/** Turn toward the player while talking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Talk")
	bool bFacePlayer = true;

	/** A conversation started. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Talk")
	FOnyxTalkSignature OnTalkStarted;

	/** The player finished reading. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Talk")
	FOnyxTalkSignature OnTalkFinished;

	/** Starts talking to Listener, as if they pressed Interact. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Talk")
	void Talk(AActor* Listener);

	/** How many conversations have finished. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Talk")
	int32 GetTimesTalked() const { return TimesTalked; }

	// IOnyxInteractable
	virtual void Interact_Implementation(AActor* User) override;
	virtual bool IsInteractionLocked_Implementation(AActor* User) const override;
	virtual FText GetInteractionPrompt_Implementation() const override;

private:
	void FinishTalk(AActor* Listener);

	int32 TimesTalked = 0;
	bool bTalking = false;
};
