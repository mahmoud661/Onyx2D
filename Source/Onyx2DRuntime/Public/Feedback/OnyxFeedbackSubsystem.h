// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "OnyxFeedbackSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnyxTraumaSignature, float /*Amount*/);

/**
 * "Game feel" services: hit-stop, screen shake requests and sprite flashes.
 * Cameras subscribe to OnTrauma, so this subsystem does not depend on any camera class.
 */
UCLASS()
class ONYX2DRUNTIME_API UOnyxFeedbackSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UOnyxFeedbackSubsystem* Get(const UObject* WorldContextObject);

	virtual void Deinitialize() override;

	/** Slows global time for Duration real seconds. Overlapping requests extend the freeze. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Feedback")
	void HitStop(float Duration = 0.06f, float TimeDilation = 0.05f);

	/** Adds screen-shake trauma (0..1) to every listening camera. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Feedback")
	void AddTrauma(float Amount);

	/** Tints every sprite/flipbook component of Actor for Duration seconds. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Feedback")
	void FlashActor(AActor* Actor, FLinearColor Color = FLinearColor(1.f, 0.25f, 0.25f), float Duration = 0.12f);

	FOnyxTraumaSignature OnTrauma;

private:
	bool TickHitStop(float DeltaTime);

	FTSTicker::FDelegateHandle HitStopTicker;
	double HitStopEndTime = 0.0;
};
