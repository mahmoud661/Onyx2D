// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/**
 * Frame-by-frame art (run-1.png, run-2.png...) to one animation strip, the layout the Character Wizard and
 * flipbook tools read. Frames of different sizes are centred on the bottom of a common cell so feet line up.
 */
class FOnyxFramePacker
{
public:
	/** Natural order by name: "run-2" before "run-10". */
	static void SortFrames(TArray<UTexture2D*>& Frames);

	/** Packs frames left to right into Folder/Name. OutFrameSize is the cell size used. */
	static UTexture2D* PackStrip(const TArray<UTexture2D*>& Frames, const FString& Folder, const FString& Name, FIntPoint& OutFrameSize, FText& OutError);
};
