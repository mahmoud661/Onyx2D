// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OnyxSaveSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "OnyxCalendarSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxNewDaySignature, int32, Day);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxClockSignature, int32, Minute);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnyxPassedOutSignature);

/**
 * Days, seasons and the clock (Onyx Calendar), on when the Onyx2D settings' Use Calendar is ticked. The clock runs from
 * Day Start while the game plays (not while paused or in a conversation), sets Onyx Lighting's hour, and at Day End the
 * player passes out; sleeping starts the next day. Farm fields grow, shops restock and the like on On New Day. It is
 * kept in the save.
 */
UCLASS()
class ONYX2DRUNTIME_API UOnyxCalendarSubsystem : public UGameInstanceSubsystem, public FTickableGameObject, public IOnyxSaveable
{
	GENERATED_BODY()

public:
	static UOnyxCalendarSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The calendar is on (the settings) and its clock runs. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Calendar")
	bool IsRunning() const;

	/** Minutes after midnight of the day it is (past 1440 after midnight: the small hours of the next morning). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Calendar")
	int32 GetMinute() const { return FMath::FloorToInt(Clock); }

	/** The hour as a share of the day, 0 midnight, 0.5 noon (Onyx Lighting's Time Of Day). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Calendar")
	float GetDayFraction() const { return FMath::Frac(Clock / 1440.f); }

	/** The day of the season (from 1), the season (from 0) and the year (from 1). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Calendar")
	int32 GetDay() const { return Day; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Calendar")
	int32 GetSeason() const { return Season; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Calendar")
	int32 GetYear() const { return Year; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Calendar")
	FText GetSeasonName() const;

	/** "Spring 3". */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Calendar")
	FText GetDateText() const;

	/** "9:40 am", in the ten-minute steps the clock shows. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Calendar")
	FText GetTimeText() const;

	/** Ends the day: the next morning at Day Start (sleeping, or passing out). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Calendar")
	void StartNewDay();

	/** Sets the clock (minutes after midnight), for tests and scripted scenes. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Calendar")
	void SetMinute(int32 Minute);

	/** A new day began (after sleeping or passing out). */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Calendar")
	FOnyxNewDaySignature OnNewDay;

	/** The clock moved on ten minutes. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Calendar")
	FOnyxClockSignature OnClockChanged;

	/** Day End came and the player was still up. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Calendar")
	FOnyxPassedOutSignature OnPassedOut;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override { return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Always; }
	virtual bool IsTickableWhenPaused() const override { return false; }
	virtual UWorld* GetTickableGameObjectWorld() const override;

	// IOnyxSaveable
	virtual FString GetSaveKey() const override { return TEXT("Onyx.Calendar"); }
	virtual void WriteSave(FArchive& Archive) override;
	virtual void ReadSave(FArchive& Archive) override;

private:
	float Clock = 360.f;
	int32 Day = 1;
	int32 Season = 0;
	int32 Year = 1;
	bool bPassedOut = false;
};
