// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "OnyxLight2DComponent.generated.h"

/**
 * A real 2D light (not a 3D light shining on flat cards): drawn into the level's 2D light buffer as a soft
 * disc or cone, clipped by 2D shadows from tiles and shadow casters, and applied to every lit sprite.
 * Add it to anything (torch, lamp, projectile, the player).
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Onyx Light 2D"))
class ONYX2DRUNTIME_API UOnyxLight2DComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UOnyxLight2DComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Light 2D")
	FLinearColor Color = FLinearColor(1.f, 0.65f, 0.35f);

	/** Brightness at the centre (1 = lights a sprite to its true colour on its own). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Light 2D", meta = (ClampMin = "0", UIMax = "8"))
	float Intensity = 1.5f;

	/** Reach in world units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Light 2D", meta = (ClampMin = "1", UIMax = "3000"))
	float Radius = 600.f;

	/** Cone width in degrees: 360 = all around (point light), less = spot light pointing along the component's +X. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light 2D", meta = (ClampMin = "2", ClampMax = "360"))
	float ConeAngle = 360.f;

	/** Blocked by tiles and Onyx Shadow Casters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light 2D")
	bool bCastShadows = true;

	/** Height above the sprites, as a fraction of the radius: low = grazing light, strong normal-map relief. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light 2D", meta = (ClampMin = "0.02", ClampMax = "2"))
	float Height = 0.25f;

	/** Multiplier driven by flicker / pulse animation and gameplay (1 = as set). */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "Light 2D")
	float IntensityScale = 1.f;

	/** Final brightness this frame. */
	float GetEffectiveIntensity() const { return Intensity * IntensityScale; }
	/** Light position on the 2D plane (X right, Y up). */
	FVector2D GetPlanePosition() const;
	/** Cone direction on the 2D plane. */
	FVector2D GetPlaneDirection() const;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
};
