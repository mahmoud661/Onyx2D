// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxAfterimage.generated.h"

class UPaperSprite;
class UPaperSpriteComponent;

/** A fading copy of a sprite left behind (a dash's trail). Spawn one with Spawn; it goes by itself. */
UCLASS(NotPlaceable, meta = (DisplayName = "Onyx Afterimage"))
class ONYX2DRUNTIME_API AOnyxAfterimage : public AActor
{
	GENERATED_BODY()

public:
	AOnyxAfterimage();

	/** Leaves Sprite at Transform, tinted Color (its alpha is where the fade starts), gone after Lifetime seconds. */
	static AOnyxAfterimage* Spawn(UObject* WorldContext, UPaperSprite* Sprite, const FTransform& Transform, const FLinearColor& Color, float Lifetime);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperSpriteComponent> Image;

	virtual void Tick(float DeltaSeconds) override;

private:
	FLinearColor Color = FLinearColor::White;
	float Lifetime = 0.25f;
	float Age = 0.f;
};
