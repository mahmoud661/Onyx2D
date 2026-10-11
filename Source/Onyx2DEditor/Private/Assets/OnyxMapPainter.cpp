// Copyright Onyx2D. All Rights Reserved.

#include "Assets/OnyxMapPainter.h"
#include "Assets/OnyxTerrains.h"
#include "Core/Onyx2DTypes.h"
#include "Assets/OnyxBrushLibrary.h"
#include "Assets/OnyxTileBuilder.h"
#include "World/OnyxTileCollisionComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PaperTileMap.h"
#include "PaperTileMapActor.h"
#include "PaperTileMapComponent.h"
#include "PaperTileSet.h"

namespace
{
	const FString LayerTagPrefix(TEXT("Onyx.Layer."));
	/** Decor sits slightly in front of the tiles; gameplay objects stay on the gameplay plane. */
	constexpr int32 MaxFillCells = 100000;

	UWorld* EditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	/** Collision bounds (what actually stands on the floor), or the visual bounds for objects without collision. */
	bool GetStandingBounds(const AActor* Actor, FVector& OutOrigin, FVector& OutExtent)
	{
		Actor->GetActorBounds(true, OutOrigin, OutExtent);
		if (OutExtent.IsNearlyZero())
		{
			Actor->GetActorBounds(false, OutOrigin, OutExtent);
		}
		return !OutExtent.IsNearlyZero();
	}

	/** Point that decides which cell an object is on: floor-standing objects use their bottom, the rest their centre. */
	FVector ObjectAnchor(const AActor* Actor, const FOnyxPainterBrush& Brush)
	{
		if (!Brush.bStandOnFloor)
		{
			return Actor->GetActorLocation();
		}
		FVector Origin;
		FVector Extent;
		return GetStandingBounds(Actor, Origin, Extent) ? Origin - FVector(0.f, 0.f, Extent.Z - 1.f) : Actor->GetActorLocation();
	}
}

FPaperTileInfo FOnyxStamp::GetTile(int32 X, int32 Y, const FIntPoint& Cell) const
{
	if (bRandom && IsValid())
	{
		// Stable per cell within a stroke: dragging back over a cell doesn't reshuffle it.
		const uint32 Hash = HashCombine(HashCombine(GetTypeHash(Cell.X), GetTypeHash(Cell.Y)), GetTypeHash(RandomSeed));
		const int32 Pick = int32(Hash % uint32(Size.X * Size.Y));
		X = Pick % Size.X;
		Y = Pick / Size.X;
	}
	else
	{
		X = ((X % Size.X) + Size.X) % Size.X;
		Y = ((Y % Size.Y) + Size.Y) % Size.Y;
		X = bFlipX ? Size.X - 1 - X : X; // a mirrored block also reverses its tile order
		Y = bFlipY ? Size.Y - 1 - Y : Y;
	}
	FPaperTileInfo Tile = GetSourceTile(X, Y);
	if (Tile.IsValid())
	{
		Tile.SetFlagValue(EPaperTileFlags::FlipHorizontal, bFlipX);
		Tile.SetFlagValue(EPaperTileFlags::FlipVertical, bFlipY);
	}
	return Tile;
}

FPaperTileInfo FOnyxStamp::GetSourceTile(int32 X, int32 Y) const
{
	FPaperTileInfo Tile;
	if (!IsValid())
	{
		return Tile;
	}
	const int32 Columns = TileSet->GetTileCountX();
	const int32 TileX = Min.X + ((X % Size.X) + Size.X) % Size.X;
	const int32 TileY = Min.Y + ((Y % Size.Y) + Size.Y) % Size.Y;
	const int32 Index = TileY * Columns + TileX;
	if (TileX < Columns && Index < TileSet->GetTileCount())
	{
		Tile.TileSet = TileSet.Get();
		Tile.PackedTileIndex = Index;
	}
	return Tile;
}

// --- Map ------------------------------------------------------------------------------

void FOnyxMapPainter::SetTileMap(UPaperTileMap* InTileMap)
{
	EndStroke();
	TileMap = InTileMap;
	Layer = 0;
}

APaperTileMapActor* FOnyxMapPainter::FindMapActor() const
{
	UWorld* World = EditorWorld();
	if (!World || !TileMap.IsValid())
	{
		return nullptr;
	}
	for (TActorIterator<APaperTileMapActor> It(World); It; ++It)
	{
		if (It->GetRenderComponent() && It->GetRenderComponent()->TileMap == TileMap.Get())
		{
			return *It;
		}
	}
	return nullptr;
}

APaperTileMapActor* FOnyxMapPainter::PlaceMapInLevel()
{
	UWorld* World = EditorWorld();
	UPaperTileMap* Map = TileMap.Get();
	if (!World || !Map)
	{
		return nullptr;
	}
	if (APaperTileMapActor* Existing = FindMapActor())
	{
		FOnyxTileBuilder::EnsureTileCollision(Existing);
		return Existing;
	}
	const FScopedTransaction Transaction(INVTEXT("Place Tile Map"));
	const float TileHeightWorld = Map->TileHeight * Map->GetUnrealUnitsPerPixel();
	// Paper2D centres tile (0, 0) on the actor: shift by half a tile so the corner is at X = 0 and the ground at Z = 0.
	const float TileWidthWorld = Map->TileWidth * Map->GetUnrealUnitsPerPixel();
	const FVector Origin(TileWidthWorld * 0.5f, Onyx2D::TileMapDepth, TileHeightWorld * (FMath::Max(Map->MapHeight - 2, 0) - 0.5f));
	APaperTileMapActor* Actor = Cast<APaperTileMapActor>(GEditor->AddActor(World->GetCurrentLevel(), APaperTileMapActor::StaticClass(), FTransform(Origin)));
	if (Actor)
	{
		Actor->SetActorLocation(Origin); // AddActor snaps to the editor grid, which would undo the depth
		Actor->GetRenderComponent()->SetTileMap(Map);
		Actor->SetActorLabel(Map->GetName());
		FOnyxTileBuilder::EnsureTileCollision(Actor);
	}
	return Actor;
}

// --- Grid -----------------------------------------------------------------------------

FIntPoint FOnyxMapPainter::GetMapSize() const
{
	return TileMap.IsValid() ? FIntPoint(TileMap->MapWidth, TileMap->MapHeight) : FIntPoint::ZeroValue;
}

FIntPoint FOnyxMapPainter::GetTileSize() const
{
	return TileMap.IsValid() ? FIntPoint(TileMap->TileWidth, TileMap->TileHeight) : FIntPoint(16, 16);
}

bool FOnyxMapPainter::IsValidCell(const FIntPoint& Cell) const
{
	const FIntPoint Size = GetMapSize();
	return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Size.X && Cell.Y < Size.Y;
}

FVector FOnyxMapPainter::CellToWorld(const FIntPoint& Cell) const
{
	const APaperTileMapActor* Actor = FindMapActor();
	FVector Location = Actor ? Actor->GetRenderComponent()->GetTileCenterPosition(Cell.X, Cell.Y, 0, true) : FVector::ZeroVector;
	Location.Y = 0.f;
	return Location;
}

bool FOnyxMapPainter::WorldToCell(const FVector& Location, FIntPoint& OutCell) const
{
	const APaperTileMapActor* Actor = FindMapActor();
	if (!Actor || !TileMap.IsValid())
	{
		return false;
	}
	const FVector Local = Actor->GetRenderComponent()->GetComponentTransform().InverseTransformPosition(Location);
	TileMap->GetTileCoordinatesFromLocalSpacePosition(Local, OutCell.X, OutCell.Y);
	return IsValidCell(OutCell);
}

void FOnyxMapPainter::Resize(const FIntPoint& NewSize)
{
	ChangeMapStructure(INVTEXT("Resize Tile Map"), [&NewSize](UPaperTileMap& Map)
	{
		Map.ResizeMap(FMath::Clamp(NewSize.X, 1, 1024), FMath::Clamp(NewSize.Y, 1, 1024));
	});
}

// --- Tile layers ------------------------------------------------------------------------

void FOnyxMapPainter::SetLayer(int32 InLayer)
{
	Layer = FMath::Clamp(InLayer, 0, FMath::Max(GetLayerCount() - 1, 0));
}

int32 FOnyxMapPainter::GetLayerCount() const
{
	return TileMap.IsValid() ? TileMap->TileLayers.Num() : 0;
}

UPaperTileLayer* FOnyxMapPainter::GetTileLayer(int32 LayerIndex) const
{
	return TileMap.IsValid() && TileMap->TileLayers.IsValidIndex(LayerIndex) ? TileMap->TileLayers[LayerIndex].Get() : nullptr;
}

FText FOnyxMapPainter::GetLayerName(int32 LayerIndex) const
{
	const UPaperTileLayer* TileLayer = GetTileLayer(LayerIndex);
	return TileLayer ? TileLayer->LayerName : FText::GetEmpty();
}

bool FOnyxMapPainter::GetLayerCollides(int32 LayerIndex) const
{
	const UPaperTileLayer* TileLayer = GetTileLayer(LayerIndex);
	return TileLayer && TileLayer->GetLayerCollides();
}

FPaperTileInfo FOnyxMapPainter::GetCell(const FIntPoint& Cell, int32 LayerIndex) const
{
	const UPaperTileLayer* TileLayer = GetTileLayer(LayerIndex);
	return TileLayer && IsValidCell(Cell) ? TileLayer->GetCell(Cell.X, Cell.Y) : FPaperTileInfo();
}

void FOnyxMapPainter::ChangeMapStructure(const FText& Description, TFunctionRef<void(UPaperTileMap&)> Change)
{
	UPaperTileMap* Map = TileMap.Get();
	if (!Map)
	{
		return;
	}
	const FScopedTransaction Transaction(Description);
	Map->Modify();
	for (UPaperTileLayer* TileLayer : Map->TileLayers)
	{
		TileLayer->Modify();
	}
	Change(*Map);
	SetLayer(Layer);
	bPendingFlush = true;
	FlushChanges();
}

void FOnyxMapPainter::AddLayer()
{
	ChangeMapStructure(INVTEXT("Add Tile Layer"), [](UPaperTileMap& Map)
	{
		UPaperTileLayer* NewLayer = Map.AddNewLayer(0); // in front of the others
		NewLayer->SetLayerCollides(false);
	});
	Layer = 0;
}

void FOnyxMapPainter::RenameLayer(int32 LayerIndex, const FText& Name)
{
	UPaperTileLayer* TileLayer = GetTileLayer(LayerIndex);
	if (TileLayer && !Name.IsEmpty())
	{
		const FScopedTransaction Transaction(INVTEXT("Rename Tile Layer"));
		TileLayer->Modify();
		TileLayer->LayerName = Name;
		TileMap->MarkPackageDirty();
	}
}

void FOnyxMapPainter::SetLayerCollides(int32 LayerIndex, bool bCollides)
{
	if (UPaperTileLayer* TileLayer = GetTileLayer(LayerIndex))
	{
		ChangeMapStructure(bCollides ? INVTEXT("Enable Layer Collision") : INVTEXT("Disable Layer Collision"), [TileLayer, bCollides](UPaperTileMap&)
		{
			TileLayer->SetLayerCollides(bCollides);
		});
	}
}

void FOnyxMapPainter::MoveLayer(int32 LayerIndex, int32 Direction)
{
	const int32 Target = LayerIndex + Direction;
	if (!GetTileLayer(LayerIndex) || !GetTileLayer(Target))
	{
		return;
	}
	const int32 Active = Layer;
	ChangeMapStructure(INVTEXT("Reorder Tile Layers"), [LayerIndex, Target](UPaperTileMap& Map)
	{
		Map.TileLayers.Swap(LayerIndex, Target);
	});
	Layer = Active == LayerIndex ? Target : (Active == Target ? LayerIndex : Active);
}

void FOnyxMapPainter::DeleteLayer(int32 LayerIndex)
{
	if (!GetTileLayer(LayerIndex) || GetLayerCount() <= 1)
	{
		return;
	}
	ChangeMapStructure(INVTEXT("Delete Tile Layer"), [LayerIndex](UPaperTileMap& Map)
	{
		Map.TileLayers.RemoveAt(LayerIndex);
	});
}

// --- Tile painting ----------------------------------------------------------------------

void FOnyxMapPainter::BeginStroke(const FText& Description)
{
	EndStroke();
	Stroke = MakeUnique<FScopedTransaction>(Description);
	if (TileMap.IsValid())
	{
		TileMap->Modify();
	}
}

void FOnyxMapPainter::EndStroke()
{
	FlushChanges();
	TouchedLayers.Reset();
	Stroke.Reset();
}

void FOnyxMapPainter::TouchLayer(UPaperTileLayer* TileLayer)
{
	if (!TouchedLayers.Contains(TileLayer))
	{
		TileLayer->Modify();
		TouchedLayers.Add(TileLayer);
	}
	bPendingFlush = true;
}

bool FOnyxMapPainter::PaintTile(const FIntPoint& Cell, const FPaperTileInfo& Tile)
{
	UPaperTileLayer* TileLayer = GetTileLayer(Layer);
	if (!TileLayer || !IsValidCell(Cell) || TileLayer->GetCell(Cell.X, Cell.Y) == Tile)
	{
		return false;
	}
	TouchLayer(TileLayer);
	TileLayer->SetCell(Cell.X, Cell.Y, Tile);
	return true;
}

void FOnyxMapPainter::PaintStamp(const FIntPoint& Cell, const FOnyxStamp& Stamp)
{
	const FIntPoint PaintSize = Stamp.GetPaintSize();
	for (int32 Y = 0; Y < PaintSize.Y; ++Y)
	{
		for (int32 X = 0; X < PaintSize.X; ++X)
		{
			PaintTile(Cell + FIntPoint(X, Y), Stamp.GetTile(X, Y, Cell + FIntPoint(X, Y)));
		}
	}
}

void FOnyxMapPainter::PaintTerrain(const FIntPoint& Cell, UPaperTileSet* TileSet, int32 Terrain, bool bErase)
{
	UPaperTileLayer* TileLayer = GetTileLayer(Layer);
	if (!TileLayer || !TileSet || !IsValidCell(Cell) || Terrain < 0 || Terrain >= FOnyxTerrains::GetCount(TileSet))
	{
		return;
	}
	auto IsTerrain = [&](const FIntPoint& At)
	{
		if (!IsValidCell(At))
		{
			return false;
		}
		const FPaperTileInfo Tile = TileLayer->GetCell(At.X, At.Y);
		return Tile.TileSet == TileSet && FOnyxTerrains::IsTerrainTile(TileSet, Tile.GetTileIndex(), Terrain);
	};

	// 1. Mark (or clear) the cell. A full tile stands in until the cell is resolved below.
	if (bErase)
	{
		if (!IsTerrain(Cell))
		{
			return;
		}
		PaintTile(Cell, FPaperTileInfo());
	}
	else
	{
		FPaperTileInfo Fill;
		Fill.TileSet = TileSet;
		Fill.PackedTileIndex = FMath::Max(FOnyxTerrains::FindTile(TileSet, Terrain, FOnyxTerrains::All), 0);
		PaintTile(Cell, Fill);
	}

	// 2. Re-pick the tile of the cell and its neighbours from who around them has the terrain.
	for (int32 DY = -1; DY <= 1; ++DY)
	{
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			const FIntPoint Target = Cell + FIntPoint(DX, DY);
			if (!IsTerrain(Target))
			{
				continue;
			}
			bool Neighbours[3][3];
			for (int32 NY = -1; NY <= 1; ++NY)
			{
				for (int32 NX = -1; NX <= 1; ++NX)
				{
					Neighbours[NY + 1][NX + 1] = IsTerrain(Target + FIntPoint(NX, NY));
				}
			}
			const int32 TileIndex = FOnyxTerrains::FindTile(TileSet, Terrain, FOnyxTerrains::CornersFromNeighbours(Neighbours));
			if (TileIndex != INDEX_NONE)
			{
				FPaperTileInfo Tile;
				Tile.TileSet = TileSet;
				Tile.PackedTileIndex = TileIndex;
				PaintTile(Target, Tile);
			}
		}
	}
}

int32 FOnyxMapPainter::Fill(const FIntPoint& Start, const FOnyxStamp& Stamp, bool bErase)
{
	UPaperTileLayer* TileLayer = GetTileLayer(Layer);
	if (!TileLayer || !IsValidCell(Start))
	{
		return 0;
	}
	const FPaperTileInfo Target = TileLayer->GetCell(Start.X, Start.Y);
	TouchLayer(TileLayer);

	TSet<FIntPoint> Area;
	TArray<FIntPoint> Open = { Start };
	while (Open.Num() > 0 && Area.Num() < MaxFillCells)
	{
		const FIntPoint Cell = Open.Pop(EAllowShrinking::No);
		if (!IsValidCell(Cell) || Area.Contains(Cell) || !(TileLayer->GetCell(Cell.X, Cell.Y) == Target))
		{
			continue;
		}
		Area.Add(Cell);
		Open.Append({ Cell + FIntPoint(1, 0), Cell - FIntPoint(1, 0), Cell + FIntPoint(0, 1), Cell - FIntPoint(0, 1) });
	}
	for (const FIntPoint& Cell : Area)
	{
		TileLayer->SetCell(Cell.X, Cell.Y, bErase ? FPaperTileInfo() : Stamp.GetTile(Cell.X - Start.X, Cell.Y - Start.Y, Cell));
	}
	return Area.Num();
}

void FOnyxMapPainter::FlushChanges()
{
	if (bPendingFlush && TileMap.IsValid())
	{
		bPendingFlush = false;
		TileMap->PostEditChange(); // re-registers every component showing the map (render data)
		TileMap->MarkPackageDirty();
		if (const APaperTileMapActor* Actor = FindMapActor())
		{
			if (UOnyxTileCollisionComponent* Collision = Actor->FindComponentByClass<UOnyxTileCollisionComponent>())
			{
				Collision->Rebuild();
			}
		}
	}
}

EOnyxTileCollision FOnyxMapPainter::GetCellCollision(const FIntPoint& Cell) const
{
	return FOnyxTileCollision::ResolveCell(TileMap.Get(), Cell.X, Cell.Y);
}

void FOnyxMapPainter::SetTileCollision(const FOnyxStamp& Stamp, EOnyxTileCollision Type)
{
	UPaperTileSet* TileSet = Stamp.TileSet.Get();
	if (!TileSet)
	{
		return;
	}
	const FScopedTransaction Transaction(FText::Format(INVTEXT("Set Tile Collision: {0}"), FOnyxTileCollision::GetDisplayName(Type)));
	TileSet->Modify();
	for (int32 Y = 0; Y < Stamp.Size.Y; ++Y)
	{
		for (int32 X = 0; X < Stamp.Size.X; ++X)
		{
			const FPaperTileInfo Tile = Stamp.GetSourceTile(X, Y);
			if (Tile.IsValid())
			{
				FOnyxTileCollision::Set(TileSet, Tile.GetTileIndex(), Type);
			}
		}
	}
	TileSet->PostEditChange();
	TileSet->MarkPackageDirty();
	RebuildLevelCollision();
}

void FOnyxMapPainter::AutoAssignCollision(UPaperTileSet* TileSet)
{
	const FScopedTransaction Transaction(INVTEXT("Auto Tile Collision"));
	FOnyxTileBuilder::AutoAssignCollision(TileSet);
	RebuildLevelCollision();
}

void FOnyxMapPainter::RebuildLevelCollision()
{
	if (UWorld* World = EditorWorld())
	{
		for (TActorIterator<APaperTileMapActor> It(World); It; ++It)
		{
			if (UOnyxTileCollisionComponent* Collision = It->FindComponentByClass<UOnyxTileCollisionComponent>())
			{
				Collision->Rebuild();
			}
		}
	}
}

// --- Object layers -----------------------------------------------------------------------

float FOnyxMapPainter::GetObjectLayerDepth(FName ObjectLayer)
{
	if (ObjectLayer == FOnyxBrushLibrary::BackgroundLayer)
	{
		return Onyx2D::BackgroundObjectDepth;
	}
	return ObjectLayer == FOnyxBrushLibrary::DecorLayer ? Onyx2D::ForegroundObjectDepth : 0.f;
}

FName FOnyxMapPainter::GetObjectLayer(const AActor* Actor)
{
	for (const FName& Tag : Actor->Tags)
	{
		const FString TagString = Tag.ToString();
		if (TagString.StartsWith(LayerTagPrefix))
		{
			return *TagString.RightChop(LayerTagPrefix.Len());
		}
	}
	return FOnyxBrushLibrary::EntitiesLayer; // placed before object layers existed
}

void FOnyxMapPainter::ForEachObject(TFunctionRef<void(const FOnyxPlacedObject&)> Visit) const
{
	UWorld* World = EditorWorld();
	if (!World || !FindMapActor())
	{
		return;
	}
	const FOnyxBrushLibrary& Library = FOnyxBrushLibrary::Get();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		FOnyxPlacedObject Object;
		const TSharedPtr<FOnyxPainterBrush> Brush = Library.FindFor(*It);
		if (Brush.IsValid() && WorldToCell(ObjectAnchor(*It, *Brush), Object.Cell))
		{
			Object.Actor = *It;
			Object.Layer = GetObjectLayer(*It);
			Visit(Object);
		}
	}
}

AActor* FOnyxMapPainter::FindObject(const FIntPoint& Cell, FName ObjectLayer) const
{
	AActor* Found = nullptr;
	ForEachObject([&](const FOnyxPlacedObject& Object)
	{
		if (!Found && Object.Cell == Cell && Object.Layer == ObjectLayer)
		{
			Found = Object.Actor;
		}
	});
	return Found;
}

AActor* FOnyxMapPainter::PlaceObject(const FIntPoint& Cell, const FOnyxPainterBrush& Brush, FName ObjectLayer)
{
	UWorld* World = EditorWorld();
	if (!World || !IsValidCell(Cell) || !FindMapActor() || FindObject(Cell, ObjectLayer))
	{
		return nullptr;
	}
	FVector Location = CellToWorld(Cell);
	Location.Y = GetObjectLayerDepth(ObjectLayer);
	AActor* Actor = FOnyxBrushLibrary::Get().Spawn(Brush, World, Location);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->Tags.AddUnique(*(LayerTagPrefix + ObjectLayer.ToString()));

	// Characters, player starts and your own objects stand on the bottom of the cell, centred.
	if (Brush.bStandOnFloor)
	{
		FVector Origin;
		FVector Extent;
		if (GetStandingBounds(Actor, Origin, Extent))
		{
			// Bottom edge straight from the tile map component, so actor scale and rotation are respected.
			const float CellBottom = FindMapActor()->GetRenderComponent()->GetTileCornerPosition(Cell.X, Cell.Y + 1, 0, true).Z;
			Actor->SetActorLocation(Actor->GetActorLocation() + FVector(Location.X - Origin.X, 0.f, CellBottom - (Origin.Z - Extent.Z)));
		}
	}
	return Actor;
}

bool FOnyxMapPainter::EraseObject(const FIntPoint& Cell, FName ObjectLayer)
{
	AActor* Actor = FindObject(Cell, ObjectLayer);
	UWorld* World = EditorWorld();
	if (!Actor || !World)
	{
		return false;
	}
	Actor->Modify();
	return World->EditorDestroyActor(Actor, true);
}

bool FOnyxMapPainter::MoveObject(AActor* Actor, const FIntPoint& Cell)
{
	const TSharedPtr<FOnyxPainterBrush> Brush = FOnyxBrushLibrary::Get().FindFor(Actor);
	FIntPoint Current;
	if (!Actor || !Brush.IsValid() || !IsValidCell(Cell) || !WorldToCell(ObjectAnchor(Actor, *Brush), Current) || Current == Cell || FindObject(Cell, GetObjectLayer(Actor)))
	{
		return false;
	}
	Actor->Modify();
	// Keep the object's offset inside its cell (custom objects stand on the cell bottom).
	Actor->SetActorLocation(Actor->GetActorLocation() + (CellToWorld(Cell) - CellToWorld(Current)));
	return true;
}
