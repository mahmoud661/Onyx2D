// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxAutotile.h"
#include "Farming/OnyxAutotileSet.h"
#include "Misc/Paths.h"

FIntPoint FOnyxAutotile::QuarterSource(uint8 Mask, int32 Q)
{
	// The quarter's side neighbours (left or right, above or below) and the diagonal between them.
	const bool bLeft = Q == 0 || Q == 2, bTop = Q < 2;
	const bool bSide = (Mask & (bLeft ? OnyxAutotile::West : OnyxAutotile::East)) != 0;
	const bool bUpDown = (Mask & (bTop ? OnyxAutotile::North : OnyxAutotile::South)) != 0;
	const uint8 Diagonal = bTop ? (bLeft ? OnyxAutotile::NorthWest : OnyxAutotile::NorthEast) : (bLeft ? OnyxAutotile::SouthWest : OnyxAutotile::SouthEast);
	const bool bDiagonal = (Mask & Diagonal) != 0;
	if (!bSide && !bUpDown)
	{
		// An outer corner: the 2x2 patch's corner.
		return FIntPoint(bLeft ? 0 : 24, bTop ? 16 : 40);
	}
	if (bSide && !bUpDown)
	{
		// The top or bottom edge: a left-half or right-half quarter of the patch's top or bottom row.
		return FIntPoint(bLeft ? 16 : 8, bTop ? 16 : 40);
	}
	if (!bSide && bUpDown)
	{
		// The left or right edge: a top-half or bottom-half quarter of the patch's side column.
		return FIntPoint(bLeft ? 0 : 24, bTop ? 32 : 24);
	}
	if (!bDiagonal)
	{
		// An inner corner.
		return FIntPoint(bLeft ? 16 : 24, bTop ? 0 : 8);
	}
	// Fill: the patch's middle, the quarter that lines up with this one.
	return FIntPoint(bLeft ? 16 : 8, bTop ? 32 : 24);
}

namespace
{
	void CopyOver(const FOnyxPixelCanvas& From, int32 FromX, int32 FromY, int32 W, int32 H, FOnyxPixelCanvas& To, int32 ToX, int32 ToY)
	{
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const FColor Color = From.Get(FromX + X, FromY + Y);
				if (Color.A >= 128)
				{
					To.Set(ToX + X, ToY + Y, FColor(Color.R, Color.G, Color.B, 255));
				}
			}
		}
	}

	uint32 CellHash(int32 X, int32 Y, int32 Seed)
	{
		uint32 H = uint32(X) * 374761393u + uint32(Y) * 668265263u + uint32(Seed) * 2246822519u;
		H = (H ^ (H >> 13)) * 1274126177u;
		return H ^ (H >> 16);
	}
}

void FOnyxAutotile::DrawTile(const FOnyxPixelCanvas& Block, uint8 Mask, FOnyxPixelCanvas& Target, int32 X, int32 Y)
{
	for (int32 Q = 0; Q < 4; ++Q)
	{
		const FIntPoint From = QuarterSource(Mask, Q);
		CopyOver(Block, From.X, From.Y, Quarter, Quarter, Target, X + (Q % 2) * Quarter, Y + (Q / 2) * Quarter);
	}
}

void FOnyxAutotile::DrawFill(const FOnyxPixelCanvas& Fill, FOnyxPixelCanvas& Target, int32 X, int32 Y)
{
	CopyOver(Fill, 0, 0, Tile, Tile, Target, X, Y);
}

void FOnyxAutotile::Paint(FOnyxPixelCanvas& Target, const FOnyxPixelCanvas& Block, const TArray<FOnyxPixelCanvas>& Fills, int32 Width, int32 Height,
	TFunctionRef<bool(int32, int32)> IsTerrain, int32 Seed)
{
	auto IsSame = [&IsTerrain, Width, Height](int32 X, int32 Y)
	{
		// Off the map counts as more of the same, so terrain runs cleanly to the edge.
		return X < 0 || Y < 0 || X >= Width || Y >= Height || IsTerrain(X, Y);
	};
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			if (!IsTerrain(X, Y))
			{
				continue;
			}
			const uint8 Mask = OnyxAutotile::MaskAt(X, Y, IsSame);
			if (IsFill(Mask) && !Fills.IsEmpty())
			{
				DrawFill(Fills[CellHash(X, Y, Seed) % uint32(Fills.Num())], Target, X * Tile, Y * Tile);
			}
			else
			{
				DrawTile(Block, Mask, Target, X * Tile, Y * Tile);
			}
		}
	}
}

void FOnyxAutotile::BuildSet(const FOnyxPixelCanvas& Block, const TArray<FOnyxPixelCanvas>& Fills, TArray<FOnyxPixelCanvas>& OutTiles, TArray<uint8>& OutMaskToTile)
{
	OutTiles.Reset();
	OutMaskToTile.Init(0, 256);
	// The plain fills first (the block's own middle when there are none).
	if (Fills.IsEmpty())
	{
		FOnyxPixelCanvas Plain(Tile, Tile);
		DrawTile(Block, 0xFF, Plain, 0, 0);
		OutTiles.Add(Plain);
	}
	for (const FOnyxPixelCanvas& Fill : Fills)
	{
		FOnyxPixelCanvas Plain(Tile, Tile);
		DrawFill(Fill, Plain, 0, 0);
		OutTiles.Add(Plain);
	}
	// Then one tile per distinct set of quarters.
	TMap<FString, int32> Known;
	for (int32 Mask = 0; Mask < 256; ++Mask)
	{
		if (IsFill(uint8(Mask)))
		{
			OutMaskToTile[Mask] = 0;
			continue;
		}
		FString Key;
		bool bAllFill = true;
		for (int32 Q = 0; Q < 4; ++Q)
		{
			const FIntPoint From = QuarterSource(uint8(Mask), Q);
			Key += FString::Printf(TEXT("%d,%d;"), From.X, From.Y);
			bAllFill &= From == QuarterSource(0xFF, Q);
		}
		if (bAllFill)
		{
			OutMaskToTile[Mask] = 0;
			continue;
		}
		if (const int32* Found = Known.Find(Key))
		{
			OutMaskToTile[Mask] = uint8(*Found);
			continue;
		}
		FOnyxPixelCanvas Made(Tile, Tile);
		DrawTile(Block, uint8(Mask), Made, 0, 0);
		const int32 Index = OutTiles.Add(Made);
		Known.Add(Key, Index);
		OutMaskToTile[Mask] = uint8(Index);
	}
}

TArray<FOnyxPixelCanvas> FOnyxAutotile::SplitStrip(const FOnyxPixelCanvas& Strip, int32 FrameWidth)
{
	TArray<FOnyxPixelCanvas> Frames;
	if (FrameWidth <= 0)
	{
		return Frames;
	}
	for (int32 X = 0; X + FrameWidth <= Strip.Size.X; X += FrameWidth)
	{
		Frames.Add(Strip.Crop(FIntRect(X, 0, X + FrameWidth, Strip.Size.Y)));
	}
	return Frames;
}

bool FOnyxAutotile::Load(const FString& Folder, const FString& Name, FOnyxPixelCanvas& OutBlock, TArray<FOnyxPixelCanvas>& OutFills)
{
	OutFills.Reset();
	if (!FOnyxPixelCanvas::Load(Folder / Name + TEXT(".png"), OutBlock) || OutBlock.Size != FIntPoint(32, 48))
	{
		return false;
	}
	FOnyxPixelCanvas FillStrip;
	if (FOnyxPixelCanvas::Load(Folder / Name + TEXT("_Fill.png"), FillStrip) && FillStrip.Size.Y == Tile)
	{
		OutFills = SplitStrip(FillStrip, Tile);
	}
	return true;
}
