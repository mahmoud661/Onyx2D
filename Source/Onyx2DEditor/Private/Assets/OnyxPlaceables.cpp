// Copyright Onyx2D. All Rights Reserved.

#include "Assets/OnyxPlaceables.h"
#include "Camera/OnyxCameraBounds.h"
#include "Character/OnyxEnemyCharacter.h"
#include "GameFramework/PlayerStart.h"
#include "Lighting/OnyxLight2D.h"
#include "Lighting/OnyxLightingEnvironment.h"
#include "World/OnyxCheckpoint.h"
#include "World/OnyxHazardVolume.h"
#include "World/OnyxLevelBuilder.h"
#include "World/OnyxLevelExit.h"
#include "World/OnyxMovingPlatform.h"
#include "World/OnyxOneWayPlatform.h"
#include "World/OnyxParallaxBackground.h"
#include "World/OnyxPickup.h"

const TArray<FOnyxPlaceable>& GetOnyxPlaceables()
{
	static const TArray<FOnyxPlaceable> Placeables = {
		{ APlayerStart::StaticClass(), INVTEXT("Player Start"), INVTEXT("Where the player spawns.") },
		{ AOnyxEnemyCharacter::StaticClass(), INVTEXT("Enemy"), INVTEXT("Patrols, chases and hurts on contact.") },
		{ AOnyxPickup::StaticClass(), INVTEXT("Pickup"), INVTEXT("Coin or health. Bobs in place, collected on touch.") },
		{ AOnyxCheckpoint::StaticClass(), INVTEXT("Checkpoint"), INVTEXT("Sets the respawn point and heals.") },
		{ AOnyxLevelExit::StaticClass(), INVTEXT("Level Exit"), INVTEXT("Fades out and opens the next level.") },
		{ AOnyxMovingPlatform::StaticClass(), INVTEXT("Moving Platform"), INVTEXT("Travels between waypoints you drag in the viewport.") },
		{ AOnyxOneWayPlatform::StaticClass(), INVTEXT("One-Way Platform"), INVTEXT("Jump up through it, drop down with Down + Jump.") },
		{ AOnyxHazardVolume::StaticClass(), INVTEXT("Hazard"), INVTEXT("Spikes / lava: hurts on touch.") },
		{ AOnyxLightingEnvironment::StaticClass(), INVTEXT("Onyx Lighting"), INVTEXT("The level's look: sun, ambient, depth fog, bloom, colour grade. Presets: Day, Dusk, Night, Cave, Dungeon.") },
		{ AOnyxLight2D::StaticClass(), INVTEXT("2D Light"), INVTEXT("Torch, candle, lantern, magic, crystal, lava or window light with a glow halo; flickers or pulses.") },
		{ AOnyxCameraBounds::StaticClass(), INVTEXT("Camera Zone"), INVTEXT("Confines the camera and/or switches its profile inside an area.") },
		{ AOnyxParallaxBackground::StaticClass(), INVTEXT("Parallax Background"), INVTEXT("Layers of tiling textures that scroll with the camera.") },
		{ AOnyxLevelBuilder::StaticClass(), INVTEXT("Level Builder"), INVTEXT("Block out a level by typing it as text.") },
	};
	return Placeables;
}
