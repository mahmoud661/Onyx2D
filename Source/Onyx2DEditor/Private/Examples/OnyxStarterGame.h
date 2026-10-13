// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Builds a small, complete game to start from, with the Sunny Land art and no Blueprint code: a meadow level with a
 * tutorial sign, a frog to talk to, enemies and gems, a bouncy-mushroom pit (a launch zone) up to a key, a checkpoint,
 * a locked gate that needs the key, and an ending conversation; plus a main menu level. Everything is made of Trigger
 * Zones, Signs and the Talk behaviour, so a beginner can open any piece and see how it works.
 */
class FOnyxStarterGame
{
public:
	/**
	 * Builds (or rebuilds) the starter game, building the Sunny Land example first when its art isn't imported yet.
	 * bMakeProjectStart also makes it what the game opens with: Project Settings > Onyx2D UI's main menu and first
	 * level, and the game's default map. Returns false with a reason when it can't.
	 */
	static bool Build(bool bMakeProjectStart, FText& OutError);

	static const TCHAR* ContentFolder;
	static const TCHAR* LevelPath;
	static const TCHAR* MenuPath;
};
