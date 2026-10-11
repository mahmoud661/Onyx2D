// Copyright Onyx2D. All Rights Reserved.

#include "Assets/OnyxFramePacker.h"
#include "Assets/OnyxAssetFactory.h"
#include "Engine/Texture2D.h"

namespace
{
	/** Splits "player-run-12" into ("player-run-", 12) for natural sorting. */
	TPair<FString, int32> SplitTrailingNumber(const FString& Name)
	{
		int32 Index = Name.Len();
		while (Index > 0 && FChar::IsDigit(Name[Index - 1]))
		{
			--Index;
		}
		const int32 Number = Index < Name.Len() ? FCString::Atoi(*Name.Mid(Index)) : -1;
		return { Name.Left(Index), Number };
	}

	bool ReadBGRA(UTexture2D* Texture, TArray<FColor>& OutPixels, FIntPoint& OutSize)
	{
		if (!Texture || !Texture->Source.IsValid() || Texture->Source.GetFormat() != TSF_BGRA8)
		{
			return false;
		}
		const uint8* Data = Texture->Source.LockMipReadOnly(0);
		if (!Data)
		{
			return false;
		}
		OutSize = FIntPoint(Texture->Source.GetSizeX(), Texture->Source.GetSizeY());
		OutPixels.SetNumUninitialized(OutSize.X * OutSize.Y);
		FMemory::Memcpy(OutPixels.GetData(), Data, OutPixels.Num() * sizeof(FColor));
		Texture->Source.UnlockMip(0);
		return true;
	}
}

void FOnyxFramePacker::SortFrames(TArray<UTexture2D*>& Frames)
{
	Frames.Remove(nullptr);
	Frames.Sort([](const UTexture2D& A, const UTexture2D& B)
	{
		const TPair<FString, int32> KeyA = SplitTrailingNumber(A.GetName());
		const TPair<FString, int32> KeyB = SplitTrailingNumber(B.GetName());
		return KeyA.Key != KeyB.Key ? KeyA.Key < KeyB.Key : KeyA.Value < KeyB.Value;
	});
}

UTexture2D* FOnyxFramePacker::PackStrip(const TArray<UTexture2D*>& InFrames, const FString& Folder, const FString& Name, FIntPoint& OutFrameSize, FText& OutError)
{
	TArray<UTexture2D*> Frames = InFrames;
	SortFrames(Frames);
	if (Frames.IsEmpty())
	{
		OutError = INVTEXT("Select the frame images first.");
		return nullptr;
	}
	if (!FOnyxAssetFactory::IsValidContentFolder(Folder))
	{
		OutError = INVTEXT("The folder must start with /Game.");
		return nullptr;
	}

	TArray<TArray<FColor>> Pixels;
	TArray<FIntPoint> Sizes;
	FIntPoint Cell(0, 0);
	for (UTexture2D* Frame : Frames)
	{
		TArray<FColor>& FramePixels = Pixels.AddDefaulted_GetRef();
		FIntPoint& Size = Sizes.AddDefaulted_GetRef();
		if (!ReadBGRA(Frame, FramePixels, Size))
		{
			OutError = FText::Format(INVTEXT("{0} can't be read (needs an 8-bit RGBA source image)."), FText::FromString(Frame->GetName()));
			return nullptr;
		}
		Cell = Cell.ComponentMax(Size);
	}

	const FIntPoint StripSize(Cell.X * Frames.Num(), Cell.Y);
	TArray<FColor> Strip;
	Strip.Init(FColor(0, 0, 0, 0), StripSize.X * StripSize.Y);
	for (int32 FrameIndex = 0; FrameIndex < Frames.Num(); ++FrameIndex)
	{
		// Bottom-centre in the cell: feet stay on the same line when frame sizes differ.
		const FIntPoint Size = Sizes[FrameIndex];
		const FIntPoint Offset(FrameIndex * Cell.X + (Cell.X - Size.X) / 2, Cell.Y - Size.Y);
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			FMemory::Memcpy(&Strip[(Offset.Y + Y) * StripSize.X + Offset.X], &Pixels[FrameIndex][Y * Size.X], Size.X * sizeof(FColor));
		}
	}

	UTexture2D* Texture = FOnyxAssetFactory::CreateTexture(Folder, Name, Strip, StripSize);
	if (!Texture)
	{
		OutError = INVTEXT("That name is already used by a different kind of asset.");
		return nullptr;
	}
	FOnyxAssetFactory::SavePackages({ Texture->GetPackage() });
	OutFrameSize = Cell;
	return Texture;
}
