// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/Onyx2DTypes.h"
#include "Engine/DataAsset.h"
#include "OnyxHeldLook.generated.h"

class UPaperFlipbook;

/** One clip's frames of a held thing: what shows behind the character and what shows in front, frame for frame with it. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxHeldFrames
{
	GENERATED_BODY()

	/** Drawn under the character (a hoe raised behind the head, a can held out ahead when facing away). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Held Look")
	TObjectPtr<UPaperFlipbook> Behind;

	/** Drawn over it (the tool in the near hand, the water, the sparks). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Held Look")
	TObjectPtr<UPaperFlipbook> InFront;
};

/**
 * How a held thing looks in the hands while its clips play: a hoe, a watering can, a lantern, a note. Its flipbooks are drawn
 * frame for frame with the character's own (the same frame size and feet point), so the character's clips hold empty hands and
 * any look can go in them: give an item another look (a copper hoe, a gilded can, a festival skin) without touching the
 * character, or the character a new outfit without redrawing the tools.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Onyx Held Look"))
class ONYX2DRUNTIME_API UOnyxHeldLook : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Frames by role (Till, Water...), or by role and direction for top-down clips (Till_Down, Till_Side: see UOnyxMoveset). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Held Look")
	TMap<FName, FOnyxHeldFrames> Clips;

	/** The frames for a role shown in a direction; the plain role's when there are none for the direction. */
	const FOnyxHeldFrames* Find(FName Role, EOnyxDirection Direction) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("OnyxHeldLook"), GetFName()); }
};
