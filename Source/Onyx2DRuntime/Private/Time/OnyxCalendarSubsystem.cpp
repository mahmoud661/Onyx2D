// Copyright Onyx2D. All Rights Reserved.

#include "Time/OnyxCalendarSubsystem.h"
#include "Core/Onyx2DSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Logic/OnyxDialogueSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxCalendar, Log, All);

UOnyxCalendarSubsystem* UOnyxCalendarSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UOnyxCalendarSubsystem>() : nullptr;
}

void UOnyxCalendarSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Clock = float(UOnyx2DSettings::Get()->DayStartMinute);
	if (UOnyxSaveSubsystem* Save = Collection.InitializeDependency<UOnyxSaveSubsystem>())
	{
		Save->Register(this);
	}
}

void UOnyxCalendarSubsystem::Deinitialize()
{
	if (UOnyxSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UOnyxSaveSubsystem>())
	{
		Save->Unregister(this);
	}
	Super::Deinitialize();
}

bool UOnyxCalendarSubsystem::IsRunning() const
{
	return UOnyx2DSettings::Get()->bUseCalendar;
}

UWorld* UOnyxCalendarSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

TStatId UOnyxCalendarSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOnyxCalendarSubsystem, STATGROUP_Tickables);
}

void UOnyxCalendarSubsystem::Tick(float DeltaTime)
{
	const UWorld* World = GetTickableGameObjectWorld();
	if (!IsRunning() || !World || !World->IsGameWorld() || World->IsPaused())
	{
		return;
	}
	// Time stands still while someone talks.
	if (const UOnyxDialogueSubsystem* Dialogue = World->GetSubsystem<UOnyxDialogueSubsystem>(); Dialogue && Dialogue->IsTalking())
	{
		return;
	}
	const UOnyx2DSettings* Settings = UOnyx2DSettings::Get();
	if (bPassedOut)
	{
		return;
	}
	const int32 Before = GetMinute() / 10;
	Clock = FMath::Min(Clock + DeltaTime * Settings->GameMinutesPerSecond, float(Settings->DayEndMinute));
	if (GetMinute() / 10 != Before)
	{
		OnClockChanged.Broadcast(GetMinute());
	}
	if (Clock >= Settings->DayEndMinute)
	{
		// The clock stops until someone puts the player to bed (an Onyx Bed); with nobody listening, the day ends here.
		UE_LOG(LogOnyxCalendar, Display, TEXT("Onyx Calendar: passed out at the end of %s"), *GetDateText().ToString());
		bPassedOut = true;
		if (OnPassedOut.IsBound())
		{
			OnPassedOut.Broadcast();
		}
		else
		{
			StartNewDay();
		}
	}
}

void UOnyxCalendarSubsystem::StartNewDay()
{
	const UOnyx2DSettings* Settings = UOnyx2DSettings::Get();
	Clock = float(Settings->DayStartMinute);
	bPassedOut = false;
	if (++Day > FMath::Max(Settings->DaysPerSeason, 1))
	{
		Day = 1;
		if (++Season >= FMath::Max(Settings->SeasonNames.Num(), 1))
		{
			Season = 0;
			++Year;
		}
	}
	UE_LOG(LogOnyxCalendar, Display, TEXT("Onyx Calendar: a new day, %s"), *GetDateText().ToString());
	OnNewDay.Broadcast(Day);
	OnClockChanged.Broadcast(GetMinute());
}

void UOnyxCalendarSubsystem::SetMinute(int32 Minute)
{
	Clock = float(FMath::Max(Minute, 0));
	bPassedOut = false;
	OnClockChanged.Broadcast(GetMinute());
}

FText UOnyxCalendarSubsystem::GetSeasonName() const
{
	const TArray<FText>& Names = UOnyx2DSettings::Get()->SeasonNames;
	return Names.IsValidIndex(Season) ? Names[Season] : FText::AsNumber(Season + 1);
}

FText UOnyxCalendarSubsystem::GetDateText() const
{
	return FText::Format(INVTEXT("{0} {1}"), GetSeasonName(), FText::AsNumber(Day));
}

FText UOnyxCalendarSubsystem::GetTimeText() const
{
	const int32 Minute = (GetMinute() / 10) * 10;
	const int32 Hour24 = (Minute / 60) % 24;
	const int32 Hour12 = Hour24 % 12 == 0 ? 12 : Hour24 % 12;
	return FText::FromString(FString::Printf(TEXT("%d:%02d %s"), Hour12, Minute % 60, Hour24 < 12 ? TEXT("am") : TEXT("pm")));
}

void UOnyxCalendarSubsystem::WriteSave(FArchive& Archive)
{
	Archive << Clock << Day << Season << Year;
}

void UOnyxCalendarSubsystem::ReadSave(FArchive& Archive)
{
	Archive << Clock << Day << Season << Year;
}
