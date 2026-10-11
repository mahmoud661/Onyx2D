// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "OnyxShadowCasterComponent.generated.h"

/**
 * Makes an object block Onyx 2D lights: a box or convex outline (local X right, Z up) that throws a 2D
 * shadow behind it. Follows the owner when it moves (characters, crates, moving platforms).
 * Tiles don't need it: Onyx Tile Collision already casts shadows from solid tiles.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Onyx Shadow Caster 2D"))
class ONYX2DRUNTIME_API UOnyxShadowCasterComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UOnyxShadowCasterComponent();

	/** Use a box of BoxSize instead of a custom outline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Shadow")
	bool bUseBox = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Shadow", meta = (EditCondition = "bUseBox"))
	FVector2D BoxSize = FVector2D(60.f, 120.f);

	/** Convex outline in local X (right) / Z (up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Shadow", meta = (EditCondition = "!bUseBox"))
	TArray<FVector2D> Outline;

	/** How far light soaks into the shape: large = the object itself stays lit and only shadows what is behind it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Shadow", meta = (ClampMin = "0"))
	float LightPenetration = 200.f;

	/** Re-sends the outline to the lighting (called automatically when the owner moves). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Shadow")
	void UpdateOccluder();

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	TArray<FVector2D> GetOutline() const;
};
