// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxPixelCanvas.h"
#include "Builders/OnyxPaletteSwap.h"
#include "ImageUtils.h"

FOnyxPixelCanvas::FOnyxPixelCanvas(int32 Width, int32 Height)
	: Size(Width, Height)
{
	Pixels.Init(FColor(0, 0, 0, 0), Width * Height);
}

void FOnyxPixelCanvas::Set(int32 X, int32 Y, const FColor& Color)
{
	if (IsInside(X, Y))
	{
		Pixels[Y * Size.X + X] = Color;
	}
}

FColor FOnyxPixelCanvas::Get(int32 X, int32 Y) const
{
	return IsInside(X, Y) ? Pixels[Y * Size.X + X] : FColor(0, 0, 0, 0);
}

void FOnyxPixelCanvas::Outline(const FColor& Color)
{
	TArray<FIntPoint> Edge;
	for (int32 Y = 0; Y < Size.Y; ++Y)
	{
		for (int32 X = 0; X < Size.X; ++X)
		{
			if (Get(X, Y).A == 0 && (Get(X - 1, Y).A > 0 || Get(X + 1, Y).A > 0 || Get(X, Y - 1).A > 0 || Get(X, Y + 1).A > 0))
			{
				Edge.Add(FIntPoint(X, Y));
			}
		}
	}
	for (const FIntPoint& Cell : Edge)
	{
		Set(Cell.X, Cell.Y, Color);
	}
}

FOnyxPixelCanvas FOnyxPixelCanvas::Strip(const TArray<FOnyxPixelCanvas>& Frames)
{
	FOnyxPixelCanvas Result(Frames[0].Size.X * Frames.Num(), Frames[0].Size.Y);
	for (int32 Index = 0; Index < Frames.Num(); ++Index)
	{
		for (int32 Y = 0; Y < Frames[Index].Size.Y; ++Y)
		{
			for (int32 X = 0; X < Frames[Index].Size.X; ++X)
			{
				Result.Set(Index * Frames[Index].Size.X + X, Y, Frames[Index].Get(X, Y));
			}
		}
	}
	return Result;
}

bool FOnyxPixelCanvas::Load(const FString& File, FOnyxPixelCanvas& Out)
{
	FImage Image;
	if (!FImageUtils::LoadImage(*File, Image))
	{
		return false;
	}
	Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	Out = FOnyxPixelCanvas(Image.SizeX, Image.SizeY);
	const TArrayView64<FColor> Source = Image.AsBGRA8();
	for (int64 Index = 0; Index < Source.Num() && Index < Out.Pixels.Num(); ++Index)
	{
		Out.Pixels[Index] = Source[Index];
	}
	return true;
}

FOnyxPixelCanvas FOnyxPixelCanvas::Crop(const FIntRect& Rect) const
{
	FOnyxPixelCanvas Result(Rect.Width(), Rect.Height());
	for (int32 Y = 0; Y < Result.Size.Y; ++Y)
	{
		for (int32 X = 0; X < Result.Size.X; ++X)
		{
			Result.Set(X, Y, Get(Rect.Min.X + X, Rect.Min.Y + Y));
		}
	}
	return Result;
}

void FOnyxPixelCanvas::Paste(const FOnyxPixelCanvas& Source, FIntPoint At)
{
	for (int32 Y = 0; Y < Source.Size.Y; ++Y)
	{
		for (int32 X = 0; X < Source.Size.X; ++X)
		{
			Set(At.X + X, At.Y + Y, Source.Get(X, Y));
		}
	}
}

void FOnyxPixelCanvas::Recolour(const FOnyxPaletteSwap& Swap)
{
	Swap.Apply(Pixels);
}

int32 FOnyxPixelCanvas::EmptyRowsBelow() const
{
	for (int32 Y = Size.Y - 1; Y >= 0; --Y)
	{
		for (int32 X = 0; X < Size.X; ++X)
		{
			if (Get(X, Y).A > 0)
			{
				return Size.Y - 1 - Y;
			}
		}
	}
	return Size.Y;
}

FIntRect FOnyxPixelCanvas::BodyBounds() const
{
	TArray<int32> Counts;
	Counts.Init(0, Size.X);
	int32 Densest = INDEX_NONE;
	for (int32 X = 0; X < Size.X; ++X)
	{
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			Counts[X] += Get(X, Y).A > 0 ? 1 : 0;
		}
		Densest = (Densest == INDEX_NONE || Counts[X] > Counts[Densest]) ? X : Densest;
	}
	if (Densest == INDEX_NONE || Counts[Densest] == 0)
	{
		return FIntRect();
	}
	// Out from the densest column while columns stay dense (a single thin column inside the body is allowed).
	const int32 Dense = FMath::Max(2, FMath::CeilToInt(Counts[Densest] * 0.4f));
	auto IsDense = [&](int32 X) { return X >= 0 && X < Size.X && Counts[X] >= Dense; };
	int32 Left = Densest;
	while (IsDense(Left - 1) || (IsDense(Left - 2) && Left - 2 >= 0))
	{
		Left -= IsDense(Left - 1) ? 1 : 2;
	}
	int32 Right = Densest;
	while (IsDense(Right + 1) || IsDense(Right + 2))
	{
		Right += IsDense(Right + 1) ? 1 : 2;
	}
	int32 Top = Size.Y;
	int32 Bottom = -1;
	for (int32 X = Left; X <= Right; ++X)
	{
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			if (Get(X, Y).A > 0)
			{
				Top = FMath::Min(Top, Y);
				Bottom = FMath::Max(Bottom, Y);
			}
		}
	}
	return Bottom >= Top ? FIntRect(Left, Top, Right + 1, Bottom + 1) : FIntRect();
}

void FOnyxPixelCanvas::Overlay(const FOnyxPixelCanvas& Other)
{
	for (int32 Y = 0; Y < FMath::Min(Size.Y, Other.Size.Y); ++Y)
	{
		for (int32 X = 0; X < FMath::Min(Size.X, Other.Size.X); ++X)
		{
			if (Other.Get(X, Y).A > 0)
			{
				Set(X, Y, Other.Get(X, Y));
			}
		}
	}
}
