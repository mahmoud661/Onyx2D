// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Editor actions on the open level: one-click 2D setup and placing Onyx actors. */
class FOnyxLevelTools
{
public:
	/**
	 * X of the left edge for a new tile map: 0 in an empty level, otherwise right of every tile map already placed, with
	 * a gap of GapWorld, snapped to TileWidthWorld so tiles line up.
	 */
	static float GetFreeMapLeftEdge(float TileWidthWorld, float GapWorld);

	/** The level open in the editor (null without one). */
	static UWorld* GetEditorWorld();

	/** Adds what a Onyx2D level needs (game mode, player start, lights, tile collision). Returns what was added. */
	static TArray<FString> SetupCurrentLevel();

	/** Spawns an actor of Class on the gameplay plane in front of the viewport camera and selects it. */
	static AActor* PlaceInFrontOfCamera(UClass* Class);

	/** Point on the gameplay plane (Y = 0) the level viewport is looking at, snapped to a 50 cm grid. */
	static FVector GetViewportFocusOnPlane();
};
