// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Lighting/OnyxLightBufferRenderer.h"
#include "Lighting/OnyxLightingTypes.h"
#include "Lighting/OnyxVisibility2D.h"
#include "Subsystems/WorldSubsystem.h"
#include "OnyxLightingSubsystem.generated.h"

class AOnyxLightingEnvironment;
class UOnyxLight2DComponent;
class UTexture;
class UTextureRenderTarget2D;

/**
 * The 2D lighting of a world:
 *  - registry of 2D lights and shadow occluders (tiles, shadow casters),
 *  - picks the visible lighting mood (scripted override > Camera Zone > level) and blends between moods,
 *  - every frame draws the screen-space light buffers that the Onyx Lighting composite applies to sprites.
 * Runs in the editor too (fed with the level viewport camera), so levels are edited with their real lighting.
 */
UCLASS()
class ONYX2DRUNTIME_API UOnyxLightingSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterEnvironment(AOnyxLightingEnvironment* Environment);
	void UnregisterEnvironment(AOnyxLightingEnvironment* Environment);

	void RegisterLight(UOnyxLight2DComponent* Light);
	void UnregisterLight(UOnyxLight2DComponent* Light);

	/** Replaces the occluder edges contributed by Owner (tile collision, a shadow caster...). */
	void SetOccluders(const UObject* Owner, TArray<FOnyxOccluderSegment>&& Segments);
	void ClearOccluders(const UObject* Owner);

	/** Editor only: the level viewport camera to light for (games use the player camera). */
	void SetEditorView(const FOnyxLightView& View) { EditorView = View; }

	/** Takes over the mood until ClearOverride (lightning, boss intro, "lights out" switch...). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Lighting")
	void PushOverride(UOnyxLightingProfile* Profile, float BlendTime = 1.f);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Lighting")
	void ClearOverride(float BlendTime = 1.f);

	/**
	 * Lights up the whole scene for a moment, then fades back (lightning, an explosion, a camera flash). Intensity is
	 * added to the ambient light; Duration is the fade back.
	 */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Lighting")
	void Flash(FLinearColor Color = FLinearColor(0.85f, 0.9f, 1.f), float Intensity = 1.5f, float Duration = 0.35f);

	/** The lighting subsystem of an object's world. */
	static UOnyxLightingSubsystem* Get(const UObject* WorldContext);

	/** The mood on screen right now (mid-blend values included). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Lighting")
	FOnyxLightingMood GetCurrentMood() const { return Current; }

	UTextureRenderTarget2D* GetLightBuffer() const { return LightTarget; }
	UTextureRenderTarget2D* GetPositionBuffer() const { return PositionTarget; }

	/** Light buffer resolution relative to the screen (light is soft: half resolution is plenty). */
	static constexpr float BufferScale = 0.5f;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	bool GetView(FOnyxLightView& OutView, FVector& OutViewTarget) const;
	void UpdateMood(float DeltaTime, const FVector& ViewTarget);
	/** Picks the wanted mood and its blend time; returns the object that asked for it. */
	const UObject* ChooseMood(const FVector& ViewTarget, FOnyxLightingMood& OutMood, float& OutBlendTime) const;
	void EnsureTargets(const FIntPoint& ViewportSize);
	const TArray<FOnyxOccluderSegment>& GetAllOccluders();

	TWeakObjectPtr<AOnyxLightingEnvironment> Environment;
	TArray<TWeakObjectPtr<UOnyxLight2DComponent>> Lights;
	TMap<TWeakObjectPtr<const UObject>, TArray<FOnyxOccluderSegment>> OccluderSets;
	TArray<FOnyxOccluderSegment> AllOccluders;
	bool bOccludersDirty = true;
	FOnyxLightView EditorView;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> LightTarget;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PositionTarget;

	UPROPERTY(Transient)
	TObjectPtr<UTexture> Falloff;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxLightingProfile> Override;
	float OverrideBlend = 1.f;
	float ClearBlend = 1.f;

	FOnyxLightingMood Current;
	FOnyxLightingMood BlendFrom;
	FLinearColor FlashColor = FLinearColor::White;
	float FlashIntensity = 0.f;
	float FlashDuration = 0.f;
	float FlashElapsed = 0.f;
	TWeakObjectPtr<const UObject> ActiveSource;
	float BlendElapsed = 0.f;
	float BlendDuration = 0.f;
	bool bHasMood = false;
};
