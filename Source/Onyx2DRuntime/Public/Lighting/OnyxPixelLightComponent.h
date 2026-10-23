// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Lighting/OnyxLightBufferRenderer.h"
#include "OnyxPixelLightComponent.generated.h"

class UOnyxPixelLightSet;
class UPaperFlipbookComponent;

/**
 * Light from chosen pixels of the character's art (a Pixel Light Set): only the blade lights the wall, shaped like the blade
 * in every frame of the swing, turning round when the character does. It follows whichever frame its sprite shows, so it
 * needs no keys or timing: frames whose art has none of those pixels give no light. No shadows (it is close to its source).
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Onyx Pixel Light"))
class ONYX2DRUNTIME_API UOnyxPixelLightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxPixelLightComponent();

	/** Which pixels give light and how it spreads, for every frame (made in the Moveset Editor or by FOnyxPixelLightBuilder). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Light")
	TObjectPtr<UOnyxPixelLightSet> LightSet;

	/** Multiplies the set's light (white: the pixels' own colours). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Pixel Light")
	FLinearColor Color = FLinearColor::White;

	/** How much it lights what is round it (walls, floor, other characters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Pixel Light", meta = (ClampMin = "0", UIMax = "8"))
	float Intensity = 1.5f;

	/**
	 * Glow in the air round the pixels, shaped like them: drawn over everything, past the sprite's outline and over the
	 * character itself, the way a white-hot blade looks through haze or a camera (0: none). Pair it with the sprite's
	 * Glowing Pixels (its glow mask) for pixels that burn.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Pixel Light", meta = (ClampMin = "0", UIMax = "4"))
	float Halo = 0.6f;

	/** Its owner's own light: a character with a Light Guard lights the world with it more than itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Light")
	bool bOwnGlow = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pixel Light")
	bool bStartEnabled = true;

	/** Turns it on or off, fading over FadeTime seconds (0: at once). */
	UFUNCTION(BlueprintCallable, Category = "Pixel Light")
	void SetLightEnabled(bool bEnabled, float FadeTime = 0.15f);

	UFUNCTION(BlueprintPure, Category = "Pixel Light")
	bool IsLightEnabled() const { return bEnabledTarget; }

	/** The sprite whose frames it follows (by default the owner's first flipbook). */
	UFUNCTION(BlueprintCallable, Category = "Pixel Light")
	void SetTarget(UPaperFlipbookComponent* Sprite);

	/** This frame's light, where the sprite is (false: none now). */
	bool GetLightQuad(FOnyxPixelLightQuad& Out) const;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TWeakObjectPtr<UPaperFlipbookComponent> Target;
	float EnabledAlpha = 1.f;
	float FadeSpeed = 0.f;
	bool bEnabledTarget = true;
};
