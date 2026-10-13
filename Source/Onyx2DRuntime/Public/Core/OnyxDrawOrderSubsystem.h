// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/Onyx2DTypes.h"
#include "OnyxDrawOrderSubsystem.generated.h"

/**
 * Gives every sprite in the world its own place in the draw order, so two sprites on the gameplay plane never fight
 * over the same pixels (z-fighting). Each actor gets a layer (tiles behind props, props behind items, items behind
 * characters, the player in front) and its own slot inside the layer; the sprites inside one actor follow their
 * Translucency Sort Priority. Onyx2D's sprite materials turn that into a tiny depth bias along the view direction, so
 * nothing moves on screen or in the world. Works in the editor and in the game, with nothing to set up.
 */
UCLASS()
class ONYX2DRUNTIME_API UOnyxDrawOrderSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UOnyxDrawOrderSubsystem* Get(const UObject* WorldContext);

	/** Recomputes the draw order of Actor's sprites (after its layer changed). Does nothing outside a 2D world. */
	static void RefreshActor(AActor* Actor);

	/** Recomputes the draw order of Actor's sprites. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Draw Order")
	void Refresh(AActor* Actor);

	/** Recomputes the draw order of every sprite in the world. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Draw Order")
	void RefreshAll();

	/** The layer an actor's sprites draw in: its Draw Order component's, else the one Onyx2D picks for it. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Draw Order")
	static EOnyxDrawLayer FindLayer(const AActor* Actor);

	/** The depth bias (cm, towards the camera) of a layer, a slot in it and a sprite's priority inside its actor. */
	static float GetBias(EOnyxDrawLayer Layer, int32 Slot, int32 ComponentPriority);

	// UTickableWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldComponentsUpdated(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual bool IsTickableWhenPaused() const override { return true; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** The actor's slot in its layer: its Draw Order component's order, else one handed out once and kept. */
	int32 FindSlot(const AActor* Actor, EOnyxDrawLayer Layer);
	void HandleActorAdded(AActor* Actor);

	TMap<TWeakObjectPtr<const AActor>, int32> Slots;
	int32 NextSlot[8] = {};
	/** Seconds since the last full pass. Blueprints rebuild their components when edited, which resets the bias. */
	float SinceRefresh = 0.f;
	FDelegateHandle SpawnedHandle;
	FDelegateHandle EditorAddedHandle;
	FDelegateHandle EditorMovedHandle;
};
