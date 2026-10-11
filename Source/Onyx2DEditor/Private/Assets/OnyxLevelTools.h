// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Editor actions on the open level: one-click 2D setup and placing Onyx actors. */
class FOnyxLevelTools
{
public:
	/** Adds what a Onyx2D level needs (game mode, player start, lights, tile collision). Returns what was added. */
	static TArray<FString> SetupCurrentLevel();

	/** Spawns an actor of Class on the gameplay plane in front of the viewport camera and selects it. */
	static AActor* PlaceInFrontOfCamera(UClass* Class);

	/** Point on the gameplay plane (Y = 0) the level viewport is looking at, snapped to a 50 cm grid. */
	static FVector GetViewportFocusOnPlane();
};
