// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/Interface.h"
#include "OnyxSaveSubsystem.generated.h"

UINTERFACE(MinimalAPI)
class UOnyxSaveable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something that keeps its state in the save (an inventory, a farm field, the calendar). It registers with Onyx Save
 * while it exists, writes itself into an archive when the game saves and reads itself back when a save is loaded.
 */
class ONYX2DRUNTIME_API IOnyxSaveable
{
	GENERATED_BODY()

public:
	/** Its name in the save, the same every run (the actor's label or a fixed name). */
	virtual FString GetSaveKey() const = 0;
	virtual void WriteSave(FArchive& Archive) = 0;
	virtual void ReadSave(FArchive& Archive) = 0;
};

/** One saveable's bytes. */
USTRUCT()
struct FOnyxSaveBlob
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<uint8> Bytes;
};

/** The saved game: a block of bytes per saveable, by key. */
UCLASS()
class ONYX2DRUNTIME_API UOnyxSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	TMap<FString, FOnyxSaveBlob> Blobs;
};

/**
 * Saving and loading (Onyx Save): everything registered as saveable writes itself into one save slot; loading hands each
 * its bytes back, now or as soon as it registers (a level loaded after the save was read). Save on sleeping, load when
 * the game starts (Onyx2D settings: Continue From Save).
 */
UCLASS()
class ONYX2DRUNTIME_API UOnyxSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UOnyxSaveSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void Register(UObject* Saveable);
	void Unregister(UObject* Saveable);

	/** Writes every saveable into the slot. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Save")
	bool SaveGame(const FString& Slot = TEXT("Onyx"));

	/** Reads the slot and hands each saveable its state (false: no save there). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Save")
	bool LoadGame(const FString& Slot = TEXT("Onyx"));

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Save")
	bool HasSave(const FString& Slot = TEXT("Onyx")) const;

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Save")
	void DeleteSave(const FString& Slot = TEXT("Onyx"));

	/** A loaded save has state waiting for this key (its owner hasn't registered yet): it isn't a new game for it. */
	bool HasPending(const FString& Key) const { return Pending.Contains(Key); }

private:
	void Apply(UObject* Saveable, const FOnyxSaveBlob& Blob);

	TArray<TWeakObjectPtr<UObject>> Saveables;
	/** Read from a save but not yet handed out (its saveable hasn't registered). */
	TMap<FString, FOnyxSaveBlob> Pending;
};
