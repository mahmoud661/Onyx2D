// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "OnyxLight2DComponent.generated.h"

class UTexture2D;

/** The shape light comes from. */
UENUM(BlueprintType)
enum class EOnyxLightShape : uint8
{
	/** From a point, all around: a torch, a candle, an orb. */
	Point,
	/** From a point, in a cone along the component's +X (Cone Angle): a lamp shade, a searchlight, a beam of torchlight. */
	Spot,
	/** From a line (Length, along +X): a neon tube, a lava crack, a glowing rune strip. Radius is how far it reaches past it. */
	Line,
	/** From a rectangle (Size): a window, a lit doorway, a glowing panel. Radius is the soft border around it. */
	Area
};

/** How brightness fades from the light to its reach. */
UENUM(BlueprintType)
enum class EOnyxLightFalloff : uint8
{
	/** A warm core easing out to the edge: the all-rounder. */
	Smooth,
	/** A gentle glow that fades early, no edge at all: an aura, an orb, mist. */
	Soft,
	/** A hot core inside a crisp pool: a torch, a lamp on a wall. */
	Sharp,
	/** Even light with a short soft edge: a window patch, a spotlight's disc. */
	Even
};

/**
 * A real 2D light (not a 3D light shining on flat cards): drawn into the level's 2D light buffer from a point, a cone,
 * a line or a rectangle, clipped by 2D shadows from tiles and shadow casters, and applied to every lit sprite. A Pattern
 * texture shapes and colours it (stained glass, window bars, leaves). Add it to anything (torch, lamp, projectile, the
 * player).
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light 2D")
	EOnyxLightShape Shape = EOnyxLightShape::Point;

	/** Spot: cone width in degrees, pointing along the component's +X (360 lights all around, like a point). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light 2D", meta = (ClampMin = "2", ClampMax = "360",
		EditCondition = "Shape == EOnyxLightShape::Spot || Shape == EOnyxLightShape::Point", EditConditionHides))
	float ConeAngle = 360.f;

	/** Line: its length along the component's +X, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Light 2D", meta = (ClampMin = "0", UIMax = "2000",
		EditCondition = "Shape == EOnyxLightShape::Line", EditConditionHides))
	float Length = 200.f;

	/** Area: its width and height, cm (X along the component's +X). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Light 2D", meta = (EditCondition = "Shape == EOnyxLightShape::Area", EditConditionHides))
	FVector2D Size = FVector2D(160.f, 220.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light 2D")
	EOnyxLightFalloff Falloff = EOnyxLightFalloff::Smooth;

	/**
	 * Point and Spot: a picture the light shines through, covering its whole reach and turning with the component:
	 * stained glass, window bars, leaves, a grate. It replaces the falloff, so paint the fade into it (black is no light).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light 2D", meta = (EditCondition = "Shape == EOnyxLightShape::Point || Shape == EOnyxLightShape::Spot"))
	TObjectPtr<UTexture2D> Pattern;

	/** Blocked by tiles and Onyx Shadow Casters (Line and Area: from the point of the light nearest each place it lights). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light 2D")
	bool bCastShadows = true;

	/** Height above the sprites, as a fraction of the radius: low = grazing light, strong normal-map relief. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light 2D", meta = (ClampMin = "0.02", ClampMax = "2"))
	float Height = 0.25f;

	/**
	 * A character's own glow (or the flash of its own blade): it lights the world around them, while sprites guarded from
	 * washing out (Onyx Character's Light Guard) turn it away, so a hero carrying light isn't washed out by it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light 2D")
	bool bOwnGlow = false;

	/** Multiplier driven by flicker / pulse animation and gameplay (1 = as set). */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "Light 2D")
	float IntensityScale = 1.f;

	/** Lit when the level starts (switch it with Set Light Enabled). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light 2D")
	bool bStartEnabled = true;

	/** Turns the light on or off, fading over FadeTime seconds (0 = at once). */
	UFUNCTION(BlueprintCallable, Category = "Light 2D")
	void SetLightEnabled(bool bEnabled, float FadeTime = 0.25f);

	UFUNCTION(BlueprintCallable, Category = "Light 2D")
	void ToggleLight(float FadeTime = 0.25f) { SetLightEnabled(!IsLightEnabled(), FadeTime); }

	UFUNCTION(BlueprintPure, Category = "Light 2D")
	bool IsLightEnabled() const { return bEnabledTarget; }

	/** 0 = off, 1 = on (in between while fading). */
	float GetEnabledAlpha() const { return EnabledAlpha; }

	/** Final brightness this frame. */
	float GetEffectiveIntensity() const { return Intensity * IntensityScale * EnabledAlpha; }
	/** Light position on the 2D plane (X right, Y up). */
	FVector2D GetPlanePosition() const;
	/** Cone direction on the 2D plane (and the way a Line runs, a pattern faces, an Area's width lies). */
	FVector2D GetPlaneDirection() const;
	/** Half the shape's extent along its direction and across it, cm (zero for points and spots). */
	FVector2D GetShapeHalfExtent() const;
	/** How far from its position the light can reach at all, cm. */
	float GetReach() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

private:
	float EnabledAlpha = 1.f;
	float FadeSpeed = 0.f;
	bool bEnabledTarget = true;
};
