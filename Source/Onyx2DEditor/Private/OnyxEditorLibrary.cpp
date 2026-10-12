// Copyright Onyx2D. All Rights Reserved.

#include "OnyxEditorLibrary.h"
#include "Builders/OnyxArtImporter.h"
#include "Builders/OnyxNormalMaps.h"
#include "Builders/OnyxFramePacker.h"
#include "Builders/OnyxTiledImporter.h"
#include "Builders/OnyxTerrains.h"
#include "Builders/OnyxTilePadding.h"
#include "Level/OnyxMapPainter.h"
#include "PaperTileSet.h"
#include "Builders/OnyxAssetUtils.h"
#include "Builders/OnyxCharacterBuilder.h"
#include "Level/OnyxLevelTools.h"
#include "Builders/OnyxTileBuilder.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PaperTileMap.h"
#include "PaperTileSet.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxEditor, Log, All);

UBlueprint* UOnyxEditorLibrary::CreateCharacterFromSheet(UTexture2D* SpriteSheet, const FString& Name, const FString& Folder, FIntPoint FrameSize,
	const TArray<FName>& RowAnimations, int32 Behaviours, uint8 Team, float CentimetersPerPixel, float FramesPerSecond)
{
	FOnyxCharacterSpec Spec;
	Spec.Name = Name;
	Spec.Folder = Folder;
	Spec.SpriteSheet = SpriteSheet;
	Spec.FrameSize = FrameSize;
	Spec.CentimetersPerPixel = CentimetersPerPixel;
	Spec.Behaviours = Behaviours;
	Spec.Team = Team;

	const TArray<FOnyxClipSpec> Presets = FOnyxCharacterSpec::DefaultClips();
	for (int32 Row = 0; Row < RowAnimations.Num(); ++Row)
	{
		const FOnyxClipSpec* Preset = Presets.FindByPredicate([&](const FOnyxClipSpec& P) { return P.Name == RowAnimations[Row]; });
		FOnyxClipSpec Animation = Preset ? *Preset : FOnyxClipSpec();
		Animation.Name = RowAnimations[Row];
		Animation.Row = Row;
		Animation.bEnabled = true;
		Animation.FramesPerSecond = Preset ? Preset->FramesPerSecond : FramesPerSecond;
		Spec.Clips.Add(Animation);
	}

	const FOnyxCharacterBuildResult Result = FOnyxCharacterBuilder::Build(Spec);
	for (const FString& Warning : Result.Warnings)
	{
		UE_LOG(LogOnyxEditor, Warning, TEXT("%s"), *Warning);
	}
	if (!Result.Succeeded())
	{
		UE_LOG(LogOnyxEditor, Error, TEXT("CreateCharacterFromSheet failed: %s"), *Result.Error.ToString());
		return nullptr;
	}
	UE_LOG(LogOnyxEditor, Log, TEXT("Created %s: %d flipbooks, %d sprites"), *Result.Blueprint->GetPathName(), Result.FlipbookCount, Result.SpriteCount);
	return Result.Blueprint;
}

UPaperTileSet* UOnyxEditorLibrary::CreateTileSet(UTexture2D* Texture, FIntPoint TileSize, const FString& Name, const FString& Folder, bool bSolidCollision)
{
	FOnyxTileSetSpec Spec;
	Spec.Texture = Texture;
	Spec.TileSize = TileSize;
	Spec.Name = Name;
	Spec.Folder = Folder;
	Spec.bSolidCollision = bSolidCollision;
	FText Error;
	UPaperTileSet* TileSet = FOnyxTileBuilder::CreateTileSet(Spec, Error);
	if (!TileSet)
	{
		UE_LOG(LogOnyxEditor, Error, TEXT("CreateTileSet failed: %s"), *Error.ToString());
	}
	return TileSet;
}

UPaperTileMap* UOnyxEditorLibrary::CreateTileMap(UPaperTileSet* TileSet, int32 Width, int32 Height, const FString& Name, const FString& Folder,
	float CentimetersPerPixel, int32 GroundTile, bool bPlaceInLevel)
{
	FOnyxTileMapSpec Spec;
	Spec.TileSet = TileSet;
	Spec.Width = Width;
	Spec.Height = Height;
	Spec.Name = Name;
	Spec.Folder = Folder;
	Spec.CentimetersPerPixel = CentimetersPerPixel;
	Spec.GroundTile = GroundTile;
	Spec.bPlaceInLevel = bPlaceInLevel;
	FText Error;
	UPaperTileMap* TileMap = FOnyxTileBuilder::CreateTileMap(Spec, Error);
	if (!TileMap)
	{
		UE_LOG(LogOnyxEditor, Error, TEXT("CreateTileMap failed: %s"), *Error.ToString());
	}
	return TileMap;
}

TArray<FString> UOnyxEditorLibrary::SetupCurrentLevelFor2D()
{
	return FOnyxLevelTools::SetupCurrentLevel();
}

UPaperSprite* UOnyxEditorLibrary::CreateSpriteFromCell(UTexture2D* Texture, FIntPoint CellSize, FIntPoint Cell, const FString& Name, const FString& Folder,
	float CentimetersPerPixel)
{
	if (!Texture || CellSize.X <= 0 || CellSize.Y <= 0 || !FOnyxAssetUtils::IsValidContentFolder(Folder))
	{
		UE_LOG(LogOnyxEditor, Error, TEXT("CreateSpriteFromCell: needs a texture, a positive cell size and a /Game folder."));
		return nullptr;
	}
	FOnyxAssetUtils::ConfigurePixelArtTexture(Texture);
	UPaperSprite* Sprite = FOnyxAssetUtils::CreateSprite(Folder, FOnyxAssetUtils::SanitizeName(Name), Texture,
		FIntPoint(Cell.X * CellSize.X, Cell.Y * CellSize.Y), CellSize, 1.f / FMath::Max(CentimetersPerPixel, 0.01f), ESpritePivotMode::Bottom_Center);
	if (Sprite)
	{
		FOnyxAssetUtils::SavePackages({ Sprite->GetPackage() });
	}
	return Sprite;
}

UPaperFlipbook* UOnyxEditorLibrary::CreateFlipbookFromCells(UTexture2D* Texture, FIntPoint CellSize, const TArray<FIntPoint>& Cells, const FString& Name,
	const FString& Folder, float CentimetersPerPixel, float FramesPerSecond)
{
	const FString SafeName = FOnyxAssetUtils::SanitizeName(Name);
	TArray<UPaperSprite*> Frames;
	TArray<UPackage*> Packages;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (UPaperSprite* Frame = CreateSpriteFromCell(Texture, CellSize, Cells[Index], FString::Printf(TEXT("S_%s_%02d"), *SafeName, Index), Folder / TEXT("Sprites"), CentimetersPerPixel))
		{
			Frames.Add(Frame);
		}
	}
	if (Frames.IsEmpty())
	{
		return nullptr;
	}
	UPaperFlipbook* Flipbook = FOnyxAssetUtils::CreateFlipbook(Folder, SafeName, Frames, FramesPerSecond);
	if (Flipbook)
	{
		FOnyxAssetUtils::SavePackages({ Flipbook->GetPackage() });
	}
	return Flipbook;
}

int32 UOnyxEditorLibrary::AutoAssignTileCollision(UPaperTileSet* TileSet, float SolidCoverage)
{
	const int32 Solid = FOnyxTileBuilder::AutoAssignCollision(TileSet, SolidCoverage);
	if (TileSet)
	{
		FOnyxAssetUtils::SavePackages({ TileSet->GetPackage() });
	}
	return Solid;
}

int32 UOnyxEditorLibrary::GenerateNormalMap(UTexture2D* Sheet, float BevelWidth, float BevelHeight, float DetailHeight, int32 Smoothing, float Strength)
{
	FOnyxNormalMapSettings Settings;
	Settings.BevelWidth = BevelWidth;
	Settings.BevelHeight = BevelHeight;
	Settings.DetailHeight = DetailHeight;
	Settings.Smoothing = Smoothing;
	Settings.Strength = Strength;
	return FOnyxNormalMaps::GenerateForSheet(Sheet, Settings);
}

UTexture2D* UOnyxEditorLibrary::PackFramesIntoStrip(const TArray<UTexture2D*>& Frames, const FString& Name, const FString& Folder, FIntPoint& FrameSize)
{
	FText Error;
	UTexture2D* Strip = FOnyxFramePacker::PackStrip(Frames, Folder, Name, FrameSize, Error);
	if (!Strip)
	{
		UE_LOG(LogOnyxEditor, Error, TEXT("PackFramesIntoStrip failed: %s"), *Error.ToString());
	}
	return Strip;
}

UBlueprint* UOnyxEditorLibrary::CreateCharacterFromStrips(const TMap<FName, UTexture2D*>& Strips, const FString& Name, const FString& Folder, FIntPoint FrameSize,
	int32 Behaviours, uint8 Team, float CentimetersPerPixel, const TMap<FName, float>& FramesPerSecond)
{
	const FOnyxCharacterBuildResult Result = FOnyxCharacterBuilder::Build(
		FOnyxCharacterSpec::FromStrips(Strips, Name, Folder, FrameSize, Behaviours, Team, CentimetersPerPixel, FramesPerSecond));
	for (const FString& Warning : Result.Warnings)
	{
		UE_LOG(LogOnyxEditor, Warning, TEXT("%s"), *Warning);
	}
	if (!Result.Succeeded())
	{
		UE_LOG(LogOnyxEditor, Error, TEXT("CreateCharacterFromStrips failed: %s"), *Result.Error.ToString());
		return nullptr;
	}
	return Result.Blueprint;
}

UPaperTileMap* UOnyxEditorLibrary::ImportTiledMap(const FString& FilePath, UPaperTileSet* TileSet, const FString& Name, const FString& Folder,
	float CentimetersPerPixel, bool bPlaceInLevel)
{
	FOnyxTiledImportSpec Spec;
	Spec.FilePath = FilePath;
	Spec.TileSet = TileSet;
	Spec.Name = Name;
	Spec.Folder = Folder;
	Spec.CentimetersPerPixel = CentimetersPerPixel;
	Spec.bPlaceInLevel = bPlaceInLevel;
	FText Error;
	UPaperTileMap* TileMap = FOnyxTiledImporter::Import(Spec, Error);
	if (!TileMap)
	{
		UE_LOG(LogOnyxEditor, Error, TEXT("ImportTiledMap failed: %s"), *Error.ToString());
	}
	return TileMap;
}

void UOnyxEditorLibrary::SetTileCollision(UPaperTileSet* TileSet, const TArray<int32>& TileIndices, EOnyxTileCollision Collision)
{
	if (!TileSet)
	{
		return;
	}
	TileSet->Modify();
	for (const int32 Index : TileIndices)
	{
		FOnyxTileCollision::Set(TileSet, Index, Collision);
	}
	TileSet->PostEditChange();
	TileSet->MarkPackageDirty();
	FOnyxAssetUtils::SavePackages({ TileSet->GetPackage() });
}

int32 UOnyxEditorLibrary::CreateTerrain(UPaperTileSet* TileSet, FIntPoint TemplateMin, FIntPoint TemplateSize, const FString& Name)
{
	const int32 Terrain = FOnyxTerrains::CreateFromBlock(TileSet, TemplateMin, TemplateSize, Name);
	if (Terrain != INDEX_NONE)
	{
		FOnyxAssetUtils::SavePackages({ TileSet->GetPackage() });
	}
	return Terrain;
}

bool UOnyxEditorLibrary::SetTerrainInnerCorners(UPaperTileSet* TileSet, int32 Terrain, FIntPoint BlockMin, FIntPoint BlockSize)
{
	const bool bSet = FOnyxTerrains::SetInnerCorners(TileSet, Terrain, BlockMin, BlockSize);
	if (bSet)
	{
		FOnyxAssetUtils::SavePackages({ TileSet->GetPackage() });
	}
	return bSet;
}

void UOnyxEditorLibrary::PaintTerrain(UPaperTileMap* TileMap, int32 LayerIndex, const TArray<FIntPoint>& Cells, UPaperTileSet* TileSet, int32 Terrain, bool bErase)
{
	if (!TileMap)
	{
		return;
	}
	FOnyxMapPainter Painter;
	Painter.SetTileMap(TileMap);
	Painter.SetLayer(LayerIndex);
	Painter.BeginStroke(INVTEXT("Paint Terrain"));
	for (const FIntPoint& Cell : Cells)
	{
		Painter.PaintTerrain(Cell, TileSet, Terrain, bErase);
	}
	Painter.EndStroke();
	FOnyxAssetUtils::SavePackages({ TileMap->GetPackage() });
}

TArray<FString> UOnyxEditorLibrary::GetTerrainNames(const UPaperTileSet* TileSet)
{
	TArray<FString> Names;
	for (int32 Index = 0; Index < FOnyxTerrains::GetCount(TileSet); ++Index)
	{
		Names.Add(FOnyxTerrains::GetName(TileSet, Index));
	}
	return Names;
}

bool UOnyxEditorLibrary::PadTileSheet(UPaperTileSet* TileSet, int32 Padding)
{
	return FOnyxTilePadding::PadTileSet(TileSet, Padding);
}

TArray<UObject*> UOnyxEditorLibrary::ImportArt(const TArray<FString>& Paths, const FString& Folder, float CentimetersPerPixel)
{
	FOnyxArtImportSettings Settings;
	Settings.Folder = Folder;
	Settings.CentimetersPerPixel = CentimetersPerPixel;
	TArray<UObject*> Results;
	for (const TSharedPtr<FOnyxArtItem>& Item : FOnyxArtImporter::Analyze(Paths))
	{
		UE_LOG(LogOnyxEditor, Display, TEXT("ImportArt: %s '%s' from %s: %s"), *FOnyxArtImporter::GetKindName(Item->Kind).ToString(), *Item->Name, *Item->Source, *Item->Describe().ToString());
		FText Error;
		if (!FOnyxArtImporter::Build(*Item, Settings, Error))
		{
			UE_LOG(LogOnyxEditor, Error, TEXT("ImportArt: %s failed: %s"), *Item->Name, *Error.ToString());
		}
		else if (UObject* Result = Item->Result.Get())
		{
			Results.Add(Result);
		}
	}
	return Results;
}

void UOnyxEditorLibrary::AddBehavioursToBlueprint(UBlueprint* Blueprint, int32 Behaviours)
{
	if (Blueprint)
	{
		FOnyxAssetUtils::AddBehaviourComponents(Blueprint, Behaviours);
		Blueprint->MarkPackageDirty();
		FOnyxAssetUtils::SavePackages({ Blueprint->GetPackage() });
	}
}
