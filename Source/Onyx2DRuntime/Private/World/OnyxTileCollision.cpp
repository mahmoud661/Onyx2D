// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxTileCollision.h"
#include "PaperTileLayer.h"
#include "PaperTileMap.h"
#include "PaperTileSet.h"

namespace
{
	const FName SolidTag(TEXT("Onyx.Solid"));
	const FName OneWayTag(TEXT("Onyx.OneWay"));
	const FName HazardTag(TEXT("Onyx.Hazard"));
	const FName NoneTag(TEXT("Onyx.None"));
	const FName SlopeUpTag(TEXT("Onyx.SlopeUp"));
	const FName SlopeDownTag(TEXT("Onyx.SlopeDown"));
	/** Edge tiles: "Onyx.Edge." and the sides blocked as a number (EOnyxTileEdge bits). */
	const TCHAR* EdgePrefix = TEXT("Onyx.Edge.");

	int32 Priority(EOnyxTileCollision Type)
	{
		switch (Type)
		{
		case EOnyxTileCollision::Solid: return 5;
		case EOnyxTileCollision::SlopeUp: return 4;
		case EOnyxTileCollision::SlopeDown: return 4;
		case EOnyxTileCollision::OneWay: return 3;
		case EOnyxTileCollision::Hazard: return 2;
		case EOnyxTileCollision::Edge: return 1;
		default: return 0;
		}
	}

	/** The sides in an Edge tag, -1 when it isn't one. */
	int32 EdgesInTag(FName Tag)
	{
		const FString Name = Tag.ToString();
		return Name.StartsWith(EdgePrefix) ? FCString::Atoi(*Name.Mid(FCString::Strlen(EdgePrefix))) & EOnyxTileEdge::All : -1;
	}
}

EOnyxTileCollision FOnyxTileCollision::ResolveCell(const UPaperTileMap* Map, int32 X, int32 Y)
{
	EOnyxTileCollision Result = EOnyxTileCollision::None;
	if (!Map || X < 0 || Y < 0 || X >= Map->MapWidth || Y >= Map->MapHeight)
	{
		return Result;
	}
	for (const UPaperTileLayer* Layer : Map->TileLayers)
	{
		if (!Layer || !Layer->GetLayerCollides())
		{
			continue;
		}
		const FPaperTileInfo Tile = Layer->GetCell(X, Y);
		const EOnyxTileCollision Type = Tile.IsValid() ? Get(Tile.TileSet, Tile.GetTileIndex()) : EOnyxTileCollision::None;
		Result = Priority(Type) > Priority(Result) ? Type : Result;
	}
	return Result;
}

EOnyxTileCollision FOnyxTileCollision::Get(const UPaperTileSet* TileSet, int32 TileIndex)
{
	const FPaperTileMetadata* Metadata = TileSet ? TileSet->GetTileMetadata(TileIndex) : nullptr;
	if (!Metadata)
	{
		return EOnyxTileCollision::None;
	}
	if (Metadata->UserDataName == SolidTag)
	{
		return EOnyxTileCollision::Solid;
	}
	if (Metadata->UserDataName == OneWayTag)
	{
		return EOnyxTileCollision::OneWay;
	}
	if (Metadata->UserDataName == HazardTag)
	{
		return EOnyxTileCollision::Hazard;
	}
	if (Metadata->UserDataName == NoneTag)
	{
		return EOnyxTileCollision::None;
	}
	if (Metadata->UserDataName == SlopeUpTag)
	{
		return EOnyxTileCollision::SlopeUp;
	}
	if (Metadata->UserDataName == SlopeDownTag)
	{
		return EOnyxTileCollision::SlopeDown;
	}
	if (EdgesInTag(Metadata->UserDataName) >= 0)
	{
		return EOnyxTileCollision::Edge;
	}
	// Tile sets authored in the Paper2D editor: any collision shape counts as solid.
	return Metadata->HasCollision() ? EOnyxTileCollision::Solid : EOnyxTileCollision::None;
}

uint8 FOnyxTileCollision::GetEdges(const UPaperTileSet* TileSet, int32 TileIndex)
{
	const FPaperTileMetadata* Metadata = TileSet ? TileSet->GetTileMetadata(TileIndex) : nullptr;
	return Metadata ? uint8(FMath::Max(EdgesInTag(Metadata->UserDataName), 0)) : uint8(EOnyxTileEdge::None);
}

uint8 FOnyxTileCollision::ResolveCellEdges(const UPaperTileMap* Map, int32 X, int32 Y)
{
	uint8 Edges = EOnyxTileEdge::None;
	if (!Map || X < 0 || Y < 0 || X >= Map->MapWidth || Y >= Map->MapHeight)
	{
		return Edges;
	}
	for (const UPaperTileLayer* Layer : Map->TileLayers)
	{
		if (Layer && Layer->GetLayerCollides())
		{
			const FPaperTileInfo Tile = Layer->GetCell(X, Y);
			Edges |= Tile.IsValid() ? GetEdges(Tile.TileSet, Tile.GetTileIndex()) : uint8(EOnyxTileEdge::None);
		}
	}
	return Edges;
}

FText FOnyxTileCollision::GetDisplayName(EOnyxTileCollision Type)
{
	switch (Type)
	{
	case EOnyxTileCollision::Solid: return INVTEXT("Solid");
	case EOnyxTileCollision::OneWay: return INVTEXT("One-Way");
	case EOnyxTileCollision::Hazard: return INVTEXT("Hazard");
	case EOnyxTileCollision::Edge: return INVTEXT("Edge");
	case EOnyxTileCollision::SlopeUp: return INVTEXT("Slope Up");
	case EOnyxTileCollision::SlopeDown: return INVTEXT("Slope Down");
	default: return INVTEXT("None");
	}
}

FLinearColor FOnyxTileCollision::GetColor(EOnyxTileCollision Type)
{
	switch (Type)
	{
	case EOnyxTileCollision::Solid: return FLinearColor(0.36f, 0.82f, 0.55f);
	case EOnyxTileCollision::OneWay: return FLinearColor(0.22f, 0.71f, 1.f);
	case EOnyxTileCollision::Hazard: return FLinearColor(1.f, 0.33f, 0.41f);
	case EOnyxTileCollision::Edge: return FLinearColor(1.f, 0.72f, 0.25f);
	case EOnyxTileCollision::SlopeUp:
	case EOnyxTileCollision::SlopeDown: return FLinearColor(0.62f, 0.88f, 0.36f);
	default: return FLinearColor(0.5f, 0.5f, 0.55f);
	}
}

#if WITH_EDITOR
void FOnyxTileCollision::Set(UPaperTileSet* TileSet, int32 TileIndex, EOnyxTileCollision Type)
{
	FPaperTileMetadata* Metadata = TileSet ? TileSet->GetMutableTileMetadata(TileIndex) : nullptr;
	if (!Metadata)
	{
		return;
	}
	if (Type == EOnyxTileCollision::Edge)
	{
		SetEdges(TileSet, TileIndex, EOnyxTileEdge::All);
		return;
	}
	switch (Type)
	{
	case EOnyxTileCollision::Solid: Metadata->UserDataName = SolidTag; break;
	case EOnyxTileCollision::OneWay: Metadata->UserDataName = OneWayTag; break;
	case EOnyxTileCollision::Hazard: Metadata->UserDataName = HazardTag; break;
	case EOnyxTileCollision::SlopeUp: Metadata->UserDataName = SlopeUpTag; break;
	case EOnyxTileCollision::SlopeDown: Metadata->UserDataName = SlopeDownTag; break;
	default: Metadata->UserDataName = NoneTag; break;
	}

	// Keep Paper2D's own shape in sync (its tile map editor and debug views show it).
	Metadata->CollisionData.Shapes.Reset();
	if (Type == EOnyxTileCollision::Solid)
	{
		const FVector2D Size(TileSet->GetTileSize());
		Metadata->CollisionData.AddRectangleShape(Size * 0.5f, Size);
	}
	else if (Type == EOnyxTileCollision::SlopeUp || Type == EOnyxTileCollision::SlopeDown)
	{
		// The solid half under the diagonal (texture space: Y down), round the tile's middle.
		const FVector2D Half(FVector2D(TileSet->GetTileSize()) * 0.5f);
		FSpriteGeometryShape Shape;
		Shape.ShapeType = ESpriteShapeType::Polygon;
		Shape.BoxPosition = Half;
		Shape.Vertices = Type == EOnyxTileCollision::SlopeUp
			? TArray<FVector2D>{ FVector2D(Half.X, -Half.Y), FVector2D(Half.X, Half.Y), FVector2D(-Half.X, Half.Y) }
			: TArray<FVector2D>{ FVector2D(-Half.X, -Half.Y), FVector2D(-Half.X, Half.Y), FVector2D(Half.X, Half.Y) };
		Metadata->CollisionData.Shapes.Add(Shape);
	}
}

void FOnyxTileCollision::SetEdges(UPaperTileSet* TileSet, int32 TileIndex, uint8 Edges)
{
	FPaperTileMetadata* Metadata = TileSet ? TileSet->GetMutableTileMetadata(TileIndex) : nullptr;
	if (!Metadata)
	{
		return;
	}
	Edges &= EOnyxTileEdge::All;
	Metadata->UserDataName = Edges ? FName(*FString::Printf(TEXT("%s%d"), EdgePrefix, Edges)) : NoneTag;
	// Paper2D's own shapes: a thin strip along each blocked side (its views show them).
	Metadata->CollisionData.Shapes.Reset();
	const FVector2D Size(TileSet->GetTileSize());
	const float Strip = FMath::Max(1.f, FMath::RoundToFloat(Size.Y * 0.2f));
	if (Edges & EOnyxTileEdge::North) Metadata->CollisionData.AddRectangleShape(FVector2D(Size.X * 0.5f, Strip * 0.5f), FVector2D(Size.X, Strip));
	if (Edges & EOnyxTileEdge::South) Metadata->CollisionData.AddRectangleShape(FVector2D(Size.X * 0.5f, Size.Y - Strip * 0.5f), FVector2D(Size.X, Strip));
	if (Edges & EOnyxTileEdge::West) Metadata->CollisionData.AddRectangleShape(FVector2D(Strip * 0.5f, Size.Y * 0.5f), FVector2D(Strip, Size.Y));
	if (Edges & EOnyxTileEdge::East) Metadata->CollisionData.AddRectangleShape(FVector2D(Size.X - Strip * 0.5f, Size.Y * 0.5f), FVector2D(Strip, Size.Y));
}
#endif
