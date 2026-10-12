// Copyright Onyx2D. All Rights Reserved.

#include "Examples/OnyxWaterExample.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Builders/OnyxAssetUtils.h"
#include "Builders/OnyxTileBuilder.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Examples/OnyxSunnyLandExample.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Level/OnyxBrushLibrary.h"
#include "Level/OnyxLevelTools.h"
#include "Level/OnyxMapPainter.h"
#include "Lighting/OnyxLightingEnvironment.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "OnyxEditorLibrary.h"
#include "PaperFlipbook.h"
#include "PaperTileMap.h"
#include "PaperTileSet.h"
#include "World/OnyxParallaxBackground.h"
#include "World/OnyxPhysicsProp.h"
#include "World/OnyxPickup.h"

const TCHAR* FOnyxWaterExample::LevelPath = TEXT("/Game/Maps/Test_Water");

namespace
{
	/**
	 * The ground, one character per tile ('#' = Ground terrain): a pool between two banks, an island to jump from.
	 * 48 x 20 tiles of 16 px (64 cm).
	 */
	const TCHAR* const PoolLayout[] = {
		TEXT("................................................"),
		TEXT("................................................"),
		TEXT("................................................"),
		TEXT("................................................"),
		TEXT("................................................"),
		TEXT("................................................"),
		TEXT("................................................"),
		TEXT("................................................"),
		TEXT("......................####......................"),
		TEXT("......................####......................"),
		TEXT("................................................"),
		TEXT("................................................"),
		TEXT("############........................############"),
		TEXT("############........................############"),
		TEXT("############........................############"),
		TEXT("############........................############"),
		TEXT("############........................############"),
		TEXT("############........................############"),
		TEXT("################################################"),
		TEXT("################################################"),
	};
	/** The water fills the pool from one tile under the banks' top to the floor. */
	const FIntRect PoolWater(FIntPoint(12, 13), FIntPoint(35, 17));

	template <class T>
	T* LoadExampleAsset(const FString& Path)
	{
		return LoadObject<T>(nullptr, *(Path + TEXT(".") + FPaths::GetBaseFilename(Path)), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	void DeleteAsset(const FString& PackageName)
	{
		TArray<FAssetData> Found;
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssetsByPackageName(FName(PackageName), Found);
		if (!Found.IsEmpty())
		{
			ObjectTools::DeleteAssets(Found, false);
		}
	}
}

bool FOnyxWaterExample::Build(FText& OutError)
{
	const FString SunnyLand = FOnyxSunnyLandExample::ContentFolder;
	UPaperTileSet* TileSet = LoadExampleAsset<UPaperTileSet>(SunnyLand / TEXT("TS_SunnyLand"));
	const UBlueprint* GameMode = LoadExampleAsset<UBlueprint>(SunnyLand / TEXT("BP_SunnyLandGameMode"));
	const int32 Ground = TileSet ? UOnyxEditorLibrary::GetTerrainNames(TileSet).IndexOfByKey(TEXT("Ground")) : INDEX_NONE;
	if (!TileSet || !GameMode || Ground == INDEX_NONE)
	{
		OutError = INVTEXT("Build the Sunny Land example first: the water level uses its tiles, fox and backgrounds.");
		return false;
	}

	// A fresh level and tile map (the old ones are deleted, so nothing stale is left over).
	UEditorLoadingAndSavingUtils::NewBlankMap(false);
	DeleteAsset(LevelPath);
	DeleteAsset(SunnyLand / TEXT("TM_WaterTest"));
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		OutError = INVTEXT("No editor world.");
		return false;
	}
	FOnyxTileMapSpec Spec;
	Spec.TileSet = TileSet;
	Spec.Name = TEXT("TM_WaterTest");
	Spec.Folder = SunnyLand;
	Spec.Width = FCString::Strlen(PoolLayout[0]);
	Spec.Height = UE_ARRAY_COUNT(PoolLayout);
	Spec.CentimetersPerPixel = FOnyxSunnyLandExample::PixelSize;
	Spec.bFillGround = false;
	Spec.bPlaceInLevel = true;
	UPaperTileMap* TileMap = FOnyxTileBuilder::CreateTileMap(Spec, OutError);
	if (!TileMap)
	{
		return false;
	}
	FOnyxLevelTools::SetupCurrentLevel(); // game mode, player start, Onyx Lighting, tile collision
	World->GetWorldSettings()->DefaultGameMode = GameMode->GeneratedClass;

	// Everything from here is what the Level Painter does when you paint by hand.
	FOnyxMapPainter Painter;
	Painter.SetTileMap(TileMap);
	Painter.BeginStroke(INVTEXT("Paint the pool"));
	for (int32 Row = 0; Row < Spec.Height; ++Row)
	{
		for (int32 Column = 0; Column < Spec.Width; ++Column)
		{
			if (PoolLayout[Row][Column] == TEXT('#'))
			{
				Painter.PaintTerrain(FIntPoint(Column, Row), TileSet, Ground, false); // edges and corners pick themselves
			}
		}
	}
	Painter.EndStroke();
	Painter.FlushChanges();
	FOnyxAssetUtils::SavePackages({ TileMap->GetPackage() }); // the tiles live in the tile map asset, not the level

	const FOnyxBrushLibrary& Library = FOnyxBrushLibrary::Get();
	const FName Entities = FOnyxBrushLibrary::EntitiesLayer;
	if (const TSharedPtr<FOnyxPainterBrush> Water = Library.FindById(TEXT("Water")))
	{
		if (AActor* Pool = Painter.PlaceArea(PoolWater, *Water, Entities))
		{
			Pool->SetActorLabel(TEXT("Water"));
		}
	}

	// Things that float or sink: Sunny Land's crates (wood) and rock (stone).
	if (const TSharedPtr<FOnyxPainterBrush> PropBrush = Library.FindById(TEXT("PhysicsProp")))
	{
		struct FProp { const TCHAR* Art; FIntPoint Cell; float Density; };
		for (const FProp& Prop : { FProp{ TEXT("crate"), FIntPoint(8, 11), 0.6f }, FProp{ TEXT("crate"), FIntPoint(17, 6), 0.6f },
			FProp{ TEXT("big-crate"), FIntPoint(27, 4), 0.5f }, FProp{ TEXT("rock"), FIntPoint(31, 8), 2.6f } })
		{
			UPaperSprite* Sprite = FOnyxSunnyLandExample::ImportProp(Prop.Art);
			FOnyxPainterBrush Brush = *PropBrush;
			Brush.Configure = [Sprite, Density = Prop.Density](AActor* Actor)
			{
				AOnyxPhysicsProp* Physics = CastChecked<AOnyxPhysicsProp>(Actor);
				Physics->Sprite = Sprite;
				Physics->Density = Density;
				Physics->RerunConstructionScripts();
			};
			if (AActor* Actor = Painter.PlaceObject(Prop.Cell, Brush, Entities))
			{
				Actor->SetActorLabel(FString::Printf(TEXT("%s_%d_%d"), Prop.Art, Prop.Cell.X, Prop.Cell.Y));
			}
		}
	}

	// Gems on the pool floor, to dive for.
	UPaperFlipbook* Gem = LoadExampleAsset<UPaperFlipbook>(SunnyLand / TEXT("Effects/FB_Gem"));
	UPaperFlipbook* Collect = LoadExampleAsset<UPaperFlipbook>(SunnyLand / TEXT("Effects/FB_ItemFeedback"));
	const TSharedPtr<FOnyxPainterBrush> CoinBrush = Library.FindById(TEXT("Coin"));
	for (const FIntPoint& Cell : { FIntPoint(16, 17), FIntPoint(23, 16), FIntPoint(31, 17) })
	{
		AOnyxPickup* Pickup = CoinBrush.IsValid() ? Cast<AOnyxPickup>(Painter.PlaceObject(Cell, *CoinBrush, Entities)) : nullptr;
		if (Pickup && Gem)
		{
			Pickup->Flipbook = Gem;
			Pickup->CollectEffect = Collect;
			Pickup->Amount = 5;
			Pickup->BobHeight = 6.f;
			Pickup->RerunConstructionScripts();
		}
	}

	// The fox starts on the left bank.
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		It->SetActorLocation(Painter.CellToWorld(FIntPoint(5, 10)));
	}

	// Sunny Land's sky and jungle behind, a sunny day.
	UTexture2D* Back = LoadExampleAsset<UTexture2D>(SunnyLand / TEXT("Textures/T_SunnyLand_Back"));
	UTexture2D* Middle = LoadExampleAsset<UTexture2D>(SunnyLand / TEXT("Textures/T_SunnyLand_Middle"));
	const FBox LevelArea = FOnyxLevelTools::GetLevelBounds2D(World);
	if (Back && Middle && LevelArea.IsValid)
	{
		AOnyxParallaxBackground* Background = World->SpawnActor<AOnyxParallaxBackground>(AOnyxParallaxBackground::StaticClass(),
			FTransform(FVector(LevelArea.GetCenter().X, 0.f, LevelArea.GetCenter().Z)));
		Background->SetActorLabel(TEXT("Parallax"));
		Background->BackgroundLayers.Reset();
		FOnyxParallaxLayer Sky;
		Sky.Name = TEXT("Sky and sea");
		Sky.Texture = Back;
		Sky.Factor = FVector2D(0.9f, 0.9f);
		Sky.Depth = 2500.f;
		Sky.Height = 2200.f;
		Background->BackgroundLayers.Add(Sky);
		FOnyxParallaxLayer Jungle;
		Jungle.Name = TEXT("Jungle");
		Jungle.Texture = Middle;
		Jungle.Factor = FVector2D(0.6f, 0.7f);
		Jungle.Depth = 1200.f;
		Jungle.Height = 1500.f;
		Jungle.OffsetZ = -200.f;
		Background->BackgroundLayers.Add(Jungle);
		Background->RerunConstructionScripts();
	}
	for (TActorIterator<AOnyxLightingEnvironment> It(World); It; ++It)
	{
		It->Preset = EOnyxMoodPreset::Day;
		It->Mood = FOnyxLightingMood::FromPreset(EOnyxMoodPreset::Day);
	}

	if (!UEditorLoadingAndSavingUtils::SaveMap(World, LevelPath))
	{
		OutError = INVTEXT("Could not save /Game/Maps/Test_Water.");
		return false;
	}
	FOnyxLevelTools::ShowSideView(LevelArea); // when built from the editor
	return true;
}
