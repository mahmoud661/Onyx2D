// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxTerrains.h"
#include "Engine/Texture2D.h"
#include "PaperTileSet.h"
#include "World/OnyxTerrainMatch.h"

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
	return FOnyxTerrainMatch::GetCorners(TileSet, TileIndex, Terrain);
}

bool FOnyxTerrains::IsTerrainTile(const UPaperTileSet* TileSet, int32 TileIndex, int32 Terrain)
{
	return GetCorners(TileSet, TileIndex, Terrain) != 0;
}

int32 FOnyxTerrains::FindTile(const UPaperTileSet* TileSet, int32 Terrain, uint8 WantedCorners)
{
	return FOnyxTerrainMatch::FindTile(TileSet, Terrain, WantedCorners);
}

uint8 FOnyxTerrains::CornersFromNeighbours(const bool (&Neighbours)[3][3])
{
	return FOnyxTerrainMatch::CornersFromNeighbours(Neighbours);
}

int32 FOnyxTerrains::CreateFromArt(UPaperTileSet* TileSet, const FString& Name, const FColor& Background)
{
	UTexture2D* Sheet = TileSet ? TileSet->GetTileSheetTexture() : nullptr;
	if (!Sheet || !Sheet->Source.IsValid() || Sheet->Source.GetFormat() != TSF_BGRA8 || GetCount(TileSet) >= int32(NoTerrain))
	{
		return INDEX_NONE;
	}
	TArray64<uint8> Data;
	Sheet->Source.GetMipData(Data, 0);
	const FIntPoint SheetSize(Sheet->Source.GetSizeX(), Sheet->Source.GetSizeY());
	const FColor* Pixels = reinterpret_cast<const FColor*>(Data.GetData());
	auto IsArt = [&](int32 X, int32 Y)
	{
		if (X < 0 || Y < 0 || X >= SheetSize.X || Y >= SheetSize.Y)
		{
			return false;
		}
		const FColor& Pixel = Pixels[int64(Y) * SheetSize.X + X];
		return Pixel.A > 0 && !(Pixel.R == Background.R && Pixel.G == Background.G && Pixel.B == Background.B);
	};
	// Art, and not a lone speck in the empty part (a star in the sky).
	auto IsSolid = [&](int32 X, int32 Y)
	{
		if (!IsArt(X, Y))
		{
			return false;
		}
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				if ((DX != 0 || DY != 0) && IsArt(X + DX, Y + DY))
				{
					return true;
				}
			}
		}
		return false;
	};

	TileSet->Modify();
	const FIntPoint TileSize = TileSet->GetTileSize();
	const int32 Terrain = GetCount(TileSet);
	int32 Center = INDEX_NONE;
	TMap<int32, uint8> Found;
	for (int32 Tile = 0; Tile < TileSet->GetTileCount(); ++Tile)
	{
		FVector2D Corner;
		if (!TileSet->GetTileUV(Tile, Corner))
		{
			continue;
		}
		const FIntPoint Min(FMath::RoundToInt(Corner.X), FMath::RoundToInt(Corner.Y));
		// A 3 x 3 patch just inside each corner of the tile; mostly art = the corner is terrain.
		auto Inside = [&](int32 StartX, int32 StartY)
		{
			int32 Count = 0;
			for (int32 Y = 0; Y < 3; ++Y)
			{
				for (int32 X = 0; X < 3; ++X)
				{
					Count += IsSolid(StartX + X, StartY + Y) ? 1 : 0;
				}
			}
			return Count >= 5;
		};
		uint8 Corners = 0;
		Corners |= Inside(Min.X, Min.Y) ? TopLeft : 0;
		Corners |= Inside(Min.X + TileSize.X - 3, Min.Y) ? TopRight : 0;
		Corners |= Inside(Min.X, Min.Y + TileSize.Y - 3) ? BottomLeft : 0;
		Corners |= Inside(Min.X + TileSize.X - 3, Min.Y + TileSize.Y - 3) ? BottomRight : 0;
		if (Corners != 0)
		{
			Found.Add(Tile, Corners);
			Center = (Corners == All && Center == INDEX_NONE) ? Tile : Center;
		}
	}
	if (Found.IsEmpty())
	{
		return INDEX_NONE;
	}
	FPaperTileSetTerrain Description;
	Description.TerrainName = Name;
	Description.CenterTileIndex = Center != INDEX_NONE ? Center : Found.CreateConstIterator().Key();
	if (!TileSet->AddTerrainDescription(Description))
	{
		return INDEX_NONE;
	}
	for (const TPair<int32, uint8>& Pair : Found)
	{
		SetCorners(TileSet, Pair.Key, Terrain, Pair.Value);
	}
	TileSet->PostEditChange();
	TileSet->MarkPackageDirty();
	return Terrain;
}
