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

bool UOnyxScoreSubsystem::SpendCoins(int32 Amount)
{
	if (Amount > Coins)
	{
		return false;
	}
	AddCoins(-Amount);
	return true;
}

void UOnyxScoreSubsystem::ResetCoins()
{
	Coins = 0;
	OnCoinsChanged.Broadcast(Coins);
}

void UOnyxScoreSubsystem::AddItem(FName Item, int32 Count)
{
	if (!Item.IsNone())
	{
		SetItemCount(Item, GetItemCount(Item) + Count);
	}
}

bool UOnyxScoreSubsystem::UseItem(FName Item, int32 Count)
{
	if (!HasItem(Item, Count))
	{
		return false;
	}
	SetItemCount(Item, GetItemCount(Item) - Count);
	return true;
}

void UOnyxScoreSubsystem::SetItemCount(FName Item, int32 Count)
{
	Count = FMath::Max(Count, 0);
	if (Count == 0)
	{
		Items.Remove(Item);
	}
	else
	{
		Items.Add(Item, Count);
	}
	OnItemsChanged.Broadcast(Item, Count);
}

void UOnyxScoreSubsystem::ResetAll()
{
	ResetCoins();
	TArray<FName> Held;
	Items.GetKeys(Held);
	for (const FName Item : Held)
	{
		SetItemCount(Item, 0);
	}
}

void UOnyxScoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UOnyxSaveSubsystem* Save = Collection.InitializeDependency<UOnyxSaveSubsystem>())
	{
		Save->Register(this);
	}
}

void UOnyxScoreSubsystem::Deinitialize()
{
	if (UOnyxSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UOnyxSaveSubsystem>())
	{
		Save->Unregister(this);
	}
	Super::Deinitialize();
}

void UOnyxScoreSubsystem::WriteSave(FArchive& Archive)
{
	Archive << Coins << Items;
}

void UOnyxScoreSubsystem::ReadSave(FArchive& Archive)
{
	Archive << Coins << Items;
	OnCoinsChanged.Broadcast(Coins);
}
