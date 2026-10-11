// Copyright Onyx2D. All Rights Reserved.

#include "Combat/OnyxCombatLibrary.h"
#include "Combat/OnyxHealthComponent.h"
#include "GameFramework/Actor.h"

bool UOnyxCombatLibrary::ApplyDamage(AActor* Target, const FOnyxDamageInfo& Damage)
{
	if (!Target)
	{
		return false;
	}
	if (Target->Implements<UOnyxDamageable>())
	{
		return IOnyxDamageable::Execute_ReceiveDamage(Target, Damage);
	}
	if (UOnyxHealthComponent* Health = Target->FindComponentByClass<UOnyxHealthComponent>())
	{
		return Health->ApplyDamage(Damage);
	}
	return false;
}

uint8 UOnyxCombatLibrary::GetTeam(const AActor* Actor)
{
	if (Actor && Actor->Implements<UOnyxTeamMember>())
	{
		return IOnyxTeamMember::Execute_GetTeamId(Actor);
	}
	return OnyxTeams::Neutral;
}

bool UOnyxCombatLibrary::AreHostile(const AActor* A, const AActor* B)
{
	if (!A || !B || A == B)
	{
		return false;
	}
	const uint8 TeamA = GetTeam(A);
	const uint8 TeamB = GetTeam(B);
	return TeamA == OnyxTeams::Neutral || TeamA != TeamB;
}
