// Copyright Onyx2D. All Rights Reserved.

#include "Assets/OnyxNormalMaps.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Assets/OnyxAssetFactory.h"
#include "Engine/Texture2D.h"
#include "PaperSprite.h"
#include "PaperTileSet.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr uint8 OpaqueAlpha = 128;
	const FName AdditionalTexturesName(TEXT("AdditionalSourceTextures"));

	/** Pixel neighbourhood rules: tile cells are closed boxes, sprite sheets end in transparency. */
	struct FRegionSampler
	{
		const FIntPoint Size;
		const FOnyxPixelGrid& Grid;

		/** Neighbour coordinate clamped into the pixel's own tile; false when it falls off a sprite sheet. */
		bool Resolve(int32 X, int32 Y, int32 DX, int32 DY, int32& OutX, int32& OutY) const
		{
			const FIntRect Cell = Grid.CellOf(X, Y, Size);
			OutX = X + DX;
			OutY = Y + DY;
			if (Grid.IsValid())
			{
				OutX = FMath::Clamp(OutX, Cell.Min.X, Cell.Max.X - 1);
				OutY = FMath::Clamp(OutY, Cell.Min.Y, Cell.Max.Y - 1);
				return true;
			}
			return OutX >= 0 && OutY >= 0 && OutX < Size.X && OutY < Size.Y;
		}
	};

	/** Chamfer distance (in pixels) from every opaque pixel to the nearest transparent one. */
	TArray<float> DistanceToEdge(const TArray<bool>& Opaque, const FRegionSampler& Region)
	{
		const FIntPoint Size = Region.Size;
		const float Far = float(Size.X + Size.Y);
		TArray<float> Distance;
		Distance.SetNumUninitialized(Opaque.Num());
		for (int32 Index = 0; Index < Opaque.Num(); ++Index)
		{
			Distance[Index] = Opaque[Index] ? Far : 0.f;
		}

		struct FStep { int32 DX; int32 DY; float Cost; };
		static const FStep Forward[] = { { -1, 0, 1.f }, { 0, -1, 1.f }, { -1, -1, UE_SQRT_2 }, { 1, -1, UE_SQRT_2 } };
		static const FStep Backward[] = { { 1, 0, 1.f }, { 0, 1, 1.f }, { 1, 1, UE_SQRT_2 }, { -1, 1, UE_SQRT_2 } };

		auto Relax = [&](int32 X, int32 Y, const FStep (&Steps)[4])
		{
			float& Here = Distance[Y * Size.X + X];
			if (Here == 0.f)
			{
				return;
			}
			for (const FStep& Step : Steps)
			{
				int32 NX, NY;
				if (!Region.Resolve(X, Y, Step.DX, Step.DY, NX, NY))
				{
					Here = FMath::Min(Here, Step.Cost); // off the sheet counts as transparent
				}
				else if (NX != X || NY != Y) // clamped onto itself at a tile border: no information
				{
					Here = FMath::Min(Here, Distance[NY * Size.X + NX] + Step.Cost);
				}
			}
		};
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			for (int32 X = 0; X < Size.X; ++X)
			{
				Relax(X, Y, Forward);
			}
		}
		for (int32 Y = Size.Y - 1; Y >= 0; --Y)
		{
			for (int32 X = Size.X - 1; X >= 0; --X)
			{
				Relax(X, Y, Backward);
			}
		}
		return Distance;
	}

	float Luma(const FColor& Color)
	{
		return (0.2126f * Color.R + 0.7152f * Color.G + 0.0722f * Color.B) / 255.f;
	}
}

FIntRect FOnyxPixelGrid::CellOf(int32 X, int32 Y, const FIntPoint& ImageSize) const
{
	if (!IsValid())
	{
		return FIntRect(FIntPoint::ZeroValue, ImageSize);
	}
	const FIntPoint Step = CellSize + Spacing;
	const int32 Column = FMath::Max(0, (X - Margin.X) / Step.X);
	const int32 Row = FMath::Max(0, (Y - Margin.Y) / Step.Y);
	const FIntPoint Min(Margin.X + Column * Step.X, Margin.Y + Row * Step.Y);
	return FIntRect(Min, (Min + CellSize).ComponentMin(ImageSize));
}

TArray<FColor> FOnyxNormalMapGenerator::Generate(const TArray<FColor>& Pixels, const FIntPoint& Size, const FOnyxNormalMapSettings& Settings,
	const FOnyxPixelGrid& Grid)
{
	TArray<FColor> Result;
	const int32 Count = Size.X * Size.Y;
	if (Size.X <= 0 || Size.Y <= 0 || Pixels.Num() != Count)
	{
		return Result;
	}
	const FRegionSampler Region{ Size, Grid };

	TArray<bool> Opaque;
	Opaque.SetNumUninitialized(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Opaque[Index] = Pixels[Index].A >= OpaqueAlpha;
	}

	// 1. Relief in art pixels: a rounded bevel along the silhouette plus brightness detail.
	const TArray<float> Distance = DistanceToEdge(Opaque, Region);
	TArray<float> Height;
	Height.SetNumZeroed(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!Opaque[Index])
		{
			continue;
		}
		const float T = Settings.BevelWidth > 0.f ? FMath::Clamp((Distance[Index] - 0.5f) / Settings.BevelWidth, 0.f, 1.f) : 1.f;
		const float Rounded = FMath::Sqrt(1.f - FMath::Square(1.f - T)); // quarter circle: soft, pillow-like edges
		Height[Index] = Rounded * Settings.BevelHeight + (Luma(Pixels[Index]) - 0.5f) * Settings.DetailHeight;
	}

	// 2. Optional blur, only between opaque pixels of the same shape / tile.
	for (int32 Pass = 0; Pass < Settings.Smoothing; ++Pass)
	{
		TArray<float> Blurred = Height;
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			for (int32 X = 0; X < Size.X; ++X)
			{
				if (!Opaque[Y * Size.X + X])
				{
					continue;
				}
				float Sum = 0.f;
				float Weight = 0.f;
				for (int32 DY = -1; DY <= 1; ++DY)
				{
					for (int32 DX = -1; DX <= 1; ++DX)
					{
						int32 NX, NY;
						const float W = (DX == 0 && DY == 0) ? 4.f : (DX == 0 || DY == 0) ? 2.f : 1.f;
						if (Region.Resolve(X, Y, DX, DY, NX, NY) && Opaque[NY * Size.X + NX])
						{
							Sum += Height[NY * Size.X + NX] * W;
							Weight += W;
						}
					}
				}
				Blurred[Y * Size.X + X] = Sum / Weight;
			}
		}
		Height = MoveTemp(Blurred);
	}

	// 3. Sobel gradient -> tangent-space normal. Image Y runs down, which is +V: green follows it (DirectX / Unreal).
	auto Sample = [&](int32 X, int32 Y, int32 DX, int32 DY)
	{
		int32 NX, NY;
		return Region.Resolve(X, Y, DX, DY, NX, NY) ? Height[NY * Size.X + NX] : 0.f;
	};
	Result.SetNumUninitialized(Count);
	for (int32 Y = 0; Y < Size.Y; ++Y)
	{
		for (int32 X = 0; X < Size.X; ++X)
		{
			const int32 Index = Y * Size.X + X;
			if (!Opaque[Index])
			{
				Result[Index] = FColor(Flat.R, Flat.G, Flat.B, 0);
				continue;
			}
			const float GX = (Sample(X, Y, 1, -1) + 2.f * Sample(X, Y, 1, 0) + Sample(X, Y, 1, 1)
				- Sample(X, Y, -1, -1) - 2.f * Sample(X, Y, -1, 0) - Sample(X, Y, -1, 1)) / 8.f;
			const float GY = (Sample(X, Y, -1, 1) + 2.f * Sample(X, Y, 0, 1) + Sample(X, Y, 1, 1)
				- Sample(X, Y, -1, -1) - 2.f * Sample(X, Y, 0, -1) - Sample(X, Y, 1, -1)) / 8.f;
			const FVector3f Normal = FVector3f(-GX * Settings.Strength, -GY * Settings.Strength, 1.f).GetSafeNormal();
			Result[Index] = FColor(
				uint8(FMath::RoundToInt((Normal.X * 0.5f + 0.5f) * 255.f)),
				uint8(FMath::RoundToInt((Normal.Y * 0.5f + 0.5f) * 255.f)),
				uint8(FMath::RoundToInt((Normal.Z * 0.5f + 0.5f) * 255.f)),
				255);
		}
	}
	return Result;
}

bool FOnyxNormalMaps::ReadPixels(UTexture2D* Texture, TArray<FColor>& OutPixels, FIntPoint& OutSize)
{
	if (!Texture || !Texture->Source.IsValid() || Texture->Source.GetFormat() != TSF_BGRA8)
	{
		return false;
	}
	FTextureSource& Source = Texture->Source;
	const uint8* Data = Source.LockMipReadOnly(0);
	if (!Data)
	{
		return false;
	}
	OutSize = FIntPoint(Source.GetSizeX(), Source.GetSizeY());
	OutPixels.SetNumUninitialized(OutSize.X * OutSize.Y);
	FMemory::Memcpy(OutPixels.GetData(), Data, OutPixels.Num() * sizeof(FColor)); // BGRA8 == FColor layout
	Source.UnlockMip(0);
	return true;
}

UTexture2D* FOnyxNormalMaps::CreateFor(UTexture2D* Sheet, const FOnyxNormalMapSettings& Settings, const FOnyxPixelGrid& Grid)
{
	TArray<FColor> Pixels;
	FIntPoint Size;
	if (!ReadPixels(Sheet, Pixels, Size))
	{
		return nullptr;
	}
	const TArray<FColor> Normals = FOnyxNormalMapGenerator::Generate(Pixels, Size, Settings, Grid);
	if (Normals.IsEmpty())
	{
		return nullptr;
	}

	const FString Folder = FPackageName::GetLongPackagePath(Sheet->GetOutermost()->GetName());
	UTexture2D* NormalMap = FOnyxAssetFactory::CreateOrReuse<UTexture2D>(Folder, Sheet->GetName() + TEXT("_N"));
	if (!NormalMap)
	{
		return nullptr;
	}
	NormalMap->Modify();
	NormalMap->PreEditChange(nullptr);
	NormalMap->Source.Init(Size.X, Size.Y, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Normals.GetData()));
	NormalMap->CompressionSettings = TC_Normalmap;
	NormalMap->SRGB = false;
	NormalMap->Filter = TF_Nearest; // one normal per art pixel, like the colour sheet
	NormalMap->MipGenSettings = TMGS_NoMipmaps;
	NormalMap->LODGroup = TEXTUREGROUP_Pixels2D;
	NormalMap->bFlipGreenChannel = false;
	NormalMap->PostEditChange();
	NormalMap->MarkPackageDirty();
	return NormalMap;
}

bool FOnyxNormalMaps::Assign(UObject* SpriteOrTileSet, UTexture* NormalMap)
{
	FArrayProperty* Property = SpriteOrTileSet ? FindFProperty<FArrayProperty>(SpriteOrTileSet->GetClass(), AdditionalTexturesName) : nullptr;
	if (!Property)
	{
		return false;
	}
	// Protected in Paper2D, so go through reflection and the normal edit-change path (it rebuilds render data).
	SpriteOrTileSet->Modify();
	SpriteOrTileSet->PreEditChange(Property);
	TArray<TObjectPtr<UTexture>>& Textures = *Property->ContainerPtrToValuePtr<TArray<TObjectPtr<UTexture>>>(SpriteOrTileSet);
	if (Textures.IsEmpty())
	{
		Textures.Add(NormalMap);
	}
	else
	{
		Textures[0] = NormalMap;
	}
	FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
	SpriteOrTileSet->PostEditChangeProperty(Event);
	SpriteOrTileSet->MarkPackageDirty();
	return true;
}

UTexture* FOnyxNormalMaps::GetAssigned(const UObject* SpriteOrTileSet)
{
	const FArrayProperty* Property = SpriteOrTileSet ? FindFProperty<FArrayProperty>(SpriteOrTileSet->GetClass(), AdditionalTexturesName) : nullptr;
	if (!Property)
	{
		return nullptr;
	}
	const TArray<TObjectPtr<UTexture>>& Textures = *Property->ContainerPtrToValuePtr<TArray<TObjectPtr<UTexture>>>(SpriteOrTileSet);
	return Textures.IsEmpty() ? nullptr : Textures[0].Get();
}

FOnyxPixelGrid FOnyxNormalMaps::GridOf(const UPaperTileSet* TileSet)
{
	FOnyxPixelGrid Grid;
	if (TileSet)
	{
		Grid.CellSize = TileSet->GetTileSize();
		Grid.Margin = FIntPoint(TileSet->GetMargin().Left, TileSet->GetMargin().Top);
		Grid.Spacing = TileSet->GetPerTileSpacing();
	}
	return Grid;
}

int32 FOnyxNormalMaps::GenerateForSheet(UTexture2D* Sheet, const FOnyxNormalMapSettings& Settings)
{
	if (!Sheet)
	{
		return 0;
	}
	// Who uses this sheet? Sprites and tile sets that reference it.
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FName> Referencers;
	Registry.GetReferencers(Sheet->GetOutermost()->GetFName(), Referencers);

	TArray<UPaperSprite*> Sprites;
	TArray<UPaperTileSet*> TileSets;
	for (const FName& PackageName : Referencers)
	{
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPackageName(PackageName, Assets);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.IsInstanceOf(UPaperSprite::StaticClass()))
			{
				UPaperSprite* Sprite = Cast<UPaperSprite>(Asset.GetAsset());
				if (Sprite && Sprite->GetSourceTexture() == Sheet)
				{
					Sprites.Add(Sprite);
				}
			}
			else if (Asset.IsInstanceOf(UPaperTileSet::StaticClass()))
			{
				UPaperTileSet* TileSet = Cast<UPaperTileSet>(Asset.GetAsset());
				if (TileSet && TileSet->GetTileSheetTexture() == Sheet)
				{
					TileSets.Add(TileSet);
				}
			}
		}
	}

	if (Sprites.IsEmpty() && TileSets.IsEmpty())
	{
		return 0; // nothing draws this sheet: don't litter the folder with an unused normal map
	}
	// A sheet used as a tile set is cut into tiles: bevel per tile so painted tiles join seamlessly.
	const FOnyxPixelGrid Grid = TileSets.IsEmpty() ? FOnyxPixelGrid() : GridOf(TileSets[0]);
	UTexture2D* NormalMap = CreateFor(Sheet, Settings, Grid);
	if (!NormalMap)
	{
		return 0;
	}

	TArray<UPackage*> Packages = { NormalMap->GetOutermost() };
	int32 Assigned = 0;
	for (UPaperSprite* Sprite : Sprites)
	{
		Assigned += Assign(Sprite, NormalMap) ? 1 : 0;
		Packages.Add(Sprite->GetOutermost());
	}
	for (UPaperTileSet* TileSet : TileSets)
	{
		Assigned += Assign(TileSet, NormalMap) ? 1 : 0;
		Packages.Add(TileSet->GetOutermost());
	}
	FOnyxAssetFactory::SavePackages(Packages);
	return Assigned;
}
