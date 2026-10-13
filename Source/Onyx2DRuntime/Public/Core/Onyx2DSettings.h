// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
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

	/** Additive radial glow drawn around Onyx Light 2D actors ("Color", "Intensity", "Falloff"). */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> LightHaloMaterial;

	/** Character the Onyx game mode spawns for the player (one with Player Control). Empty = a grey stand-in you can already play. */
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay", meta = (MetaClass = "/Script/Onyx2DRuntime.OnyxCharacter"))
	FSoftClassPath DefaultPlayerClass;

	/** Character the level builder's 'E' tiles spawn. Empty = a grey patrolling stand-in. */
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay", meta = (MetaClass = "/Script/Onyx2DRuntime.OnyxCharacter"))
	FSoftClassPath DefaultEnemyClass;

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
