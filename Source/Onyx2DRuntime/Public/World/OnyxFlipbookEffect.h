// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxFlipbookEffect.generated.h"

class UPaperFlipbook;
class UPaperFlipbookComponent;

/** Plays a flipbook once where it is spawned and removes itself: hit sparks, pickup sparkles, enemy death puffs. */
UCLASS(NotPlaceable, meta = (DisplayName = "Onyx Flipbook Effect"))
class ONYX2DRUNTIME_API AOnyxFlipbookEffect : public AActor
{
	GENERATED_BODY()

public:
	AOnyxFlipbookEffect();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperFlipbookComponent> Sprite;

	/** Spawns an effect that plays Flipbook once (null when there is nothing to play). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Effects", meta = (WorldContext = "WorldContext"))
	static AOnyxFlipbookEffect* Spawn(UObject* WorldContext, UPaperFlipbook* Flipbook, FVector Location, float Scale = 1.f);

private:
	UFUNCTION()
	void HandleFinished();
};
