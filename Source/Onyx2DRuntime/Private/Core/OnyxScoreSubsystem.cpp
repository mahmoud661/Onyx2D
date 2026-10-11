// Copyright Onyx2D. All Rights Reserved.

#include "Core/OnyxScoreSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UOnyxScoreSubsystem* UOnyxScoreSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UOnyxScoreSubsystem>() : nullptr;
}

void UOnyxScoreSubsystem::AddCoins(int32 Amount)
{
	Coins = FMath::Max(Coins + Amount, 0);
	OnCoinsChanged.Broadcast(Coins);
}

void UOnyxScoreSubsystem::ResetCoins()
{
	Coins = 0;
	OnCoinsChanged.Broadcast(Coins);
}
