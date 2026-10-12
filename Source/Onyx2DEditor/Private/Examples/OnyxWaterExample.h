// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * A small 2D level to try Onyx Water, made with Sunny Land's tiles and fox: a pool painted on a tile map with the Ground
 * terrain, the water dragged out over it as a Level Painter area, crates that float, a rock that sinks and gems to dive
 * for. Everything is placed through the Level Painter's own calls, so it opens there ready to edit.
 * Needs the Sunny Land example (built first by the examples commandlet).
 */
class FOnyxWaterExample
{
public:
	static bool Build(FText& OutError);

	static const TCHAR* LevelPath;
};
