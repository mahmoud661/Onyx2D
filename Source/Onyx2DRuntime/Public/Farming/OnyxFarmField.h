// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OnyxSaveSubsystem.h"
#include "GameFramework/Actor.h"
#include "OnyxFarmField.generated.h"

class UOnyxAutotileSet;
class UOnyxCrop;
class UOnyxInventoryComponent;
class UPaperFlipbook;
class UPaperGroupedSpriteComponent;
class USoundBase;

/** One cell of a farm field: its soil and what grows in it. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxSoilCell
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soil")
	bool bTilled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soil")
	bool bWatered = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soil")
	TObjectPtr<UOnyxCrop> Crop;

	/** Its stage (0 just sown) and the watered days it has spent in it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soil")
	int32 Stage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soil")
	int32 Days = 0;
};

/**
 * A farm field (top down): a grid of soil cells laid on the ground from the actor's location (the field's top-left
 * corner). The hoe tills a cell, the watering can waters it, seeds sow a crop in tilled soil, and each new day (Onyx
 * Calendar) watered crops grow a day and the soil dries. Ready crops are picked with the scythe or the Interact input.
 * Tilled soil is drawn with autotile sets (dry and watered) so beds join up; crops stand in it, drawn in front of or
 * behind characters by their row. All of it is kept in the save.
 */
UCLASS(meta = (DisplayName = "Onyx Farm Field"))
class ONYX2DRUNTIME_API AOnyxFarmField : public AActor, public IOnyxSaveable
{
	GENERATED_BODY()

public:
	AOnyxFarmField();

	/** The field's size in cells, and a cell's size (cm; a tile). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Farm", meta = (ClampMin = "1", ClampMax = "256"))
	int32 Width = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Farm", meta = (ClampMin = "1", ClampMax = "256"))
	int32 Height = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Farm", meta = (ClampMin = "1"))
	float CellSize = 64.f;

	/** How tilled soil looks, dry and watered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Farm")
	TObjectPtr<UOnyxAutotileSet> DrySoil;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Farm")
	TObjectPtr<UOnyxAutotileSet> WetSoil;

	/** Which cells can be tilled (one byte per cell, row by row; empty: all of them). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Farm")
	TArray<uint8> Tillable;

	/** The soil, row by row (Width x Height); set it in the editor for a field that starts with beds or crops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Farm")
	TArray<FOnyxSoilCell> Cells;

	/** Played where the hoe bites, the can waters and a crop is picked (flipbooks, centred), and the sounds of it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Farm|Feel")
	TObjectPtr<UPaperFlipbook> TillEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Farm|Feel")
	TObjectPtr<UPaperFlipbook> WaterEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Farm|Feel")
	TObjectPtr<UPaperFlipbook> HarvestEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Farm|Feel")
	TObjectPtr<USoundBase> TillSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Farm|Feel")
	TObjectPtr<USoundBase> WaterSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Farm|Feel")
	TObjectPtr<USoundBase> HarvestSound;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D|Farm")
	TObjectPtr<UPaperGroupedSpriteComponent> Soil;

	/** The field under a world location (top down: X across, Z up the screen), and the cell there. */
	static AOnyxFarmField* FindAt(const UWorld* World, const FVector& Location, FIntPoint& OutCell);

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Farm")
	bool WorldToCell(const FVector& Location, FIntPoint& OutCell) const;

	/** A cell's middle in the world. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Farm")
	FVector GetCellCenter(FIntPoint Cell) const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Farm")
	bool IsInField(FIntPoint Cell) const { return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Width && Cell.Y < Height; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Farm")
	FOnyxSoilCell GetCell(FIntPoint Cell) const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Farm")
	bool IsReady(FIntPoint Cell) const;

	/** Tills a cell (true if it was tillable and not tilled yet). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Farm")
	bool Till(FIntPoint Cell);

	/** Waters a tilled cell (true if it was dry). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Farm")
	bool Water(FIntPoint Cell);

	/** Sows a crop in an empty tilled cell. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Farm")
	bool Plant(FIntPoint Cell, UOnyxCrop* Crop);

	/** Picks a ready crop into the inventory (true if one was picked). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Farm")
	bool Harvest(FIntPoint Cell, UOnyxInventoryComponent* Into);

	/** A day passes: watered crops grow, the soil dries. Called by the calendar's new day; call it yourself to test. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Farm")
	void AdvanceDay();

	/** Rain: every tilled cell is watered (call it at the start of a rainy day). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Farm")
	void WaterAll();

	// IOnyxSaveable
	virtual FString GetSaveKey() const override;
	virtual void WriteSave(FArchive& Archive) override;
	virtual void ReadSave(FArchive& Archive) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleNewDay(int32 Day);

	FOnyxSoilCell* At(FIntPoint Cell);
	const FOnyxSoilCell* At(FIntPoint Cell) const;
	bool CanTill(FIntPoint Cell) const;
	void EnsureCells();
	void RefreshSoil();
	void RefreshCrops();
	void Feedback(FIntPoint Cell, UPaperFlipbook* Effect, USoundBase* Sound) const;

	/** One actor per row of crops, so characters stand in front of or behind each row as they should. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> CropRows;
};
