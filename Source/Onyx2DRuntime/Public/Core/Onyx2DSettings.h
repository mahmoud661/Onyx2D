// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Core/Onyx2DTypes.h"
#include "Onyx2DSettings.generated.h"

class UMaterialInterface;
class UMaterialParameterCollection;

/** Project Settings > Plugins > Onyx2D. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Onyx2D"))
class ONYX2DRUNTIME_API UOnyx2DSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UOnyx2DSettings();

	static const UOnyx2DSettings* Get() { return GetDefault<UOnyx2DSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Texture pixels per world unit, used for pixel snapping (Paper2D default is 1). */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering", meta = (ClampMin = "0.01"))
	float PixelsPerUnit = 1.f;

	/**
	 * Lit material for sprites, flipbooks and tile maps: reacts to lights, casts shadows and reads the sprite's
	 * normal map (Additional Textures slot 0, made by "Generate Normal Map") for shading in relief.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> LitSpriteMaterial;

	/** Colorable material ("Color" parameter) used by placeholders, the level builder and hazards. */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> SolidColorMaterial;

	/** Unlit glowing material ("Color", "Intensity") used by pickups so they read in the dark. */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> GlowMaterial;

	/**
	 * Post-process that lights sprites the 2D way: scene base colour x (global light + sun + 2D light buffer),
	 * with normal-map relief ("LightBuffer", "PositionBuffer", "Ambient", "SunColor", "SunDirection", "NormalStrength").
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> LightCompositeMaterial;

	/** Values the lighting shares with every material ("BackgroundTint" for parallax layers). */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	TSoftObjectPtr<UMaterialParameterCollection> LightingParameters;

	/** Onyx Water shader: depth gradient, caustics, bubbles, smooth surface line ("ShallowColor", "DeepColor", "SurfaceColor", "Depth", "Caustics", "Tint"...). */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> WaterMaterial;

	/** Round, soft splash droplets for Onyx Water (vertex colour, "Tint"). */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> WaterDropMaterial;

	/** Onyx Fog's drifting fog, lit by the 2D lights ("Tint", "Density", "CloudSize", "Drift", "Ground", "Clear0"...). */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> FogMaterial;

	/** Additive radial glow drawn around Onyx Light 2D actors ("Color", "Intensity", "Falloff"). */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> LightHaloMaterial;

	/** See-through silhouette filled with the sprite's colour, for afterimages such as dash trails ("Intensity"). */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> AfterimageMaterial;

	/**
	 * How levels are played unless they say otherwise (World Settings > Onyx Level): seen from the side (a platformer) or
	 * from above (top down). A game can mix both: an overworld from above, its dungeons from the side.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay")
	EOnyxGameStyle DefaultGameStyle = EOnyxGameStyle::SideView;

	/** From above: a prop standing in front of the player (a tree, a house) turns see-through while it hides the player. */
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay")
	bool bSeeThroughProps = true;

	/** How see-through: the part of its art pixels that drop out, in an even pattern. */
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay", meta = (ClampMin = "0.1", ClampMax = "1", EditCondition = "bSeeThroughProps"))
	float SeeThroughAmount = 0.5f;

	/** Character the Onyx game mode spawns for the player (one with Player Control). Empty = a grey stand-in you can already play. */
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay", meta = (MetaClass = "/Script/Onyx2DRuntime.OnyxCharacter"))
	FSoftClassPath DefaultPlayerClass;

	/**
	 * W and the Up arrow jump, like Space. Off: they aim up instead (up-slashes, climbing onto ledges) and Space jumps
	 * (the stick and the d-pad aim up either way).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay")
	bool bUpKeysJump = true;

	/** Character the level builder's 'E' tiles spawn. Empty = a grey patrolling stand-in. */
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay", meta = (MetaClass = "/Script/Onyx2DRuntime.OnyxCharacter"))
	FSoftClassPath DefaultEnemyClass;

	/**
	 * Days, seasons and the clock (Onyx Calendar): time runs from Day Start to Day End while the game plays, drives the
	 * lighting's hour, and a new day begins when the player sleeps (or passes out at Day End).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Calendar")
	bool bUseCalendar = false;

	/** Game minutes that pass each real second (Stardew-like days: about 1.4, a 20-hour day in 14 minutes). */
	UPROPERTY(Config, EditAnywhere, Category = "Calendar", meta = (ClampMin = "0.01", EditCondition = "bUseCalendar"))
	float GameMinutesPerSecond = 1.43f;

	/** When a day starts and when the player passes out, in minutes after midnight (26:00 = 2 am the next morning). */
	UPROPERTY(Config, EditAnywhere, Category = "Calendar", meta = (ClampMin = "0", ClampMax = "1440", EditCondition = "bUseCalendar"))
	int32 DayStartMinute = 360;

	UPROPERTY(Config, EditAnywhere, Category = "Calendar", meta = (ClampMin = "60", ClampMax = "2880", EditCondition = "bUseCalendar"))
	int32 DayEndMinute = 1560;

	/** Days in a season, and the seasons in order. */
	UPROPERTY(Config, EditAnywhere, Category = "Calendar", meta = (ClampMin = "1", EditCondition = "bUseCalendar"))
	int32 DaysPerSeason = 28;

	UPROPERTY(Config, EditAnywhere, Category = "Calendar", meta = (EditCondition = "bUseCalendar"))
	TArray<FText> SeasonNames = { INVTEXT("Spring"), INVTEXT("Summer"), INVTEXT("Autumn"), INVTEXT("Winter") };

	/** The game picks up where the last save left off (Onyx Save: what the player owns, the farm, the day) when it starts. */
	UPROPERTY(Config, EditAnywhere, Category = "Saving")
	bool bContinueFromSave = false;

	/** Imported textures get pixel-art settings (nearest filtering, no mips, uncompressed). */
	UPROPERTY(Config, EditAnywhere, Category = "Editor")
	bool bAutoConfigurePixelArtTextures = true;

	/**
	 * 2D levels (with tile maps, a Level Builder or Onyx Lighting) open in the flat side view of the gameplay plane,
	 * framed on the level, instead of the perspective view.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Editor")
	bool bOpenLevelsInSideView = true;

	/**
	 * Where the editor's Help buttons open the Onyx2D documentation. The default is the docs site running on this
	 * computer (npm run dev in the Onyx2D-Docs folder); point it at wherever your team hosts the docs.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Editor")
	FString DocsUrl = TEXT("http://localhost:3000/docs");
};
