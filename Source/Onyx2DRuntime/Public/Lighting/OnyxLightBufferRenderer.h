// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "Lighting/OnyxVisibility2D.h"

class UOnyxLight2DComponent;
class UTexture;
class UTextureRenderTarget2D;
class UWorld;

/** What the light buffer is drawn for: a camera and the size of the screen it fills. */
struct FOnyxLightView
{
	FMinimalViewInfo POV;
	FIntPoint ViewportSize = FIntPoint::ZeroValue;

	bool IsValid() const { return ViewportSize.X > 0 && ViewportSize.Y > 0; }
};

/**
 * Draws the screen-space 2D light buffers, the way 2D renderers (e.g. Unity's) do it:
 *  - Light buffer (RGB): sum of every light's colour x falloff, clipped to its visibility polygon (2D shadows).
 *  - Position buffer (RGB): sum of (light screen UV + 1, light height) weighted by the light's brightness,
 *    so the composite can find each pixel's average light direction for normal-mapped relief.
 * Stateless: the caller owns the render targets and the falloff texture.
 */
class ONYX2DRUNTIME_API FOnyxLightBufferRenderer
{
public:
	static void Render(UWorld* World, const FOnyxLightView& View, UTextureRenderTarget2D* LightTarget, UTextureRenderTarget2D* PositionTarget,
		UTexture* Falloff, TConstArrayView<const UOnyxLight2DComponent*> Lights, TConstArrayView<FOnyxOccluderSegment> Occluders,
		float LightStrength);

	/** Builds the radial falloff texture: smooth, bright core, no hard rim. */
	static UTexture* CreateFalloffTexture(UObject* Outer);

	/** Cone edges fade over this many degrees instead of cutting hard. */
	static constexpr float ConeSoftness = 12.f;
};
