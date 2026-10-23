// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "Lighting/OnyxVisibility2D.h"
#include "UObject/ObjectKey.h"

class FTexture;
class UOnyxLight2DComponent;
class UTexture;
class UTextureRenderTarget2D;
class UWorld;
enum class EOnyxLightFalloff : uint8;

/** What the light buffer is drawn for: a camera and the size of the screen it fills. */
struct FOnyxLightView
{
	FMinimalViewInfo POV;
	FIntPoint ViewportSize = FIntPoint::ZeroValue;

	bool IsValid() const { return ViewportSize.X > 0 && ViewportSize.Y > 0; }
};

/** A light's shadow, kept while nothing it depends on changes (its place, shape and reach, and the walls). */
struct FOnyxLightShadowCache
{
	uint32 Key = 0;
	bool bValid = false;
	/** Point and Spot: where its rays stop, and where the light soaking into walls fades out. */
	TArray<FVector2D> Polygon;
	TArray<FVector2D> Soak;
	/** Line and Area: how lit each point of its grid is (0 in shadow, 1 lit). */
	TArray<float> Visibility;
};

using FOnyxLightShadowCaches = TMap<TObjectKey<UOnyxLight2DComponent>, FOnyxLightShadowCache>;

/** Light shaped by pixels of art (an Onyx Pixel Light): a textured rectangle on the 2D plane, drawn as it is (no shadows). */
struct FOnyxPixelLightQuad
{
	/** Plane corners: top left, top right, bottom right, bottom left. */
	FVector2D Corners[4];
	FVector2D UVs[4];
	const FTexture* Texture = nullptr;
	/** The halo's picture (same corners and UVs); null: Texture. */
	const FTexture* HaloTexture = nullptr;
	FLinearColor Color = FLinearColor::White;
	/** Glow in the air round the pixels, added over the picture as it is (it lights nothing): colour x strength, black for none. */
	FLinearColor Halo = FLinearColor::Black;
	bool bOwnGlow = true;
};

/** What a frame of light drawing did (for the log and for tuning). */
struct FOnyxLightBufferStats
{
	int32 Drawn = 0;
	int32 ShadowsReused = 0;
	int32 ShadowsComputed = 0;
	int32 PixelLightsDrawn = 0;
};

/**
 * Draws the screen-space 2D light buffers:
 *  - Light buffer (RGB): every light's colour x falloff (or its pattern), clipped by 2D shadows. Points and spots are
 *    fans clipped to their visibility polygon; lines and areas are grids, each point lit by its distance to the shape
 *    and shadowed when a wall stands between it and the nearest point of the light.
 *  - Position buffer (RGB): sum of (light screen UV + 1, light height) weighted by the light's brightness,
 *    so the composite can find each pixel's average light direction for normal-mapped relief.
 *  - Own glow buffer (RGB): the share of the light buffer that comes from own glows (Own Glow lights), which the
 *    composite takes back off the sprites guarded from washing out (Light Guard).
 *  - Halo buffer (RGB): glow in the air round Pixel Lights (a blade burning bright), which the composite adds on top of
 *    everything, past the sprite's outline and over the character itself.
 * The caller owns the render targets, the falloff atlas and the shadow caches.
 */
class ONYX2DRUNTIME_API FOnyxLightBufferRenderer
{
public:
	/**
	 * @param Caches           Shadows kept between frames (null: computed every time).
	 * @param OccluderVersion  Changes whenever any wall or shadow caster does, so kept shadows know to be redone.
	 */
	static void Render(UWorld* World, const FOnyxLightView& View, UTextureRenderTarget2D* LightTarget, UTextureRenderTarget2D* PositionTarget,
		UTextureRenderTarget2D* OwnGlowTarget, UTexture* Falloff, TConstArrayView<const UOnyxLight2DComponent*> Lights,
		TConstArrayView<FOnyxOccluderSegment> Occluders, float LightStrength, FOnyxLightShadowCaches* Caches = nullptr,
		uint32 OccluderVersion = 0, FOnyxLightBufferStats* OutStats = nullptr, TConstArrayView<FOnyxPixelLightQuad> PixelLights = {},
		UTextureRenderTarget2D* HaloTarget = nullptr);

	/** Builds the falloff atlas: one radial cell per falloff profile, side by side. */
	static UTexture* CreateFalloffTexture(UObject* Outer);

	/** Brightness at a distance from the light, 0 at its centre to 1 at its reach. */
	static float EvaluateFalloff(EOnyxLightFalloff Falloff, float Distance);

	/** Cone edges fade over this many degrees instead of cutting hard. */
	static constexpr float ConeSoftness = 12.f;
	/** Falloff profiles in the atlas, and the size of each cell. */
	static constexpr int32 FalloffCells = 4;
	static constexpr int32 FalloffCellSize = 128;
};
