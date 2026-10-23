// Copyright Onyx2D. All Rights Reserved.

#include "Farming/OnyxAutotileSet.h"
#include "PaperSprite.h"

UPaperSprite* UOnyxAutotileSet::GetTile(uint8 Mask, uint32 Variation) const
{
	if (!MaskToTile.IsValidIndex(Mask))
	{
		return nullptr;
	}
	int32 Index = MaskToTile[Mask];
	if (Index == 0 && FillCount > 1)
	{
		Index = int32(Variation % uint32(FillCount));
	}
	return Tiles.IsValidIndex(Index) ? Tiles[Index].Get() : nullptr;
}
