// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "Camera/OnyxCameraRig.h"
#include "Camera/OnyxCameraTypes.h"
#include "OnyxCameraComponent.generated.h"

class AOnyxCameraBounds;

/**
 * Side-view follow camera for 2D games (Cinemachine-style). Lives on the followed actor but moves in world space.
 * The settings come from, in order: a scripted override, the Camera Zone the target is in, the Profile asset,
 * or the inline Settings below, and changes blend smoothly.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent))
class ONYX2DRUNTIME_API UOnyxCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	UOnyxCameraComponent();

	/** Default framing used when no profile, zone or override applies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Camera")
	FOnyxCameraSettings Settings;

	/** Optional shared setup; replaces Settings when set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Camera")
	TObjectPtr<UOnyxCameraProfile> Profile;

	/** Blend time when going back to the default settings (zones use their own). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Camera", meta = (ClampMin = "0"))
	float DefaultBlendTime = 0.75f;

	/** Adds screen shake (0..1; the shake grows with trauma squared). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Camera")
	void AddTrauma(float Amount);

	/** Jumps straight to the framed position (after teleports / respawns). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Camera")
	void SnapToTarget();

	/** Forces a camera setup (cutscenes, boss arenas) until cleared. Wins over zones. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Camera")
	void PushOverride(UOnyxCameraProfile* InOverride, float BlendTime = 0.75f);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Camera")
	void ClearOverride(float BlendTime = 0.75f);

	/** The settings in use right now (after zones and blending). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Camera")
	FOnyxCameraSettings GetCurrentSettings() const { return Current; }

	/** Half of the visible area in cm (X, Z). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Camera")
	FVector2D GetViewHalfExtents() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Picks the settings the camera should move towards and starts a blend when they change. */
	void UpdateSettings(float DeltaTime, const AOnyxCameraBounds* Zone);
	const FOnyxCameraSettings& GetBaseSettings() const;
	const AOnyxCameraBounds* FindZone(const FOnyxCameraTarget& Target) const;
	void ApplyLens();
	FOnyxCameraTarget GetTarget() const;
	float GetAspectRatio() const;
	/** Viewport size in pixels (0 when there is no player viewport yet). */
	FIntPoint GetViewportPixels() const;
	/** View height actually used: ViewHeight, or the nearest whole-pixel zoom in pixel-perfect mode. */
	float GetEffectiveViewHeight() const;
	/** World size of one screen pixel at the gameplay plane (0 when unknown). */
	float GetWorldUnitsPerScreenPixel() const;
	/** Zone box the view must stay in (invalid when the zone doesn't confine or confining by zones is off). */
	FBox2D GetZoneArea(const AOnyxCameraBounds* Zone) const;
	/** Whole level (from the tile maps), the hard outer limit (invalid when confining by level is off). */
	FBox2D GetLevelArea() const;
	/** Zone first, then the level; switching zones blends the correction instead of jumping. */
	FVector2D Confine(const FVector2D& Framed, const FVector2D& ViewHalf, const AOnyxCameraBounds* Zone, float DeltaTime);
	void ApplyTransform(const FVector2D& Center, const FVector& ShakeOffset);

	FOnyxCameraFraming Framing;
	FOnyxCameraShake Shake;

	FOnyxCameraSettings Current;
	FOnyxCameraSettings BlendFrom;
	TWeakObjectPtr<const UObject> ActiveSource;
	float BlendElapsed = 0.f;
	float BlendDuration = 0.f;

	TWeakObjectPtr<const AOnyxCameraBounds> ConfineZone;
	FVector2D ConfineCorrection = FVector2D::ZeroVector;
	FVector2D ConfineBlendFrom = FVector2D::ZeroVector;
	float ConfineBlendElapsed = 0.f;
	float ConfineBlendDuration = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxCameraProfile> Override;
	float OverrideBlend = 0.75f;

	FDelegateHandle TraumaHandle;
};
