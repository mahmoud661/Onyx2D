// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OnyxCrop.generated.h"

class UOnyxItem;
class UPaperSprite;

/**
 * A crop: how it looks at each stage of growing, how many days each stage takes (watered days), and what it gives when
 * picked. Planted from Seeds items in an Onyx Farm Field.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Onyx Crop"))
class ONYX2DRUNTIME_API UOnyxCrop : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	FText DisplayName;

	/** Its look at each stage, from just sown to ready (the last). Bottom-pivoted, standing on the cell's lower edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	TArray<TObjectPtr<UPaperSprite>> Stages;

	/** Watered days each stage takes before the next (one entry per stage but the last; missing entries count as 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	TArray<int32> DaysPerStage;

	/** What picking it gives, and how many. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	TObjectPtr<UOnyxItem> Produce;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = "1"))
	int32 ProduceCount = 1;

	/** After picking, it grows back to ready in this many watered days (0: picking ends it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = "0"))
	int32 RegrowDays = 0;

	int32 GetReadyStage() const { return FMath::Max(Stages.Num() - 1, 0); }
	int32 GetDaysForStage(int32 Stage) const { return DaysPerStage.IsValidIndex(Stage) ? FMath::Max(DaysPerStage[Stage], 1) : 1; }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("OnyxCrop"), GetFName()); }
};
