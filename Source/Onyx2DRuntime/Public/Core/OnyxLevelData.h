// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "Core/Onyx2DTypes.h"
#include "OnyxLevelData.generated.h"

class UWorld;

/**
 * A level's own Onyx2D settings, kept in its World Settings (Asset User Data > Onyx Level). The Onyx2D panel and
 * Set Up Level for 2D add it; a level without one plays the project's way (Project Settings > Onyx2D).
 */
UCLASS(meta = (DisplayName = "Onyx Level"))
class ONYX2DRUNTIME_API UOnyxLevelData : public UAssetUserData
{
	GENERATED_BODY()

public:
	/** How this level is played: seen from the side (a platformer) or from above (top down). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	EOnyxGameStyle GameStyle = EOnyxGameStyle::SideView;

	/** The level's Onyx settings (null when it has none). */
	static const UOnyxLevelData* Find(const UWorld* World);

	/** How a world is played: its level's style, else the project's. */
	static EOnyxGameStyle GetGameStyle(const UWorld* World);
};
