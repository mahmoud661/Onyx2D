// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/OnyxCombatTypes.h"
#include "OnyxCombatLibrary.generated.h"

UCLASS()
class ONYX2DRUNTIME_API UOnyxCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Routes damage to IOnyxDamageable, or to a Onyx Health component. Returns true if accepted. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Combat")
	static bool ApplyDamage(AActor* Target, const FOnyxDamageInfo& Damage);

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Combat")
	static uint8 GetTeam(const AActor* Actor);

	/** True when A is allowed to hurt B (different teams; neutral hurts everyone). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Combat")
	static bool AreHostile(const AActor* A, const AActor* B);
};
