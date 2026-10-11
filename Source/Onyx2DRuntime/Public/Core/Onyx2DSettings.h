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

	/** Additive radial glow drawn around Onyx Light 2D actors ("Color", "Intensity", "Falloff"). */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> LightHaloMaterial;

	/** Pawn the Onyx game mode spawns for players (e.g. a character made with the Character Wizard). Empty = Onyx Player Character. */
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay", meta = (MetaClass = "/Script/Onyx2DRuntime.OnyxCharacter"))
	FSoftClassPath DefaultPlayerClass;

	/** Enemy spawned by the level builder's 'E' tiles. Empty = Onyx Enemy Character. */
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay", meta = (MetaClass = "/Script/Onyx2DRuntime.OnyxCharacter"))
	FSoftClassPath DefaultEnemyClass;

	/** Imported textures get pixel-art settings (nearest filtering, no mips, uncompressed). */
	UPROPERTY(Config, EditAnywhere, Category = "Editor")
	bool bAutoConfigurePixelArtTextures = true;
};
