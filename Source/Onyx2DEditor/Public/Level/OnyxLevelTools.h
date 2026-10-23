// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/Onyx2DTypes.h"

class AActor;
class UWorld;

/** Editor actions on the open level: one-click 2D setup and placing Onyx actors. */
class ONYX2DEDITOR_API FOnyxLevelTools
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

	/**
	 * How a level is played, kept in its World Settings (Onyx Level): seen from the side or from above. Undoable; the
	 * level is marked as changed. Returns false without a world.
	 */
	static bool SetGameStyle(UWorld* World, EOnyxGameStyle Style);

	/** Spawns an actor of Class on the gameplay plane in front of the viewport camera and selects it. */
	static AActor* PlaceInFrontOfCamera(UClass* Class);

	/** Point on the gameplay plane (Y = 0) the level viewport is looking at, snapped to a 50 cm grid. */
	static FVector GetViewportFocusOnPlane();

	/** A 2D level: it has tile maps, a Level Builder or Onyx Lighting. */
	static bool IsLevel2D(UWorld* World);

	/** What to frame in a 2D level: its tile maps and Level Builders (invalid when it has none). */
	static FBox GetLevelBounds2D(UWorld* World);

	/**
	 * Turns the editor's perspective viewports into the flat side view of the gameplay plane (lit, animating), framing
	 * Area when it is valid.
	 */
	static void ShowSideView(const FBox& Area);

	/**
	 * Files the level's actors in the World Outliner under the tile map they stand in, by kind: "<Map>/Characters",
	 * "<Map>/Lights", "<Map>/Props", "<Map>/Pickups", "<Map>/Gameplay"... (the map itself in "<Map>"). What is outside
	 * every map, or belongs to the whole level (mood, backdrop, audio), goes in "Level/...". Actors you put in folders of
	 * your own stay where they are. Returns how many moved.
	 */
	static int32 OrganizeOutliner(UWorld* World);

	/** Files one actor (just placed) the same way. */
	static void FileInOutliner(AActor* Actor);
};
