// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxCollisionMerge.h"

TArray<FIntRect> FOnyxCollisionMerge::MergeRectangles(const TArray<bool>& Cells, int32 Width, int32 Height)
{
	TArray<FIntRect> Result;
	if (Width <= 0 || Height <= 0 || Cells.Num() != Width * Height)
	{
		return Result;
	}
	TArray<bool> Used;
	Used.Init(false, Cells.Num());
	auto Free = [&](int32 X, int32 Y) { const int32 Index = Y * Width + X; return Cells[Index] && !Used[Index]; };

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			if (!Free(X, Y))
			{
				continue;
			}
			// Widest run on this row...
			int32 EndX = X + 1;
			while (EndX < Width && Free(EndX, Y))
			{
				++EndX;
			}
			// ...then grow down while the whole span is free.
			int32 EndY = Y + 1;
			while (EndY < Height)
			{
				bool bRowFree = true;
				for (int32 SpanX = X; SpanX < EndX && bRowFree; ++SpanX)
				{
					bRowFree = Free(SpanX, EndY);
				}
				if (!bRowFree)
				{
					break;
				}
				++EndY;
			}
			for (int32 FillY = Y; FillY < EndY; ++FillY)
			{
				for (int32 FillX = X; FillX < EndX; ++FillX)
				{
					Used[FillY * Width + FillX] = true;
				}
			}
			Result.Add(FIntRect(X, Y, EndX, EndY));
		}
	}
	return Result;
}

TArray<FIntRect> FOnyxCollisionMerge::MergeRows(const TArray<bool>& Cells, int32 Width, int32 Height)
{
	TArray<FIntRect> Result;
	if (Width <= 0 || Height <= 0 || Cells.Num() != Width * Height)
	{
		return Result;
	}
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			if (!Cells[Y * Width + X])
			{
				continue;
			}
			int32 EndX = X + 1;
			while (EndX < Width && Cells[Y * Width + EndX])
			{
				++EndX;
			}
			Result.Add(FIntRect(X, Y, EndX, Y + 1));
			X = EndX - 1;
		}
	}
	return Result;
}
