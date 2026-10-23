// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OnyxSeeThroughSubsystem.generated.h"

/**
 * From above (Top Down levels), the player is never lost behind a tree or a house: a prop standing in front of the
 * player (lower on the screen) that hides part of it turns see-through, its art pixels dropping out in an even pattern,
 * and comes back when the player walks out from behind it. Onyx2D's lit sprite material draws the pattern (custom
 * primitive data at Onyx2D::SeeThroughDataIndex). Project Settings > Plugins > Onyx2D > See-Through Props turns it off;
 * an actor tagged Onyx.NoSeeThrough never fades.
 */
UCLASS()
class ONYX2DRUNTIME_API UOnyxSeeThroughSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** How see-through an actor is now (0 not at all). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Draw Order")
	float GetSeeThrough(const AActor* Actor) const;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** The props that can hide the player: every actor in the Props layer with pictures (looked for again now and then). */
	void GatherProps();
	static void Apply(AActor* Actor, float Amount);

	TArray<TWeakObjectPtr<AActor>> Props;
	TMap<TWeakObjectPtr<AActor>, float> Faded;
	float SinceGather = 0.f;
};
