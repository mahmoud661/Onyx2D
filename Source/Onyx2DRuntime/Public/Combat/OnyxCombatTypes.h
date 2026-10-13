// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "OnyxCombatTypes.generated.h"

/** Everything a receiver needs to react to a hit. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxDamageInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float Amount = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	TObjectPtr<AActor> Instigator = nullptr;

	/** Launch velocity applied to the victim (world space, XZ). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	FVector Knockback = FVector::ZeroVector;

	/** Free-form tag (e.g. "Spikes", "Slash") for reactions and sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	FName DamageType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bIgnoreInvulnerability = false;
};

/** Teams: 0 = neutral (hurts everyone), 1 = player, 2 = enemies. Any value works. */
namespace OnyxTeams
{
	constexpr uint8 Neutral = 0;
	constexpr uint8 Player = 1;
	constexpr uint8 Enemy = 2;
}

/** The standard teams, for pickers (a character's Team Id can be any number). */
UENUM(BlueprintType)
enum class EOnyxTeam : uint8
{
	/** Fights everyone. */
	Neutral = 0,
	/** The player's side. */
	Player = 1,
	/** Against the player. */
	Enemy = 2
};

UINTERFACE(MinimalAPI, BlueprintType)
class UOnyxDamageable : public UInterface
{
	GENERATED_BODY()
};

/** Anything that can be hit (characters, breakable crates, switches...). */
class ONYX2DRUNTIME_API IOnyxDamageable
{
	GENERATED_BODY()

public:
	/** Returns true when the damage was accepted. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Onyx2D|Combat")
	bool ReceiveDamage(const FOnyxDamageInfo& Damage);
};

UINTERFACE(MinimalAPI, BlueprintType)
class UOnyxTeamMember : public UInterface
{
	GENERATED_BODY()
};

/** Anything on a team. Attacks only hurt actors on other teams; Neutral (0) fights everyone. */
class ONYX2DRUNTIME_API IOnyxTeamMember
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Onyx2D|Combat")
	uint8 GetTeamId() const;
};
