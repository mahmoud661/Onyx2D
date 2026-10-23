// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OnyxSaveSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OnyxScoreSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxCoinsChangedSignature, int32, Coins);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnyxItemsChangedSignature, FName, Item, int32, Count);

/**
 * What the player has collected: coins, and named items such as keys. Lives on the game instance, so it lasts across
 * levels. Pickups and the Give action add to it; Trigger Zones that need an item read and use it up. Kept in the save.
 */
UCLASS()
class ONYX2DRUNTIME_API UOnyxScoreSubsystem : public UGameInstanceSubsystem, public IOnyxSaveable
{
	GENERATED_BODY()

public:
	static UOnyxScoreSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// IOnyxSaveable
	virtual FString GetSaveKey() const override { return TEXT("Onyx.Score"); }
	virtual void WriteSave(FArchive& Archive) override;
	virtual void ReadSave(FArchive& Archive) override;

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Score")
	void AddCoins(int32 Amount);

	/** Takes Amount coins if there are that many. Returns false (and takes none) if not. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Score")
	bool SpendCoins(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Score")
	void ResetCoins();

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Score")
	int32 GetCoins() const { return Coins; }

	/** Gives Count of a named item ("Key", "RedGem"). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Items")
	void AddItem(FName Item, int32 Count = 1);

	/** Takes Count of an item if there are that many. Returns false (and takes none) if not. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Items")
	bool UseItem(FName Item, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Items")
	int32 GetItemCount(FName Item) const { return Items.FindRef(Item); }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Items")
	bool HasItem(FName Item, int32 Count = 1) const { return GetItemCount(Item) >= Count; }

	/** Every item held, with its count. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Items")
	TMap<FName, int32> GetItems() const { return Items; }

	/** Empties coins and items (a new game). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Score")
	void ResetAll();

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Score")
	FOnyxCoinsChangedSignature OnCoinsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Items")
	FOnyxItemsChangedSignature OnItemsChanged;

private:
	void SetItemCount(FName Item, int32 Count);

	int32 Coins = 0;
	TMap<FName, int32> Items;
};
