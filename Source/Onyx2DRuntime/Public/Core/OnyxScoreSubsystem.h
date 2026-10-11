// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OnyxScoreSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxCoinsChangedSignature, int32, Coins);

/** Run-wide collectibles (coins). Lives on the game instance, so it survives level changes. */
UCLASS()
class ONYX2DRUNTIME_API UOnyxScoreSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UOnyxScoreSubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Score")
	void AddCoins(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Score")
	void ResetCoins();

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Score")
	int32 GetCoins() const { return Coins; }

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Score")
	FOnyxCoinsChangedSignature OnCoinsChanged;

private:
	int32 Coins = 0;
};
