// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OnyxDialogueSubsystem.generated.h"

class USoundBase;

/** One line of a conversation. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxDialogueLine
{
	GENERATED_BODY()

	/** Who is talking, shown above the text (empty = no name). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText Speaker;

	/** What they say. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText Text;

	/** A face shown next to the text: a sprite or a texture (empty = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (AllowedClasses = "/Script/Engine.Texture2D,/Script/Paper2D.PaperSprite"))
	TObjectPtr<UObject> Portrait;

	/** A short blip played as the letters appear, like a voice (empty = silent). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", AdvancedDisplay)
	TObjectPtr<USoundBase> Voice;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnyxDialogueSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxMessageSignature, const FText&, Message);

/**
 * Messages and conversations for the level. A message is a short line across the screen that fades by itself; a
 * conversation is a box at the bottom that types its lines out while the player reads, pressing Jump or Interact to go
 * on. The player can't move while a conversation is open. The Onyx2D UI draws both; your own UI can read them from here.
 */
UCLASS()
class ONYX2DRUNTIME_API UOnyxDialogueSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UOnyxDialogueSubsystem* Get(const UObject* WorldContext);

	/** Shows a short message across the screen ("The door is locked") that fades after Duration seconds. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Dialogue")
	void ShowMessage(const FText& Message, float Duration = 3.f);

	/** Starts a conversation, or queues it after the one open now. LettersPerSecond 0 shows each line at once. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Dialogue")
	void StartDialogue(const TArray<FOnyxDialogueLine>& Lines, float LettersPerSecond = 45.f);

	/** C++: the same, calling OnFinished once the player has read the last line. */
	void QueueDialogue(const TArray<FOnyxDialogueLine>& Lines, float LettersPerSecond, TFunction<void()> OnFinished);

	/** Shows the rest of the line, or goes to the next one (what Jump and Interact do while talking). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Dialogue")
	void Advance();

	/** Closes every open and queued conversation now (their After actions still run). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Dialogue")
	void EndAllDialogue();

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Dialogue")
	bool IsTalking() const { return Conversations.Num() > 0; }

	/** The line on screen now (empty when not talking). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Dialogue")
	FOnyxDialogueLine GetLine() const;

	/** The part of the line typed out so far. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Dialogue")
	FText GetShownText() const;

	/** The whole line is on screen: the next press goes to the next line. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Dialogue")
	bool IsLineComplete() const;

	/** True on the conversation's last line. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Dialogue")
	bool IsLastLine() const;

	/** The message on screen now (empty = none). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Dialogue")
	FText GetMessage() const { return Message; }

	/** How visible the message is (fades in and out), 0..1. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Dialogue")
	float GetMessageOpacity() const;

	/** Counts up each time a message is shown, so UI can tell a repeated message apart. */
	int32 GetMessageSerial() const { return MessageSerial; }

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Dialogue")
	FOnyxDialogueSignature OnDialogueStarted;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Dialogue")
	FOnyxDialogueSignature OnDialogueEnded;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Dialogue")
	FOnyxMessageSignature OnMessageShown;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FConversation
	{
		TArray<FOnyxDialogueLine> Lines;
		float LettersPerSecond = 45.f;
		TFunction<void()> OnFinished;
	};

	void BeginLine();
	void FinishConversation();
	int32 GetLineLength() const;

	/** [0] is on screen; the rest wait their turn. */
	TArray<FConversation> Conversations;
	int32 LineIndex = 0;
	float ShownLetters = 0.f;
	int32 LettersSinceVoice = 0;
	/** Seconds since the conversation opened: a press this soon is the one that opened it, so it is ignored. */
	float OpenTime = 0.f;

	FText Message;
	float MessageAge = 0.f;
	float MessageDuration = 0.f;
	int32 MessageSerial = 0;
};
