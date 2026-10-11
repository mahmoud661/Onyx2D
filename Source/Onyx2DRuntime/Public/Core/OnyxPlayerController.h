// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OnyxPlayerController.generated.h"

class UOnyxInputConfig;

/** Applies the Onyx input scheme and forwards Pause to FOnyxGameDelegates (UI listens). */
UCLASS()
class ONYX2DRUNTIME_API AOnyxPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Optional custom controls; the built-in default scheme is used when empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxInputConfig> InputConfig;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void HandlePause();
};
