// Copyright Onyx2D. All Rights Reserved.

#include "Level/OnyxPlaceables.h"
#include "Camera/OnyxCameraBounds.h"
#include "Character/OnyxCharacter.h"
#include "GameFramework/PlayerStart.h"
#include "Lighting/OnyxLight2D.h"
#include "Lighting/OnyxLightingEnvironment.h"
#include "World/OnyxBreakable.h"
#include "World/OnyxCheckpoint.h"
#include "World/OnyxHazardVolume.h"
#include "World/OnyxLevelBuilder.h"
#include "World/OnyxLevelExit.h"
#include "World/OnyxMovingPlatform.h"
#include "World/OnyxOneWayPlatform.h"
#include "World/OnyxParallaxBackground.h"
#include "World/OnyxPhysicsProp.h"
#include "World/OnyxPickup.h"
#include "World/OnyxMirrorFloor.h"
#include "World/OnyxWater.h"
#include "World/OnyxFog.h"
#include "World/OnyxLightSwitch.h"
#include "World/OnyxSign.h"
#include "Logic/OnyxTriggerZone.h"
#include "Audio/OnyxLevelAudio.h"

const TArray<FOnyxPlaceable>& GetOnyxPlaceables()
{
	static const TArray<FOnyxPlaceable> Placeables = {
		{ APlayerStart::StaticClass(), INVTEXT("Player Start"), INVTEXT("Where the player spawns.") },
		{ AOnyxCharacter::StaticClass(), INVTEXT("Character"), INVTEXT("A 2D character (grey stand-in until it has art). Give it behaviours in Details: Player Control, Patrol, Contact Damage...") },
		{ AOnyxPickup::StaticClass(), INVTEXT("Pickup"), INVTEXT("Coin or health. Bobs in place, collected on touch.") },
		{ AOnyxCheckpoint::StaticClass(), INVTEXT("Checkpoint"), INVTEXT("Sets the respawn point and heals.") },
		{ AOnyxLevelExit::StaticClass(), INVTEXT("Level Exit"), INVTEXT("Fades out and opens the next level.") },
		{ AOnyxMovingPlatform::StaticClass(), INVTEXT("Moving Platform"), INVTEXT("Travels between waypoints you drag in the viewport.") },
		{ AOnyxOneWayPlatform::StaticClass(), INVTEXT("One-Way Platform"), INVTEXT("Jump up through it, drop down with Down + Jump.") },
		{ AOnyxHazardVolume::StaticClass(), INVTEXT("Hazard"), INVTEXT("Spikes / lava: hurts on touch.") },
		{ AOnyxWater::StaticClass(), INVTEXT("Water"), INVTEXT("Characters swim, physics objects float and bob, the surface ripples and splashes. Set its Size, current and colours.") },
		{ AOnyxFog::StaticClass(), INVTEXT("Fog"), INVTEXT("Drifting fog over part of the level: glows where 2D lights shine into it, thins round the player and lanterns (Onyx Fog Clearer), thickens at night. In front of characters or behind them. Presets: Ground Mist, Marsh Fog, Cave Haze, Fog Wall, Night Mist.") },
		{ AOnyxMirrorFloor::StaticClass(), INVTEXT("Mirror Floor"), INVTEXT("A polished floor that mirrors the scene above it (characters, props, walls, lights), fading with depth; shows while playing. Put it on the floor's surface; set its Width, Strength, Depth and Tint.") },
		{ AOnyxPhysicsProp::StaticClass(), INVTEXT("Physics Prop"), INVTEXT("A crate, barrel or rock (any sprite) that falls, tumbles and floats in water, or sinks when it is denser.") },
		{ AOnyxBreakable::StaticClass(), INVTEXT("Breakable"), INVTEXT("A pot, urn or barrel (any sprite) the player smashes: it rocks when struck, bursts with an effect and a sound, and can spill coins. Never blocks the way.") },
		{ AOnyxLightingEnvironment::StaticClass(), INVTEXT("Onyx Lighting"), INVTEXT("The level's look: global light, a 2D sun for normal maps, bloom, colour grade, day and night. Presets: Day, Dusk, Night, Cave, Dungeon.") },
		{ AOnyxLight2D::StaticClass(), INVTEXT("2D Light"), INVTEXT("A light from a point, a cone, a line or a window-shaped area, with 2D shadows: torch, candle, neon, searchlight, hanging lantern, alarm, stained glass and more. Flickers, pulses, strobes, sweeps or swings; can light itself at night only; optional halo and sound.") },
		{ AOnyxLightSwitch::StaticClass(), INVTEXT("Light Switch"), INVTEXT("Lever or pressure plate that turns lights on / off with a fade (Interact input, or walking over it).") },
		{ AOnyxTriggerZone::StaticClass(), INVTEXT("Trigger Zone"), INVTEXT("Invisible box that makes things happen with no Blueprint: pick When (the player enters, presses Interact...) and list Actions (message, open a door, give a key, teleport...).") },
		{ AOnyxSign::StaticClass(), INVTEXT("Sign"), INVTEXT("A sign the player reads with Interact: its pages show in the dialogue box.") },
		{ AOnyxLevelAudio::StaticClass(), INVTEXT("Level Audio"), INVTEXT("The level's music and ambience loop; Camera Zones can override them.") },
		{ AOnyxCameraBounds::StaticClass(), INVTEXT("Camera Zone"), INVTEXT("Confines the camera and/or switches its profile inside an area.") },
		{ AOnyxParallaxBackground::StaticClass(), INVTEXT("Parallax Background"), INVTEXT("Layers of tiling textures that scroll with the camera.") },
		{ AOnyxLevelBuilder::StaticClass(), INVTEXT("Level Builder"), INVTEXT("Block out a level by typing it as text.") },
	};
	return Placeables;
}
