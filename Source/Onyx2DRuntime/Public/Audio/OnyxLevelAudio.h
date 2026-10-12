// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxLevelAudio.generated.h"

class UBillboardComponent;
class USoundBase;

/**
 * A level's music and ambience. Starts them when the level starts (crossfading from the previous level's tracks, or
 * continuing when they are the same). Camera Zones can override both while the player is inside. One per level.
 */
UCLASS(meta = (DisplayName = "Onyx Level Audio"))
class ONYX2DRUNTIME_API AOnyxLevelAudio : public AActor
{
	GENERATED_BODY()

public:
	AOnyxLevelAudio();

	/** Background music (use a looping sound). Empty = silence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Audio")
	TObjectPtr<USoundBase> Music;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Audio", meta = (ClampMin = "0", ClampMax = "2"))
	float MusicVolume = 1.f;

	/** Background loop under the music: birds, wind, rain, a cave hum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Audio")
	TObjectPtr<USoundBase> Ambience;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Audio", meta = (ClampMin = "0", ClampMax = "2"))
	float AmbienceVolume = 0.8f;

	/** Seconds to fade in (and to crossfade from the previous level's tracks). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Audio", meta = (ClampMin = "0", Units = "s"))
	float FadeTime = 1.5f;

protected:
	virtual void BeginPlay() override;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Icon;
#endif
};
