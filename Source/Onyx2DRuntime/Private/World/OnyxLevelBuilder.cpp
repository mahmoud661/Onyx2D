// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxLevelBuilder.h"
#include "Camera/OnyxCameraBounds.h"
#include "Character/OnyxEnemyCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Core/Onyx2DSettings.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Lighting/OnyxLight2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "World/OnyxCheckpoint.h"
#include "World/OnyxHazardVolume.h"
#include "World/OnyxLevelExit.h"
#include "World/OnyxMovingPlatform.h"
#include "World/OnyxOneWayPlatform.h"
#include "World/OnyxPickup.h"

namespace
{
	constexpr TCHAR SolidTile = TEXT('#');
	constexpr TCHAR PlatformTile = TEXT('=');
	constexpr TCHAR SpikeTile = TEXT('^');
	constexpr TCHAR PlayerTile = TEXT('P');
	constexpr TCHAR EnemyTile = TEXT('E');
	constexpr TCHAR LightTile = TEXT('L');
	constexpr TCHAR CoinTile = TEXT('C');
	constexpr TCHAR HealthTile = TEXT('H');
	constexpr TCHAR CheckpointTile = TEXT('K');
	constexpr TCHAR ExitTile = TEXT('X');
	constexpr TCHAR MovingPlatformTile = TEXT('M');
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
	EnemyClass = AOnyxEnemyCharacter::StaticClass();
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
		const int32 Column = Rows[Row].Find(FString::Chr(PlayerTile), ESearchCase::CaseSensitive);
		if (Column != INDEX_NONE)
		{
			OutLocation = TileToWorld(Column, Row, Rows.Num()) + FVector(0.f, 0.f, TileSize * 0.5f);
			return true;
		}
	}
	return false;
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
			if (Rows[Row][Column] == SolidTile)
			{
				const FVector Local = TileToWorld(Column, Row, Rows.Num()) - GetActorLocation();
				Blocks->AddInstance(FTransform(FRotator::ZeroRotator, Local, Scale));
			}
		}
	}

	UOnyx2DLibrary::ApplySolidColor(Blocks, BlockColor);

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

UClass* AOnyxLevelBuilder::ResolveEnemyClass() const
{
	// An explicit choice on this actor wins; otherwise use the project-wide default enemy.
	if (EnemyClass && EnemyClass != AOnyxEnemyCharacter::StaticClass())
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
			case PlatformTile:
			{
				// Merge horizontal runs into one platform.
				int32 End = Column;
				while (End + 1 < Line.Len() && Line[End + 1] == PlatformTile)
				{
					++End;
				}
				const int32 Count = End - Column + 1;
				const FVector Center = Location + FVector((Count - 1) * TileSize * 0.5f, 0.f, TileSize * 0.5f - 12.f);
				if (AOnyxOneWayPlatform* Platform = World->SpawnActorDeferred<AOnyxOneWayPlatform>(AOnyxOneWayPlatform::StaticClass(), FTransform(Center), this))
				{
					Platform->Size = FVector2D(Count * TileSize, 24.f);
					Platform->FinishSpawning(FTransform(Center));
				}
				Column = End;
				break;
			}
			case SpikeTile:
				World->SpawnActor<AOnyxHazardVolume>(AOnyxHazardVolume::StaticClass(), Location - FVector(0.f, 0.f, TileSize * 0.3f), FRotator::ZeroRotator, Params);
				break;
			case EnemyTile:
				if (UClass* Enemy = ResolveEnemyClass())
				{
					World->SpawnActor<AActor>(Enemy, Location + FVector(0.f, 0.f, TileSize * 0.2f), FRotator::ZeroRotator, Params);
				}
				break;
			case LightTile:
				if (LightClass)
				{
					World->SpawnActor<AActor>(LightClass, Location, FRotator::ZeroRotator, Params);
				}
				break;
			case CoinTile:
			case HealthTile:
				if (AOnyxPickup* Pickup = World->SpawnActorDeferred<AOnyxPickup>(AOnyxPickup::StaticClass(), FTransform(Location), this))
				{
					Pickup->Type = Line[Column] == CoinTile ? EOnyxPickupType::Coin : EOnyxPickupType::Health;
					Pickup->FinishSpawning(FTransform(Location));
				}
				break;
			case CheckpointTile:
				World->SpawnActor<AOnyxCheckpoint>(AOnyxCheckpoint::StaticClass(), Location, FRotator::ZeroRotator, Params);
				break;
			case ExitTile:
				if (AOnyxLevelExit* Exit = World->SpawnActorDeferred<AOnyxLevelExit>(AOnyxLevelExit::StaticClass(), FTransform(Location + FVector(0.f, 0.f, 40.f)), this))
				{
					Exit->NextLevel = NextLevel;
					Exit->FinishSpawning(FTransform(Location + FVector(0.f, 0.f, 40.f)));
				}
				break;
			case MovingPlatformTile:
			{
				int32 End = Column;
				while (End + 1 < Line.Len() && Line[End + 1] == MovingPlatformTile)
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
