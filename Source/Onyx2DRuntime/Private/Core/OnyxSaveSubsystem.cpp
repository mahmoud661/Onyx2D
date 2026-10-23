// Copyright Onyx2D. All Rights Reserved.

#include "Core/OnyxSaveSubsystem.h"
#include "Core/Onyx2DSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxSave, Log, All);

UOnyxSaveSubsystem* UOnyxSaveSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UOnyxSaveSubsystem>() : nullptr;
}

void UOnyxSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Picking up where the last save left off: read it now, hand it out as each saveable appears.
	if (UOnyx2DSettings::Get()->bContinueFromSave && HasSave())
	{
		LoadGame();
	}
}

void UOnyxSaveSubsystem::Deinitialize()
{
	Saveables.Reset();
	Pending.Reset();
	Super::Deinitialize();
}

void UOnyxSaveSubsystem::Register(UObject* Saveable)
{
	IOnyxSaveable* AsSaveable = Cast<IOnyxSaveable>(Saveable);
	if (!AsSaveable)
	{
		return;
	}
	Saveables.AddUnique(Saveable);
	const FString Key = AsSaveable->GetSaveKey();
	if (const FOnyxSaveBlob* Blob = Pending.Find(Key))
	{
		Apply(Saveable, *Blob);
		Pending.Remove(Key);
	}
}

void UOnyxSaveSubsystem::Unregister(UObject* Saveable)
{
	Saveables.Remove(Saveable);
}

void UOnyxSaveSubsystem::Apply(UObject* Saveable, const FOnyxSaveBlob& Blob)
{
	if (IOnyxSaveable* AsSaveable = Cast<IOnyxSaveable>(Saveable))
	{
		FMemoryReader Reader(Blob.Bytes, true);
		AsSaveable->ReadSave(Reader);
	}
}

bool UOnyxSaveSubsystem::SaveGame(const FString& Slot)
{
	UOnyxSaveGame* Save = Cast<UOnyxSaveGame>(UGameplayStatics::CreateSaveGameObject(UOnyxSaveGame::StaticClass()));
	if (!Save)
	{
		return false;
	}
	// What other levels left in the save stays in it.
	Save->Blobs = Pending;
	for (const TWeakObjectPtr<UObject>& Weak : Saveables)
	{
		IOnyxSaveable* AsSaveable = Cast<IOnyxSaveable>(Weak.Get());
		if (!AsSaveable)
		{
			continue;
		}
		FOnyxSaveBlob Blob;
		FMemoryWriter Writer(Blob.Bytes, true);
		AsSaveable->WriteSave(Writer);
		Save->Blobs.Add(AsSaveable->GetSaveKey(), MoveTemp(Blob));
	}
	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
	UE_LOG(LogOnyxSave, Display, TEXT("Onyx Save: %s %d entries to slot %s"), bSaved ? TEXT("saved") : TEXT("could not save"), Save->Blobs.Num(), *Slot);
	return bSaved;
}

bool UOnyxSaveSubsystem::LoadGame(const FString& Slot)
{
	const UOnyxSaveGame* Save = Cast<UOnyxSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Save)
	{
		return false;
	}
	Pending = Save->Blobs;
	for (const TWeakObjectPtr<UObject>& Weak : Saveables)
	{
		IOnyxSaveable* AsSaveable = Cast<IOnyxSaveable>(Weak.Get());
		if (!AsSaveable)
		{
			continue;
		}
		const FString Key = AsSaveable->GetSaveKey();
		if (const FOnyxSaveBlob* Blob = Pending.Find(Key))
		{
			Apply(Weak.Get(), *Blob);
			Pending.Remove(Key);
		}
	}
	UE_LOG(LogOnyxSave, Display, TEXT("Onyx Save: loaded %d entries from slot %s"), Save->Blobs.Num(), *Slot);
	return true;
}

bool UOnyxSaveSubsystem::HasSave(const FString& Slot) const
{
	return UGameplayStatics::DoesSaveGameExist(Slot, 0);
}

void UOnyxSaveSubsystem::DeleteSave(const FString& Slot)
{
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	Pending.Reset();
}
