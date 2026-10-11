// Copyright Onyx2D. All Rights Reserved.

#include "Assets/OnyxTerrains.h"
#include "PaperTileSet.h"

namespace
{
	constexpr uint8 NoTerrain = 0xFF;
	/** Paper2D membership slots, counter-clockwise from top-left. */
	constexpr FOnyxTerrains::ECorner SlotCorners[4] = {
		FOnyxTerrains::TopLeft, FOnyxTerrains::BottomLeft, FOnyxTerrains::BottomRight, FOnyxTerrains::TopRight };

	int32 TileIndexAt(const UPaperTileSet* TileSet, const FIntPoint& Tile)
	{
		return Tile.Y * TileSet->GetTileCountX() + Tile.X;
	}

	int32 CountBits(uint8 Value)
	{
		return FMath::CountBits(Value);
	}
}

int32 FOnyxTerrains::GetCount(const UPaperTileSet* TileSet)
{
	return TileSet ? TileSet->GetNumTerrains() : 0;
}

FString FOnyxTerrains::GetName(const UPaperTileSet* TileSet, int32 Terrain)
{
	return TileSet ? TileSet->GetTerrain(Terrain).TerrainName : FString();
}

void FOnyxTerrains::SetCorners(UPaperTileSet* TileSet, int32 TileIndex, int32 Terrain, uint8 Corners)
{
	if (FPaperTileMetadata* Metadata = TileSet->GetMutableTileMetadata(TileIndex))
	{
		for (int32 Slot = 0; Slot < 4; ++Slot)
		{
			if (Corners & SlotCorners[Slot])
			{
				Metadata->TerrainMembership[Slot] = uint8(Terrain);
			}
			else if (Metadata->TerrainMembership[Slot] == uint8(Terrain))
			{
				Metadata->TerrainMembership[Slot] = NoTerrain;
			}
		}
	}
}

int32 FOnyxTerrains::GetTemplateStride(const FIntPoint& SelectionSize, int32 TemplateTiles)
{
	if (SelectionSize.X != SelectionSize.Y)
	{
		return 0;
	}
	// TemplateTiles tiles per side, either touching (stride 1) or with a one-tile gap between them (stride 2).
	if (SelectionSize.X == TemplateTiles)
	{
		return 1;
	}
	return SelectionSize.X == TemplateTiles * 2 - 1 ? 2 : 0;
}

int32 FOnyxTerrains::CreateFromBlock(UPaperTileSet* TileSet, const FIntPoint& BlockMin, const FIntPoint& BlockSize, const FString& Name)
{
	const int32 Stride = GetTemplateStride(BlockSize, 3);
	if (!TileSet || Stride == 0 || GetCount(TileSet) >= int32(NoTerrain))
	{
		return INDEX_NONE;
	}
	TileSet->Modify();
	FPaperTileSetTerrain Terrain;
	Terrain.TerrainName = Name;
	Terrain.CenterTileIndex = TileIndexAt(TileSet, BlockMin + FIntPoint(Stride, Stride));
	if (!TileSet->AddTerrainDescription(Terrain))
	{
		return INDEX_NONE;
	}
	const int32 Index = GetCount(TileSet) - 1;
	for (int32 Row = 0; Row < 3; ++Row)
	{
		for (int32 Column = 0; Column < 3; ++Column)
		{
			// A corner of a block tile is inside the terrain when it points into the block.
			uint8 Corners = 0;
			Corners |= (Column > 0 && Row > 0) ? TopLeft : 0;
			Corners |= (Column < 2 && Row > 0) ? TopRight : 0;
			Corners |= (Column > 0 && Row < 2) ? BottomLeft : 0;
			Corners |= (Column < 2 && Row < 2) ? BottomRight : 0;
			SetCorners(TileSet, TileIndexAt(TileSet, BlockMin + FIntPoint(Column, Row) * Stride), Index, Corners);
		}
	}
	TileSet->PostEditChange();
	TileSet->MarkPackageDirty();
	return Index;
}

bool FOnyxTerrains::SetInnerCorners(UPaperTileSet* TileSet, int32 Terrain, const FIntPoint& BlockMin, const FIntPoint& BlockSize)
{
	const int32 Stride = GetTemplateStride(BlockSize, 2);
	if (!TileSet || Stride == 0 || Terrain < 0 || Terrain >= GetCount(TileSet))
	{
		return false;
	}
	TileSet->Modify();
	SetCorners(TileSet, TileIndexAt(TileSet, BlockMin + FIntPoint(0, 0) * Stride), Terrain, All & ~BottomRight);
	SetCorners(TileSet, TileIndexAt(TileSet, BlockMin + FIntPoint(1, 0) * Stride), Terrain, All & ~BottomLeft);
	SetCorners(TileSet, TileIndexAt(TileSet, BlockMin + FIntPoint(0, 1) * Stride), Terrain, All & ~TopRight);
	SetCorners(TileSet, TileIndexAt(TileSet, BlockMin + FIntPoint(1, 1) * Stride), Terrain, All & ~TopLeft);
	TileSet->PostEditChange();
	TileSet->MarkPackageDirty();
	return true;
}

uint8 FOnyxTerrains::GetCorners(const UPaperTileSet* TileSet, int32 TileIndex, int32 Terrain)
{
	const FPaperTileMetadata* Metadata = TileSet ? TileSet->GetTileMetadata(TileIndex) : nullptr;
	uint8 Corners = 0;
	if (Metadata && Terrain >= 0)
	{
		for (int32 Slot = 0; Slot < 4; ++Slot)
		{
			Corners |= Metadata->TerrainMembership[Slot] == uint8(Terrain) ? SlotCorners[Slot] : 0;
		}
	}
	return Corners;
}

bool FOnyxTerrains::IsTerrainTile(const UPaperTileSet* TileSet, int32 TileIndex, int32 Terrain)
{
	return GetCorners(TileSet, TileIndex, Terrain) != 0;
}

int32 FOnyxTerrains::FindTile(const UPaperTileSet* TileSet, int32 Terrain, uint8 WantedCorners)
{
	if (!TileSet || Terrain < 0 || Terrain >= GetCount(TileSet))
	{
		return INDEX_NONE;
	}
	// Closest corner pattern; ties go to the tile with more terrain (thin strips read as ground, not as gaps).
	int32 Best = INDEX_NONE;
	int32 BestScore = MAX_int32;
	for (int32 TileIndex = 0; TileIndex < TileSet->GetTileCount(); ++TileIndex)
	{
		const uint8 Corners = GetCorners(TileSet, TileIndex, Terrain);
		if (Corners == 0)
		{
			continue;
		}
		const int32 Score = CountBits(Corners ^ WantedCorners) * 8 - CountBits(Corners & WantedCorners);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = TileIndex;
		}
	}
	return Best;
}

uint8 FOnyxTerrains::CornersFromNeighbours(const bool (&N)[3][3])
{
	uint8 Corners = 0;
	Corners |= (N[0][0] && N[0][1] && N[1][0]) ? TopLeft : 0;
	Corners |= (N[0][2] && N[0][1] && N[1][2]) ? TopRight : 0;
	Corners |= (N[2][0] && N[2][1] && N[1][0]) ? BottomLeft : 0;
	Corners |= (N[2][2] && N[2][1] && N[1][2]) ? BottomRight : 0;
	return Corners;
}
