// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SWidget;
class UOnyxClipEvent;
struct FSlateBrush;

/** Sizes, colours and brushes shared by the clip timeline's widgets, so the header column and the lanes line up. */
namespace OnyxTimeline
{
	constexpr float HeaderWidth = 172.f;
	constexpr float BarHeight = 34.f;
	constexpr float RulerHeight = 28.f;
	constexpr float FramesHeight = 70.f;
	constexpr float LaneHeight = 32.f;
	constexpr float AddLaneHeight = 28.f;

	FLinearColor Panel();       // the lane area
	FLinearColor Bar();         // top bar
	FLinearColor Header();      // header column
	FLinearColor RulerColor();
	FLinearColor FramesRow();
	FLinearColor FrameCell();
	FLinearColor FrameCellLine();
	FLinearColor Divider();
	FLinearColor Playhead();
	FLinearColor Accent();
	FLinearColor TextColor();
	FLinearColor MutedText();

	/** Lane background: the panel with a hint of the lane's colour. */
	FLinearColor LaneRow(const FLinearColor& LaneColor, int32 Index);
	/** Fill for an event's flag tag or window capsule (its timeline colour, lifted). */
	FLinearColor EventFill(const FLinearColor& Color);
	/** Text / icon colour on that fill: a deep shade of the same hue. */
	FLinearColor EventInk(const FLinearColor& Color);

	/** White rounded boxes, tinted when drawn. */
	const FSlateBrush* Rounded(float Radius);
	/** A flag's tag: rounded, with the corner next to the stem square. */
	const FSlateBrush* FlagTag();

	/** Icon for an event or window (by its class), and its style name for menus. */
	const FSlateBrush* EventIcon(const UOnyxClipEvent* Event);
	FName EventIconName(const UClass* Class);

	/** The lane colours offered in the colour menus. */
	struct FNamedColor { FText Name; FLinearColor Color; };
	const TArray<FNamedColor>& LaneColors();

	/** A small window asking for a name (spaces removed, as for message names); OnCommit runs on Enter. */
	void PromptForName(const TSharedRef<SWidget>& Parent, const FText& Title, const FText& Initial, TFunction<void(FName)> OnCommit);
}
