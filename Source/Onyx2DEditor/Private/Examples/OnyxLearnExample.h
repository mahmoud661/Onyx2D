// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * What the docs' Learn Unreal pages build, finished, in Content/LearnUnreal, so every screenshot there is of a real
 * asset you can open and compare with your own:
 *  - BP_HelloWorld: an actor Blueprint whose Event Graph prints a line when the game starts (the first Blueprint).
 *  - BP_Spring: a spring pad Blueprint (a Box Collision and a Paper Sprite) whose graph launches any character that
 *    touches it, with an editable Launch Speed variable.
 *  - Input: Onyx2D's built-in controls saved as Enhanced Input assets (an action per control, a mapping context and an
 *    Onyx Input Config), to read or copy when you rebind the keys.
 *  - LU_Playground: a small Sunny Land level with both springs (the Blueprint one and the C++ ASpringPad from the demo
 *    project, when it is compiled in), the hello actor and the fox starting over the Blueprint spring.
 *  - BP_LearnLever, BP_LearnDoor, BP_LearnLamp and LU_TalkingBlueprint: the "How Classes Talk to Each Other" example
 *    as Blueprints (an interface event, a cast, a direct reference, an event dispatcher, Bind Event and Unbind). LU_Talking
 *    is the same level made of the demo project's C++ classes.
 *  - WBP_LearnHUD: a HUD with a layout of its own ("Menus and HUD"), a Widget Blueprint child of the Onyx HUD whose
 *    Designer widgets are named so Onyx2D fills them in.
 * The Blueprint graphs are built node by node, exactly as the pages tell you to place and wire them.
 * Needs the Sunny Land example (built first by the examples commandlet).
 */
class FOnyxLearnExample
{
public:
	static bool Build(FText& OutError);
	/** Build, building the Sunny Land example first when its art isn't in the project yet. */
	static bool BuildWithSunnyLand(FText& OutError);

	static const TCHAR* ContentFolder;
	static const TCHAR* LevelPath;
	/** The lever, door and lamp of "How Classes Talk to Each Other" (when the demo project classes are compiled in). */
	static const TCHAR* TalkingLevelPath;
	/** The same level made of Blueprints: BP_LearnLever, BP_LearnDoor and BP_LearnLamp. */
	static const TCHAR* TalkingBlueprintLevelPath;
};
