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

	int32 Priority(EOnyxTileCollision Type)
	{
		switch (Type)
		{
		case EOnyxTileCollision::Solid: return 3;
		case EOnyxTileCollision::OneWay: return 2;
		case EOnyxTileCollision::Hazard: return 1;
		default: return 0;
		}
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
	// Tile sets authored in the Paper2D editor: any collision shape counts as solid.
	return Metadata->HasCollision() ? EOnyxTileCollision::Solid : EOnyxTileCollision::None;
}

FText FOnyxTileCollision::GetDisplayName(EOnyxTileCollision Type)
{
	switch (Type)
	{
	case EOnyxTileCollision::Solid: return INVTEXT("Solid");
	case EOnyxTileCollision::OneWay: return INVTEXT("One-Way");
	case EOnyxTileCollision::Hazard: return INVTEXT("Hazard");
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
	switch (Type)
	{
	case EOnyxTileCollision::Solid: Metadata->UserDataName = SolidTag; break;
	case EOnyxTileCollision::OneWay: Metadata->UserDataName = OneWayTag; break;
	case EOnyxTileCollision::Hazard: Metadata->UserDataName = HazardTag; break;
	default: Metadata->UserDataName = NoneTag; break;
	}

	// Keep Paper2D's own shape in sync (its tile map editor and debug views show it).
	Metadata->CollisionData.Shapes.Reset();
	if (Type == EOnyxTileCollision::Solid)
	{
		const FVector2D Size(TileSet->GetTileSize());
		Metadata->CollisionData.AddRectangleShape(Size * 0.5f, Size);
	}
}
#endif
