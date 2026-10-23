// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OnyxWorldSubsystem.generated.h"

class AOnyxCameraBounds;
class UPrimitiveComponent;

/** Registry of level pieces that gameplay systems query each frame (avoids actor iteration). */
UCLASS()
class ONYX2DRUNTIME_API UOnyxWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * One-way surfaces: any blocking primitive characters can jump up through and drop down from
	 * (one-way platform actors, one-way tiles...). Movement only depends on this abstraction.
	 */
	void RegisterOneWaySurface(UPrimitiveComponent* Surface);
	void UnregisterOneWaySurface(UPrimitiveComponent* Surface);
	bool IsOneWaySurface(const UPrimitiveComponent* Surface) const;
	const TArray<TWeakObjectPtr<UPrimitiveComponent>>& GetOneWaySurfaces() const { return OneWaySurfaces; }

	void RegisterCameraBounds(AOnyxCameraBounds* Bounds);
	void UnregisterCameraBounds(AOnyxCameraBounds* Bounds);

	/** Highest-priority camera zone containing Location, or null. */
	AOnyxCameraBounds* FindCameraBounds(const FVector& Location) const;
	/** Highest-priority zone containing Location that passes Filter (e.g. "changes the lighting"). */
	AOnyxCameraBounds* FindCameraBounds(const FVector& Location, TFunctionRef<bool(const AOnyxCameraBounds&)> Filter) const;

	/** Playable area contributed by tile maps etc. (camera confiner, falling out of the level). */
	void SetLevelBounds(const UObject* Owner, const FBox& Bounds);
	void ClearLevelBounds(const UObject* Owner);
	/** Union of every contributed area (invalid box when there is none). */
	FBox GetLevelBounds() const;

	/** The boss whose fight is on, or was last (the HUD shows its name and health). */
	void SetActiveBoss(class UOnyxBossComponent* Boss);
	class UOnyxBossComponent* GetActiveBoss() const;

private:
	TArray<TWeakObjectPtr<UPrimitiveComponent>> OneWaySurfaces;
	TArray<TWeakObjectPtr<AOnyxCameraBounds>> CameraBounds;
	TMap<TWeakObjectPtr<const UObject>, FBox> LevelBounds;
	TWeakObjectPtr<class UOnyxBossComponent> ActiveBoss;
};
