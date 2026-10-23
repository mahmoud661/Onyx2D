// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxLevelBuilder.h"
#include "Camera/OnyxCameraBounds.h"
#include "Character/OnyxBehaviourLibrary.h"
#include "Character/OnyxCharacter.h"
#include "Combat/OnyxCombatTypes.h"
#include "Movement/OnyxMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Core/Onyx2DSettings.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Lighting/OnyxLight2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PaperFlipbook.h"
#include "PaperTileLayer.h"
#include "PaperTileMap.h"
#include "PaperTileMapComponent.h"
#include "PaperTileSet.h"
#include "UObject/ConstructorHelpers.h"
#include "World/OnyxCheckpoint.h"
#include "World/OnyxHazardVolume.h"
#include "World/OnyxLevelExit.h"
#include "World/OnyxMovingPlatform.h"
#include "World/OnyxWater.h"
#include "World/OnyxOneWayPlatform.h"
#include "World/OnyxPickup.h"
#include "World/OnyxTerrainMatch.h"

/** The characters of a layout. */
namespace OnyxLayoutChar
{
	constexpr TCHAR Solid = TEXT('#');
	constexpr TCHAR Platform = TEXT('=');
	constexpr TCHAR Spike = TEXT('^');
	constexpr TCHAR Player = TEXT('P');
	constexpr TCHAR Enemy = TEXT('E');
	constexpr TCHAR Light = TEXT('L');
	constexpr TCHAR Coin = TEXT('C');
	constexpr TCHAR Health = TEXT('H');
	constexpr TCHAR Checkpoint = TEXT('K');
	constexpr TCHAR Exit = TEXT('X');
	constexpr TCHAR MovingPlatform = TEXT('M');
	constexpr TCHAR Water = TEXT('~');
}

namespace
{
	constexpr float EngineCubeSize = 100.f;

	const TCHAR* DemoLayout =
		TEXT("#..........................................................#\n")
		TEXT("#..........................................................#\n")
		TEXT("#.................L.......C.C.C.......L..................L.#\n")
		TEXT("#..........................................................#\n")
		TEXT("#.........................=====...............####.........#\n")
		TEXT("#..............E.....C.C...............E..........#.....C..#\n")
		TEXT("#...........#######.................#######.......#....E...#\n")
		TEXT("#....L.................====.......H...............#..####..#\n")
		TEXT("#...........C.C.C.......................MM............#....#\n")
		TEXT("#.P.......E.........E.....K.....#.........E.......#...#.XE.#\n")
		TEXT("##########^^#########^^^###############^^^#####...##########\n");
}

AOnyxLevelBuilder::AOnyxLevelBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	Layout = DemoLayout;
	EnemyClass = AOnyxCharacter::StaticClass();
	LightClass = AOnyxLight2D::StaticClass();

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	BlockMesh = Cube.Object;

	Blocks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Blocks"));
	Blocks->SetupAttachment(RootComponent);
	Blocks->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Blocks->SetStaticMesh(BlockMesh);

	Background = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Background"));
	Background->SetupAttachment(RootComponent);
	Background->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Background->SetStaticMesh(BlockMesh);
	Background->SetCastShadow(false);

	Tiles = CreateDefaultSubobject<UPaperTileMapComponent>(TEXT("Tiles"));
	Tiles->SetupAttachment(RootComponent);
	Tiles->SetCollisionEnabled(ECollisionEnabled::NoCollision); // the blocks collide
	Tiles->SetCanEverAffectNavigation(false);
	Tiles->SetVisibility(false);
#if WITH_EDITORONLY_DATA
	Tiles->bShowOutlineWhenUnselected = false;
#endif
}

void AOnyxLevelBuilder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildStatic();
}

TArray<FString> AOnyxLevelBuilder::GetRows() const
{
	TArray<FString> Rows;
	Layout.Replace(TEXT("\r"), TEXT("")).ParseIntoArray(Rows, TEXT("\n"), false);
	return Rows;
}

FVector AOnyxLevelBuilder::TileToWorld(int32 Column, int32 Row, int32 RowCount) const
{
	return GetActorLocation() + FVector(Column * TileSize, 0.f, (RowCount - 1 - Row) * TileSize);
}

bool AOnyxLevelBuilder::GetPlayerStart(FVector& OutLocation) const
{
	const TArray<FString> Rows = GetRows();
	for (int32 Row = 0; Row < Rows.Num(); ++Row)
	{
		const int32 Column = Rows[Row].Find(FString::Chr(OnyxLayoutChar::Player), ESearchCase::CaseSensitive);
		if (Column != INDEX_NONE)
		{
			OutLocation = TileToWorld(Column, Row, Rows.Num()) + FVector(0.f, 0.f, TileSize * 0.5f);
			return true;
		}
	}
	return false;
}

FBox AOnyxLevelBuilder::GetLayoutBounds() const
{
	const TArray<FString> Rows = GetRows();
	int32 Columns = 0;
	for (const FString& Row : Rows)
	{
		Columns = FMath::Max(Columns, Row.Len());
	}
	if (Rows.IsEmpty() || Columns == 0)
	{
		return FBox(ForceInit);
	}
	// Tiles are centred on their grid points: the layout reaches half a tile past the first and last ones.
	const FVector HalfTile(TileSize * 0.5f, 0.f, TileSize * 0.5f);
	return FBox(TileToWorld(0, Rows.Num() - 1, Rows.Num()) - HalfTile, TileToWorld(Columns - 1, 0, Rows.Num()) + HalfTile);
}

void AOnyxLevelBuilder::RebuildStatic()
{
	Blocks->ClearInstances();
	Blocks->SetStaticMesh(BlockMesh);

	const TArray<FString> Rows = GetRows();
	const FVector Scale(TileSize / EngineCubeSize, BlockDepth / EngineCubeSize, TileSize / EngineCubeSize);
	int32 MaxColumns = 0;
	for (int32 Row = 0; Row < Rows.Num(); ++Row)
	{
		MaxColumns = FMath::Max(MaxColumns, Rows[Row].Len());
		for (int32 Column = 0; Column < Rows[Row].Len(); ++Column)
		{
			if (Rows[Row][Column] == OnyxLayoutChar::Solid)
			{
				const FVector Local = TileToWorld(Column, Row, Rows.Num()) - GetActorLocation();
				Blocks->AddInstance(FTransform(FRotator::ZeroRotator, Local, Scale));
			}
		}
	}

	UOnyx2DLibrary::ApplySolidColor(Blocks, BlockColor);

	// With a tile set the tiles are what you see; the blocks stay, unseen, for collision.
	const bool bTiled = TileSet && MaxColumns > 0 && PaintTiles(Rows, MaxColumns);
	Blocks->SetVisibility(!bTiled);
	Tiles->SetVisibility(bTiled);

	Background->SetVisibility(bBuildBackground && Rows.Num() > 0);
	if (bBuildBackground && Rows.Num() > 0)
	{
		const float Width = (MaxColumns + 20) * TileSize;
		const float Height = (Rows.Num() + 12) * TileSize;
		const FVector Center((MaxColumns - 1) * TileSize * 0.5f, -BackgroundDistance - BlockDepth * 0.5f, (Rows.Num() - 1) * TileSize * 0.5f);
		Background->SetRelativeLocation(Center);
		Background->SetRelativeScale3D(FVector(Width / EngineCubeSize, 0.1f, Height / EngineCubeSize));
		UOnyx2DLibrary::ApplySolidColor(Background, BackgroundColor);
	}
}

TArray<FString> AOnyxLevelBuilder::GetTerrainOptions() const
{
	TArray<FString> Names;
	for (int32 Index = 0; TileSet && Index < TileSet->GetNumTerrains(); ++Index)
	{
		Names.Add(TileSet->GetTerrain(Index).TerrainName);
	}
	return Names;
}

int32 AOnyxLevelBuilder::FindTerrain() const
{
	if (!TileSet || TileSet->GetNumTerrains() == 0)
	{
		return INDEX_NONE;
	}
	return Terrain.IsEmpty() ? 0 : GetTerrainOptions().IndexOfByKey(Terrain);
}

bool AOnyxLevelBuilder::UsesPlatformTiles() const
{
	return TileSet && PlatformTile >= 0 && PlatformTile < TileSet->GetTileCount();
}

bool AOnyxLevelBuilder::PaintTiles(const TArray<FString>& Rows, int32 Columns)
{
	const int32 TerrainIndex = FindTerrain();
	if (TerrainIndex == INDEX_NONE)
	{
		return false;
	}
	const FIntPoint TilePixels = TileSet->GetTileSize();
	Tiles->CreateNewTileMap(Columns, Rows.Num(), TilePixels.X, TilePixels.Y, TilePixels.X / TileSize, true);
	UPaperTileMap* Map = Tiles->TileMap;
	Map->TileLayers[0]->SetLayerCollides(false);
	if (UMaterialInterface* LitMaterial = UOnyx2DSettings::Get()->LitSpriteMaterial.LoadSynchronous())
	{
		Map->Material = LitMaterial;
	}

	auto IsSolid = [&Rows, Columns](int32 Column, int32 Row)
	{
		// Above the layout is open sky. Past its other edges counts as solid: walls and floors carry on out of view.
		if (Row < 0)
		{
			return false;
		}
		if (Row >= Rows.Num() || Column < 0 || Column >= Columns)
		{
			return true;
		}
		return Column < Rows[Row].Len() && Rows[Row][Column] == OnyxLayoutChar::Solid;
	};
	auto Paint = [this](int32 Column, int32 Row, int32 TileIndex)
	{
		FPaperTileInfo Tile;
		Tile.TileSet = TileSet;
		Tile.PackedTileIndex = TileIndex;
		Tiles->SetTile(Column, Row, 0, Tile);
	};
	auto ValidTile = [this](int32 TileIndex, int32 Fallback)
	{
		return TileIndex >= 0 && TileIndex < TileSet->GetTileCount() ? TileIndex : Fallback;
	};
	int32 TileForCorners[16];
	for (int32& Tile : TileForCorners)
	{
		Tile = INDEX_NONE - 1; // not looked up yet
	}
	for (int32 Row = 0; Row < Rows.Num(); ++Row)
	{
		const FString& Line = Rows[Row];
		for (int32 Column = 0; Column < Line.Len(); ++Column)
		{
			if (Line[Column] == OnyxLayoutChar::Solid)
			{
				bool Neighbours[3][3];
				for (int32 DY = -1; DY <= 1; ++DY)
				{
					for (int32 DX = -1; DX <= 1; ++DX)
					{
						Neighbours[DY + 1][DX + 1] = IsSolid(Column + DX, Row + DY);
					}
				}
				const uint8 Corners = FOnyxTerrainMatch::CornersFromNeighbours(Neighbours);
				int32& Tile = TileForCorners[Corners];
				Tile = Tile == INDEX_NONE - 1 ? FOnyxTerrainMatch::FindTile(TileSet, TerrainIndex, Corners) : Tile;
				if (Tile != INDEX_NONE)
				{
					Paint(Column, Row, Tile);
				}
			}
			else if (Line[Column] == OnyxLayoutChar::Platform && UsesPlatformTiles())
			{
				const bool bLeftEnd = Column == 0 || Line[Column - 1] != OnyxLayoutChar::Platform;
				const bool bRightEnd = Column + 1 == Line.Len() || Line[Column + 1] != OnyxLayoutChar::Platform;
				const int32 Tile = bLeftEnd && !bRightEnd ? ValidTile(PlatformLeftTile, PlatformTile)
					: bRightEnd && !bLeftEnd ? ValidTile(PlatformRightTile, PlatformTile)
					: PlatformTile;
				Paint(Column, Row, Tile);
			}
		}
	}

	// Paper2D centres tile (0, 0) on the component and grows right and down: line that tile up with the layout's first one.
	const FVector FirstTile = TileToWorld(0, 0, Rows.Num()) - GetActorLocation();
	Tiles->SetRelativeLocation(FirstTile - Tiles->GetTileCenterPosition(0, 0, 0, false));
	Tiles->MarkRenderStateDirty();
	return true;
}

UClass* AOnyxLevelBuilder::ResolveEnemyClass() const
{
	// An explicit choice on this actor wins; otherwise use the project-wide default enemy.
	if (EnemyClass && EnemyClass != AOnyxCharacter::StaticClass())
	{
		return EnemyClass;
	}
	if (UClass* Configured = UOnyx2DSettings::Get()->DefaultEnemyClass.TryLoadClass<AActor>())
	{
		return Configured;
	}
	return EnemyClass;
}

void AOnyxLevelBuilder::BeginPlay()
{
	Super::BeginPlay();
	SpawnDynamic();
}

void AOnyxLevelBuilder::SpawnDynamic()
{
	UWorld* World = GetWorld();
	const TArray<FString> Rows = GetRows();
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Platforms drawn by the tiles hide their own bar; checkpoints and exits stand on the floor of their tile.
	const bool bTiledPlatforms = Tiles->IsVisible() && UsesPlatformTiles();
	auto OnFloor = [this](const FVector& Location, const UBoxComponent* Trigger)
	{
		return Location + FVector(0.f, 0.f, Trigger->GetUnscaledBoxExtent().Z - TileSize * 0.5f);
	};

	int32 MaxColumns = 0;
	for (int32 Row = 0; Row < Rows.Num(); ++Row)
	{
		const FString& Line = Rows[Row];
		MaxColumns = FMath::Max(MaxColumns, Line.Len());

		for (int32 Column = 0; Column < Line.Len(); ++Column)
		{
			const FVector Location = TileToWorld(Column, Row, Rows.Num());
			switch (Line[Column])
			{
			case OnyxLayoutChar::Platform:
			{
				// Merge horizontal runs into one platform.
				int32 End = Column;
				while (End + 1 < Line.Len() && Line[End + 1] == OnyxLayoutChar::Platform)
				{
					++End;
				}
				const int32 Count = End - Column + 1;
				const FVector Center = Location + FVector((Count - 1) * TileSize * 0.5f, 0.f, TileSize * 0.5f - 12.f);
				if (AOnyxOneWayPlatform* Platform = World->SpawnActorDeferred<AOnyxOneWayPlatform>(AOnyxOneWayPlatform::StaticClass(), FTransform(Center), this))
				{
					Platform->Size = FVector2D(Count * TileSize, 24.f);
					Platform->FinishSpawning(FTransform(Center));
					Platform->Visual->SetVisibility(!bTiledPlatforms);
				}
				Column = End;
				break;
			}
			case OnyxLayoutChar::Spike:
				World->SpawnActor<AOnyxHazardVolume>(AOnyxHazardVolume::StaticClass(), Location - FVector(0.f, 0.f, TileSize * 0.3f), FRotator::ZeroRotator, Params);
				break;
			case OnyxLayoutChar::Enemy:
				if (UClass* Enemy = ResolveEnemyClass())
				{
					AActor* Spawned = World->SpawnActor<AActor>(Enemy, Location + FVector(0.f, 0.f, TileSize * 0.2f), FRotator::ZeroRotator, Params);
					// The plain stand-in becomes a patrolling foe.
					if (AOnyxCharacter* Foe = Cast<AOnyxCharacter>(Spawned); Foe && Enemy == AOnyxCharacter::StaticClass())
					{
						Foe->TeamId = OnyxTeams::Enemy;
						if (UOnyxMovementComponent* Movement = Foe->GetOnyxMovement())
						{
							Movement->RunSpeed = 170.f;
						}
						UOnyxBehaviourLibrary::AddBehaviours(Foe, int32(UOnyxBehaviourLibrary::PatrolFoePreset));
					}
				}
				break;
			case OnyxLayoutChar::Light:
				if (LightClass)
				{
					World->SpawnActor<AActor>(LightClass, Location, FRotator::ZeroRotator, Params);
				}
				break;
			case OnyxLayoutChar::Coin:
			case OnyxLayoutChar::Health:
				if (AOnyxPickup* Pickup = World->SpawnActorDeferred<AOnyxPickup>(AOnyxPickup::StaticClass(), FTransform(Location), this))
				{
					const bool bCoin = Line[Column] == OnyxLayoutChar::Coin;
					Pickup->Type = bCoin ? EOnyxPickupType::Coin : EOnyxPickupType::Health;
					if (UPaperFlipbook* Look = bCoin ? CoinLook : HealthLook)
					{
						Pickup->Flipbook = Look;
					}
					Pickup->FinishSpawning(FTransform(Location));
				}
				break;
			case OnyxLayoutChar::Checkpoint:
			{
				const FTransform Where(OnFloor(Location, GetDefault<AOnyxCheckpoint>()->Trigger));
				if (AOnyxCheckpoint* Checkpoint = World->SpawnActorDeferred<AOnyxCheckpoint>(AOnyxCheckpoint::StaticClass(), Where, this))
				{
					if (CheckpointLook)
					{
						Checkpoint->DormantLook = CheckpointLook;
						Checkpoint->ActiveLook = CheckpointActiveLook;
					}
					Checkpoint->FinishSpawning(Where);
				}
				break;
			}
			case OnyxLayoutChar::Exit:
			{
				const FTransform Where(OnFloor(Location, GetDefault<AOnyxLevelExit>()->Trigger));
				if (AOnyxLevelExit* Exit = World->SpawnActorDeferred<AOnyxLevelExit>(AOnyxLevelExit::StaticClass(), Where, this))
				{
					Exit->NextLevel = NextLevel;
					if (ExitLook)
					{
						Exit->Look = ExitLook;
					}
					Exit->FinishSpawning(Where);
				}
				break;
			}
			case OnyxLayoutChar::Water:
			{
				// A body of water: the run of ~ on this row, as deep as the rows below repeat it.
				int32 End = Column;
				while (End + 1 < Line.Len() && Line[End + 1] == OnyxLayoutChar::Water)
				{
					++End;
				}
				const bool bStartedAbove = Row > 0 && Column < Rows[Row - 1].Len() && Rows[Row - 1][Column] == OnyxLayoutChar::Water;
				if (!bStartedAbove)
				{
					auto IsWaterRow = [&Rows, Column, End](int32 Other)
					{
						for (int32 Cell = Column; Cell <= End; ++Cell)
						{
							if (Cell >= Rows[Other].Len() || Rows[Other][Cell] != OnyxLayoutChar::Water)
							{
								return false;
							}
						}
						return true;
					};
					int32 Depth = 1;
					while (Row + Depth < Rows.Num() && IsWaterRow(Row + Depth))
					{
						++Depth;
					}
					const int32 Count = End - Column + 1;
					const FTransform Surface(Location + FVector((Count - 1) * TileSize * 0.5f, 0.f, TileSize * 0.5f));
					if (AOnyxWater* Water = World->SpawnActorDeferred<AOnyxWater>(AOnyxWater::StaticClass(), Surface, this))
					{
						Water->Size = FVector2D(Count * TileSize, Depth * TileSize);
						Water->FinishSpawning(Surface);
					}
				}
				Column = End;
				break;
			}
			case OnyxLayoutChar::MovingPlatform:
			{
				int32 End = Column;
				while (End + 1 < Line.Len() && Line[End + 1] == OnyxLayoutChar::MovingPlatform)
				{
					++End;
				}
				const int32 Count = End - Column + 1;
				const FTransform Transform(Location + FVector((Count - 1) * TileSize * 0.5f, 0.f, TileSize * 0.5f - 15.f));
				if (AOnyxMovingPlatform* Platform = World->SpawnActorDeferred<AOnyxMovingPlatform>(AOnyxMovingPlatform::StaticClass(), Transform, this))
				{
					Platform->Size = FVector2D(Count * TileSize, 30.f);
					Platform->Waypoints = { FVector(MovingPlatformTravel * TileSize, 0.f, 0.f) };
					Platform->FinishSpawning(Transform);
				}
				Column = End;
				break;
			}
			default:
				break;
			}
		}
	}

	if (bAddCameraBounds && Rows.Num() > 0)
	{
		const FVector Min = TileToWorld(0, Rows.Num() - 1, Rows.Num()) - FVector(TileSize * 0.5f);
		const FVector Max = TileToWorld(MaxColumns - 1, 0, Rows.Num()) + FVector(TileSize * 0.5f);
		if (AOnyxCameraBounds* Bounds = World->SpawnActor<AOnyxCameraBounds>(AOnyxCameraBounds::StaticClass(), (Min + Max) * 0.5f, FRotator::ZeroRotator, Params))
		{
			Bounds->Area->SetBoxExtent((Max - Min) * 0.5f + FVector(0.f, 500.f, 0.f));
		}
	}
}
