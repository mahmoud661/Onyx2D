// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Builds a complete, playable example from "Sunny Land" by ansimuz (CC0, opengameart.org/node/74142):
 * animated fox player, opossum / eagle / frog enemies (walk / fly / hop, stompable), cherries and gems with
 * effects, the original level imported from its Tiled map with the game's solid and one-way tiles, props,
 * parallax and 2D lighting (sunny outside, a dark torch-lit cave underground) and a Ground terrain.
 * Everything goes through the plugin's own C++ builders; the result is /Game/Maps/SunnyLand.
 */
class FOnyxSunnyLandExample
{
public:
	/** The pack's art, shipped in the plugin (Extras/SunnyLand). */
	static FString GetPackFolder();
	static bool IsPackPresent();

	/** Builds (or rebuilds) every asset and the level. Returns false with a reason when it can't. */
	static bool Build(FText& OutError);

	static const TCHAR* LevelPath;
};
