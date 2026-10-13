// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OnyxDialogueWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * Draws the level's messages (a banner near the top that fades by itself) and conversations (a box at the bottom with
 * the speaker's name, their portrait and the typed-out line), read from the Onyx Dialogue subsystem. Shown in every
 * level except the main menu. Swap it in Project Settings > Onyx2D UI > Dialogue Class.
 *
 * A Widget Blueprint child with no layout of its own gets the built-in one. With a layout of its own (the Designer),
 * name its widgets like the properties below (MessagePanel, MessageText, Box, SpeakerText, LineText, PortraitFrame,
 * Portrait, ContinueMark) and they are driven the same way; any of them can be left out.
 */
UCLASS(Blueprintable)
class ONYX2DUI_API UOnyxDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildUI();
	/** The line with line breaks where it will wrap, so typing it out never moves a word to the next line midway. */
	FString WrapLine(const FString& Line, bool bWithPortrait) const;

protected:
	/** The message banner: shown and faded while a message is up. */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MessagePanel;

	/** The message's text. */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	/** The conversation box: shown and slid in while talking. */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Box;

	/** The speaker's name. */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpeakerText;

	/** The line, typed out letter by letter. */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LineText;

	/** Shown only when the line has a portrait. */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PortraitFrame;

	/** The speaker's portrait. */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Portrait;

	/** Blinks when the player can go on. */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ContinueMark;

private:
	float BoxShown = 0.f;
	float Blink = 0.f;
	float MessagePop = 1.f;
	int32 ShownMessageSerial = 0;
	FString WrappedSource;
	FString Wrapped;
	TWeakObjectPtr<UObject> ShownPortrait;
};
