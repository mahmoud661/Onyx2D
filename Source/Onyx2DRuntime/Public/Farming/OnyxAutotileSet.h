// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OnyxAutotileSet.generated.h"

class UPaperSprite;

/** Which neighbours of a cell are the same terrain, as bits of an autotile mask. */
namespace OnyxAutotile
{
	constexpr uint8 North = 1 << 0;
	constexpr uint8 NorthEast = 1 << 1;
	constexpr uint8 East = 1 << 2;
	constexpr uint8 SouthEast = 1 << 3;
	constexpr uint8 South = 1 << 4;
	constexpr uint8 SouthWest = 1 << 5;
	constexpr uint8 West = 1 << 6;
	constexpr uint8 NorthWest = 1 << 7;

	/** The mask of the cell at (X, Y), Y growing down the screen: IsSame(X, Y) says whether a cell is the terrain. */
	template <class FIsSame>
	uint8 MaskAt(int32 X, int32 Y, const FIsSame& IsSame)
	{
		uint8 Mask = 0;
		Mask |= IsSame(X, Y - 1) ? North : 0;
		Mask |= IsSame(X + 1, Y - 1) ? NorthEast : 0;
		Mask |= IsSame(X + 1, Y) ? East : 0;
		Mask |= IsSame(X + 1, Y + 1) ? SouthEast : 0;
		Mask |= IsSame(X, Y + 1) ? South : 0;
		Mask |= IsSame(X - 1, Y + 1) ? SouthWest : 0;
		Mask |= IsSame(X - 1, Y) ? West : 0;
		Mask |= IsSame(X - 1, Y - 1) ? NorthWest : 0;
		return Mask;
	}
}

/**
 * A terrain drawn as an autotile: one sprite for every way its edges and corners can meet its neighbours, picked by
 * which of the eight neighbours are the same terrain. Built from a drawn block by the editor (Onyx Autotile); used at run
 * time for terrain that changes as you play, such as tilled soil.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Onyx Autotile Set"))
class ONYX2DRUNTIME_API UOnyxAutotileSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Every distinct tile; the first FillCount are the plain fills (all neighbours the same terrain). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autotile")
	TArray<TObjectPtr<UPaperSprite>> Tiles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autotile")
	int32 FillCount = 1;

	/** For each of the 256 neighbour masks, the tile that draws it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autotile")
	TArray<uint8> MaskToTile;

	/** The tile for a mask; Variation picks among the plain fills so large areas don't repeat. */
	UPaperSprite* GetTile(uint8 Mask, uint32 Variation = 0) const;
};
