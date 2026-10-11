// Copyright Onyx2D. All Rights Reserved.

#include "Assets/OnyxImageAnalysis.h"
#include "Engine/Texture2D.h"

namespace
{
	constexpr uint8 VisibleAlpha = 8;
	constexpr int32 MinFrameSize = 8;
	/** Share of edge pixels that may be cut (effects such as sword slashes often touch the cell border). */
	constexpr float MaxCutRatio = 0.02f;

	struct FAlphaImage
	{
		TArray<uint8> Alpha;
		FIntPoint Size = FIntPoint::ZeroValue;

		bool At(int32 X, int32 Y) const { return Alpha[Y * Size.X + X] > VisibleAlpha; }
	};

	/** Visible pixel pairs that a cell edge every CellSize pixels would split (Axis 0 = X, 1 = Y). */
	int32 CountCuts(const FAlphaImage& Image, int32 Axis, int32 CellSize)
	{
		const int32 Length = Axis == 0 ? Image.Size.X : Image.Size.Y;
		const int32 Across = Axis == 0 ? Image.Size.Y : Image.Size.X;
		int32 Cuts = 0;
		for (int32 Edge = CellSize; Edge < Length; Edge += CellSize)
		{
			for (int32 Other = 0; Other < Across; ++Other)
			{
				const bool bBefore = Axis == 0 ? Image.At(Edge - 1, Other) : Image.At(Other, Edge - 1);
				const bool bAfter = Axis == 0 ? Image.At(Edge, Other) : Image.At(Other, Edge);
				Cuts += bBefore && bAfter ? 1 : 0;
			}
		}
		return Cuts;
	}

	/** True when cell edges every CellSize pixels (almost) never split visible pixels. */
	bool IsCleanCellSize(const FAlphaImage& Image, int32 Axis, int32 CellSize)
	{
		const int32 Length = Axis == 0 ? Image.Size.X : Image.Size.Y;
		const int32 Across = Axis == 0 ? Image.Size.Y : Image.Size.X;
		const int32 Edges = Length / CellSize - 1;
		return Edges <= 0 || CountCuts(Image, Axis, CellSize) <= FMath::CeilToInt(Edges * Across * MaxCutRatio);
	}

	bool StripHasContent(const FAlphaImage& Image, int32 Axis, int32 Start, int32 End)
	{
		const int32 Across = Axis == 0 ? Image.Size.Y : Image.Size.X;
		for (int32 Along = Start; Along < End; ++Along)
		{
			for (int32 Other = 0; Other < Across; ++Other)
			{
				if (Axis == 0 ? Image.At(Along, Other) : Image.At(Other, Along))
				{
					return true;
				}
			}
		}
		return false;
	}

	/** True when, at this cell size, cells come in pairs where one half is always empty (the real frame is twice as big). */
	bool HalvesAlwaysPaired(const FAlphaImage& Image, int32 Axis, int32 CellSize)
	{
		const int32 Length = Axis == 0 ? Image.Size.X : Image.Size.Y;
		if (Length % (CellSize * 2) != 0)
		{
			return false;
		}
		bool bAnyPair = false;
		for (int32 Start = 0; Start < Length; Start += CellSize * 2)
		{
			const bool bFirst = StripHasContent(Image, Axis, Start, Start + CellSize);
			const bool bSecond = StripHasContent(Image, Axis, Start + CellSize, Start + CellSize * 2);
			if (bFirst && bSecond)
			{
				return false;
			}
			bAnyPair |= bFirst || bSecond;
		}
		return bAnyPair && IsCleanCellSize(Image, Axis, CellSize * 2);
	}

	int32 DetectAxis(const FAlphaImage& Image, int32 Axis)
	{
		const int32 Length = Axis == 0 ? Image.Size.X : Image.Size.Y;
		for (int32 CellSize = MinFrameSize; CellSize < Length; ++CellSize)
		{
			if (Length % CellSize == 0 && IsCleanCellSize(Image, Axis, CellSize))
			{
				int32 Result = CellSize;
				while (HalvesAlwaysPaired(Image, Axis, Result))
				{
					Result *= 2;
				}
				return Result;
			}
		}
		return Length;
	}
}

bool FOnyxImageAnalysis::ReadAlpha(UTexture2D* Texture, TArray<uint8>& OutAlpha, FIntPoint& OutSize)
{
	if (!Texture || !Texture->Source.IsValid())
	{
		return false;
	}
	FTextureSource& Source = Texture->Source;
	const ETextureSourceFormat Format = Source.GetFormat();
	if (Format != TSF_BGRA8 && Format != TSF_G8)
	{
		return false;
	}
	const uint8* Pixels = Source.LockMipReadOnly(0);
	if (!Pixels)
	{
		return false;
	}
	OutSize = FIntPoint(Source.GetSizeX(), Source.GetSizeY());
	const int32 Count = OutSize.X * OutSize.Y;
	OutAlpha.SetNumUninitialized(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		OutAlpha[Index] = Format == TSF_BGRA8 ? Pixels[Index * 4 + 3] : 255;
	}
	Source.UnlockMip(0);
	return true;
}

TOptional<FIntPoint> FOnyxImageAnalysis::DetectFrameSize(UTexture2D* Texture)
{
	TArray<uint8> Alpha;
	FIntPoint Size;
	return ReadAlpha(Texture, Alpha, Size) ? DetectFrameSize(Alpha, Size) : TOptional<FIntPoint>();
}

TOptional<FIntPoint> FOnyxImageAnalysis::DetectFrameSize(const TArray<uint8>& Alpha, FIntPoint InSize)
{
	FAlphaImage Image{ Alpha, InSize };
	if (Image.Size.X < MinFrameSize || Image.Size.Y < MinFrameSize || Alpha.Num() != Image.Size.X * Image.Size.Y)
	{
		return {};
	}
	const FIntPoint Size(DetectAxis(Image, 0), DetectAxis(Image, 1));
	// No grid found (e.g. opaque backgrounds): let the caller keep its current value.
	return Size == Image.Size ? TOptional<FIntPoint>() : TOptional<FIntPoint>(Size);
}

TOptional<FIntRect> FOnyxImageAnalysis::VisibleBounds(UTexture2D* Texture, const TArray<FIntRect>& Frames)
{
	FAlphaImage Image;
	if (Frames.IsEmpty() || !ReadAlpha(Texture, Image.Alpha, Image.Size))
	{
		return {};
	}
	FIntRect Bounds(FIntPoint(MAX_int32), FIntPoint(MIN_int32));
	bool bFound = false;
	for (const FIntRect& Frame : Frames)
	{
		const FIntRect Clipped(Frame.Min.ComponentMax(FIntPoint::ZeroValue), Frame.Max.ComponentMin(Image.Size));
		for (int32 Y = Clipped.Min.Y; Y < Clipped.Max.Y; ++Y)
		{
			for (int32 X = Clipped.Min.X; X < Clipped.Max.X; ++X)
			{
				if (Image.At(X, Y))
				{
					const FIntPoint Local(X - Frame.Min.X, Y - Frame.Min.Y);
					Bounds.Min = Bounds.Min.ComponentMin(Local);
					Bounds.Max = Bounds.Max.ComponentMax(Local + FIntPoint(1, 1));
					bFound = true;
				}
			}
		}
	}
	return bFound ? TOptional<FIntRect>(Bounds) : TOptional<FIntRect>();
}

float FOnyxImageAnalysis::GetCoverage(UTexture2D* Texture, const FIntRect& Rect)
{
	FAlphaImage Image;
	if (!ReadAlpha(Texture, Image.Alpha, Image.Size))
	{
		return -1.f;
	}
	const FIntRect Clipped(Rect.Min.ComponentMax(FIntPoint::ZeroValue), Rect.Max.ComponentMin(Image.Size));
	const int32 Total = Clipped.Area();
	if (Total <= 0)
	{
		return 0.f;
	}
	int32 Visible = 0;
	for (int32 Y = Clipped.Min.Y; Y < Clipped.Max.Y; ++Y)
	{
		for (int32 X = Clipped.Min.X; X < Clipped.Max.X; ++X)
		{
			Visible += Image.At(X, Y) ? 1 : 0;
		}
	}
	return float(Visible) / float(Total);
}
