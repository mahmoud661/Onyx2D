// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnyxNormalMaps.generated.h"

class UPaperTileSet;
class UTexture;
class UTexture2D;

/** How the relief of flat pixel art is guessed for 2D lighting. */
USTRUCT(BlueprintType)
struct FOnyxNormalMapSettings
{
	GENERATED_BODY()

	/** Width of the rounded edge around every shape, in art pixels (0 = flat shapes, detail only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Normal Map", meta = (ClampMin = "0", ClampMax = "32"))
	float BevelWidth = 2.5f;

	/** Height of that edge, in art pixels: higher = rounder, more shaded shapes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Normal Map", meta = (ClampMin = "0", ClampMax = "16"))
	float BevelHeight = 2.f;

	/** Bright pixels stand out and dark pixels (outlines, cracks) sink in, in art pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Normal Map", meta = (ClampMin = "0", ClampMax = "8"))
	float DetailHeight = 0.6f;

	/** Blur passes over the relief before the normals are taken (0 = sharp, pixel-exact shading). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Normal Map", meta = (ClampMin = "0", ClampMax = "4"))
	int32 Smoothing = 1;

	/** Overall strength of the result. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Normal Map", meta = (ClampMin = "0.05", ClampMax = "4"))
	float Strength = 1.f;
};

/** A tile sheet's layout: shapes never bevel across tile borders, so painted tiles join seamlessly. */
struct FOnyxPixelGrid
{
	FIntPoint CellSize = FIntPoint::ZeroValue;
	FIntPoint Margin = FIntPoint::ZeroValue;
	FIntPoint Spacing = FIntPoint::ZeroValue;

	bool IsValid() const { return CellSize.X > 0 && CellSize.Y > 0; }
	/** Cell rectangle that contains the pixel (the whole image when there is no grid). */
	FIntRect CellOf(int32 X, int32 Y, const FIntPoint& ImageSize) const;
};

/** Pure pixel maths: colour sheet in, tangent-space normal map out (DirectX convention, as Unreal expects). */
class FOnyxNormalMapGenerator
{
public:
	static TArray<FColor> Generate(const TArray<FColor>& Pixels, const FIntPoint& Size, const FOnyxNormalMapSettings& Settings,
		const FOnyxPixelGrid& Grid = FOnyxPixelGrid());

	/** Colour of a flat normal (pointing straight out of the sprite). */
	static constexpr FColor Flat = FColor(128, 128, 255, 255);
};

/**
 * Normal map assets for Paper2D: creates "<Texture>_N" next to the sheet and plugs it into the sprites and
 * tile sets that use the sheet (their Additional Textures slot 0, read by the Onyx lit sprite material).
 */
class FOnyxNormalMaps
{
public:
	/** Creates or refreshes the normal map of a sheet. Tile sets pass their grid so tiles stay seamless. */
	static UTexture2D* CreateFor(UTexture2D* Sheet, const FOnyxNormalMapSettings& Settings, const FOnyxPixelGrid& Grid = FOnyxPixelGrid());

	/** Puts the normal map in slot 0 of a sprite's or tile set's Additional Textures. */
	static bool Assign(UObject* SpriteOrTileSet, UTexture* NormalMap);

	/** The normal map currently plugged into a sprite or tile set (null when none). */
	static UTexture* GetAssigned(const UObject* SpriteOrTileSet);

	/**
	 * One click: builds the sheet's normal map and assigns it to every sprite and tile set that uses the sheet.
	 * Returns the number of assets that now have it.
	 */
	static int32 GenerateForSheet(UTexture2D* Sheet, const FOnyxNormalMapSettings& Settings);

	/** Grid of a tile set's sheet. */
	static FOnyxPixelGrid GridOf(const UPaperTileSet* TileSet);

private:
	static bool ReadPixels(UTexture2D* Texture, TArray<FColor>& OutPixels, FIntPoint& OutSize);
};
