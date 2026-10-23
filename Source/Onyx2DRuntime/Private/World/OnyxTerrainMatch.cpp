// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxTerrainMatch.h"
#include "PaperTileSet.h"

namespace OnyxTerrainMatch
{
	/** The corner each of Paper2D's four terrain slots stands for. */
	constexpr FOnyxTerrainMatch::ECorner SlotCorners[4] = {
		FOnyxTerrainMatch::TopLeft, FOnyxTerrainMatch::BottomLeft, FOnyxTerrainMatch::BottomRight, FOnyxTerrainMatch::TopRight };
}

uint8 FOnyxTerrainMatch::GetCorners(const UPaperTileSet* TileSet, int32 TileIndex, int32 Terrain)
{
	const FPaperTileMetadata* Metadata = TileSet ? TileSet->GetTileMetadata(TileIndex) : nullptr;
	uint8 Corners = 0;
	if (Metadata && Terrain >= 0)
	{
		for (int32 Slot = 0; Slot < 4; ++Slot)
		{
			Corners |= Metadata->TerrainMembership[Slot] == uint8(Terrain) ? OnyxTerrainMatch::SlotCorners[Slot] : 0;
		}
	}
	return Corners;
}

int32 FOnyxTerrainMatch::FindTile(const UPaperTileSet* TileSet, int32 Terrain, uint8 WantedCorners)
{
	if (!TileSet || Terrain < 0 || Terrain >= TileSet->GetNumTerrains())
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
		const int32 Score = FMath::CountBits(Corners ^ WantedCorners) * 8 - FMath::CountBits(Corners & WantedCorners);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = TileIndex;
		}
	}
	return Best;
}

uint8 FOnyxTerrainMatch::CornersFromNeighbours(const bool (&N)[3][3])
{
	uint8 Corners = 0;
	Corners |= (N[0][0] && N[0][1] && N[1][0]) ? TopLeft : 0;
	Corners |= (N[0][2] && N[0][1] && N[1][2]) ? TopRight : 0;
	Corners |= (N[2][0] && N[2][1] && N[1][0]) ? BottomLeft : 0;
	Corners |= (N[2][2] && N[2][1] && N[1][2]) ? BottomRight : 0;
	return Corners;
}
