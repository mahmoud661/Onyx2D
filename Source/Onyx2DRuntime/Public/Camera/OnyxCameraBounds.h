// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxCameraBounds.generated.h"

class UBoxComponent;
class UOnyxCameraProfile;
class UOnyxLightingProfile;
class USoundBase;

/**
 * Camera Zone: while the player is inside this box (XZ) the camera can be confined to it, switch to another
 * Camera Profile (zoom out in a big room...), frame extra actors (a boss) and change the lighting mood
 * (enter a cave and it gets dark) and the music / ambience. Everything blends smoothly.
 */
UCLASS(meta = (DisplayName = "Onyx Camera Zone"))
class ONYX2DRUNTIME_API AOnyxCameraBounds : public AActor
{
	GENERATED_BODY()

public:
	AOnyxCameraBounds();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Area;

	/** When zones overlap, the higher priority wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	int32 Priority = 0;

	/** Keep the view inside this box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	bool bConfine = true;

	/** Camera setup to use while inside (empty = keep the camera's own). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	TObjectPtr<UOnyxCameraProfile> Profile;

	/** Seconds to blend into this zone's profile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (ClampMin = "0"))
	float BlendTime = 0.8f;

	/** Also keep these on screen while the player is inside (boss, door that opens...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	TArray<TObjectPtr<AActor>> FocusActors;

	/** Lighting mood while inside (empty = keep the level's). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Lighting")
	TObjectPtr<UOnyxLightingProfile> Lighting;

	/** Seconds to fade into and out of this zone's lighting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Lighting", meta = (ClampMin = "0"))
	float LightingBlendTime = 1.5f;

	/** Music while the player is inside (empty = keep the level's). Crossfades in and back out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Audio")
	TObjectPtr<USoundBase> Music;

	/** Ambience loop while the player is inside (cave drips, wind), replacing the level's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Audio")
	TObjectPtr<USoundBase> Ambience;

	/** Seconds of crossfade when entering or leaving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Audio", meta = (ClampMin = "0", Units = "s"))
	float AudioFadeTime = 1.5f;

	/** Does this zone change anything about the camera (confine, profile, framing)? */
	bool AffectsCamera() const { return bConfine || Profile != nullptr || !FocusActors.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	FBox GetBounds() const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	bool ContainsPoint(const FVector& Point) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
