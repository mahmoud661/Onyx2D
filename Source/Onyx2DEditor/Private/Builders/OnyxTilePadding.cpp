// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxTilePadding.h"
#include "Builders/OnyxAssetUtils.h"
#include "Builders/OnyxNormalMaps.h"
#include "Engine/Texture2D.h"
#include "PaperTileSet.h"

namespace
{
	const TCHAR* PaddedSuffix = TEXT("_Padded");

	bool ReadPixels(UTexture2D* Texture, TArray<FColor>& OutPixels, FIntPoint& OutSize)
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

bool FOnyxTilePadding::IsPadded(const UPaperTileSet* TileSet)
{
	const UTexture2D* Sheet = TileSet ? TileSet->GetTileSheetTexture() : nullptr;
	return Sheet && Sheet->GetName().EndsWith(PaddedSuffix);
}

UTexture2D* FOnyxTilePadding::PadTexture(UTexture2D* Source, const UPaperTileSet* Layout, int32 Padding, bool bNormalMap)
{
	TArray<FColor> Pixels;
	FIntPoint Size;
	if (!ReadPixels(Source, Pixels, Size))
	{
		return nullptr;
	}
	const FIntPoint Tile = Layout->GetTileSize();
	const FIntMargin Margin = Layout->GetMargin();
	const FIntPoint Spacing = Layout->GetPerTileSpacing();
	const int32 Columns = Layout->GetTileCountX();
	const int32 Rows = Layout->GetTileCountY();
	if (Columns <= 0 || Rows <= 0)
	{
		return nullptr;
	}

	const FIntPoint Cell = Tile + FIntPoint(Padding * 2);
	const FIntPoint PaddedSize(Columns * Cell.X, Rows * Cell.Y);
	TArray<FColor> Padded;
	Padded.Init(FColor(0, 0, 0, 0), PaddedSize.X * PaddedSize.Y);
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			const FIntPoint SourceMin(Margin.Left + Column * (Tile.X + Spacing.X), Margin.Top + Row * (Tile.Y + Spacing.Y));
			const FIntPoint TargetMin(Column * Cell.X, Row * Cell.Y);
			// Every padded pixel copies the nearest pixel of its own tile (edges and corners are extruded).
			for (int32 Y = 0; Y < Cell.Y; ++Y)
			{
				for (int32 X = 0; X < Cell.X; ++X)
				{
					const int32 SourceX = FMath::Clamp(SourceMin.X + FMath::Clamp(X - Padding, 0, Tile.X - 1), 0, Size.X - 1);
					const int32 SourceY = FMath::Clamp(SourceMin.Y + FMath::Clamp(Y - Padding, 0, Tile.Y - 1), 0, Size.Y - 1);
					Padded[(TargetMin.Y + Y) * PaddedSize.X + TargetMin.X + X] = Pixels[SourceY * Size.X + SourceX];
				}
			}
		}
	}

	const FString Folder = FPackageName::GetLongPackagePath(Source->GetOutermost()->GetName());
	FString Name = Source->GetName();
	if (bNormalMap && Name.EndsWith(TEXT("_N")))
	{
		Name = Name.LeftChop(2) + PaddedSuffix + TEXT("_N"); // keep the "<sheet>_N" naming of normal maps
	}
	else
	{
		Name += PaddedSuffix;
	}
	UTexture2D* Texture = FOnyxAssetUtils::CreateOrReuse<UTexture2D>(Folder, Name);
	if (!Texture)
	{
		return nullptr;
	}
	Texture->Modify();
	Texture->PreEditChange(nullptr);
	Texture->Source.Init(PaddedSize.X, PaddedSize.Y, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Padded.GetData()));
	// Same settings as the original (pixel art colour sheet, or normal map).
	Texture->CompressionSettings = Source->CompressionSettings;
	Texture->SRGB = Source->SRGB;
	Texture->Filter = TF_Nearest;
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->LODGroup = TEXTUREGROUP_Pixels2D;
	Texture->NeverStream = true;
	Texture->PostEditChange();
	Texture->MarkPackageDirty();
	return Texture;
}

bool FOnyxTilePadding::PadTileSet(UPaperTileSet* TileSet, int32 Padding)
{
	UTexture2D* Sheet = TileSet ? TileSet->GetTileSheetTexture() : nullptr;
	if (!Sheet || Padding <= 0 || IsPadded(TileSet))
	{
		return Sheet != nullptr && IsPadded(TileSet);
	}
	UTexture2D* PaddedSheet = PadTexture(Sheet, TileSet, Padding, false);
	if (!PaddedSheet)
	{
		return false;
	}
	UTexture2D* NormalMap = Cast<UTexture2D>(FOnyxNormalMaps::GetAssigned(TileSet));
	UTexture2D* PaddedNormal = NormalMap ? PadTexture(NormalMap, TileSet, Padding, true) : nullptr;

	TileSet->Modify();
	TileSet->SetTileSheetTexture(PaddedSheet);
	TileSet->SetMargin(FIntMargin(Padding));
	TileSet->SetPerTileSpacing(FIntPoint(Padding * 2));
	TileSet->PostEditChange();
	TileSet->MarkPackageDirty();
	if (PaddedNormal)
	{
		FOnyxNormalMaps::Assign(TileSet, PaddedNormal);
	}

	TArray<UPackage*> Packages = { TileSet->GetPackage(), PaddedSheet->GetPackage() };
	if (PaddedNormal)
	{
		Packages.Add(PaddedNormal->GetPackage());
	}
	FOnyxAssetUtils::SavePackages(Packages);
	return true;
}
