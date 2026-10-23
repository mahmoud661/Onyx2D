// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxTileCollisionComponent.h"
#include "Lighting/OnyxLightingSubsystem.h"
#include "Combat/OnyxHitboxComponent.h"
#include "Components/BoxComponent.h"
#include "Core/OnyxWorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PaperTileMap.h"
#include "PaperTileMapComponent.h"
#include "World/OnyxCollisionMerge.h"

namespace
{
	TArray<bool> Mask(const TArray<EOnyxTileCollision>& Cells, EOnyxTileCollision Type)
	{
		TArray<bool> Result;
		Result.Reserve(Cells.Num());
		for (const EOnyxTileCollision Cell : Cells)
		{
			Result.Add(Cell == Type);
		}
		return Result;
	}
}

UOnyxTileCollisionComponent::UOnyxTileCollisionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

UPaperTileMapComponent* UOnyxTileCollisionComponent::FindTileMapComponent() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UPaperTileMapComponent>() : nullptr;
}

void UOnyxTileCollisionComponent::OnRegister()
{
	Super::OnRegister();
	Rebuild();
}

void UOnyxTileCollisionComponent::OnUnregister()
{
	ClearShapes();
	Super::OnUnregister();
}

#if WITH_EDITOR
void UOnyxTileCollisionComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	Rebuild();
}
#endif

void UOnyxTileCollisionComponent::ClearShapes()
{
	UpdateRegistry(false);
	if (UOnyxLightingSubsystem* Lighting = GetWorld() ? GetWorld()->GetSubsystem<UOnyxLightingSubsystem>() : nullptr)
	{
		Lighting->ClearOccluders(this);
	}
	for (UPrimitiveComponent* Shape : Shapes)
	{
		if (IsValid(Shape))
		{
			Shape->DestroyComponent();
		}
	}
	Shapes.Reset();
	OneWayShapes.Reset();
}

TArray<EOnyxTileCollision> UOnyxTileCollisionComponent::BuildCellGrid(const UPaperTileMapComponent& TileMapComponent, int32& OutWidth, int32& OutHeight) const
{
	const UPaperTileMap* Map = TileMapComponent.TileMap;
	OutWidth = Map ? Map->MapWidth : 0;
	OutHeight = Map ? Map->MapHeight : 0;
	TArray<EOnyxTileCollision> Cells;
	Cells.Reserve(OutWidth * OutHeight);
	for (int32 Y = 0; Y < OutHeight; ++Y)
	{
		for (int32 X = 0; X < OutWidth; ++X)
		{
			Cells.Add(FOnyxTileCollision::ResolveCell(Map, X, Y));
		}
	}
	return Cells;
}

EOnyxTileCollision UOnyxTileCollisionComponent::GetCellCollision(int32 X, int32 Y) const
{
	const UPaperTileMapComponent* TileMapComponent = FindTileMapComponent();
	return TileMapComponent ? FOnyxTileCollision::ResolveCell(TileMapComponent->TileMap, X, Y) : EOnyxTileCollision::None;
}

FBox UOnyxTileCollisionComponent::CellRectToLocalBox(const UPaperTileMapComponent& TileMapComponent, const FIntRect& Rect) const
{
	// Corners straight from Paper2D, so this stays right whatever the map's orientation or tile size.
	const FVector A = TileMapComponent.GetTileCornerPosition(Rect.Min.X, Rect.Min.Y, 0, false);
	const FVector B = TileMapComponent.GetTileCornerPosition(Rect.Max.X, Rect.Max.Y, 0, false);
	FBox Box(ForceInit);
	Box += FVector(A.X, -Thickness * 0.5f, A.Z);
	Box += FVector(B.X, Thickness * 0.5f, B.Z);
	return Box;
}

UPrimitiveComponent* UOnyxTileCollisionComponent::AddBox(UPaperTileMapComponent& TileMapComponent, TSubclassOf<UPrimitiveComponent> Class, const FBox& LocalBox)
{
	AActor* Owner = GetOwner();
	UBoxComponent* Box = NewObject<UBoxComponent>(Owner, Class, NAME_None, RF_Transient | RF_TextExportTransient | RF_DuplicateTransient);
	Box->SetupAttachment(&TileMapComponent);
	Box->SetMobility(TileMapComponent.Mobility);
	Box->SetRelativeLocation(LocalBox.GetCenter());
	Box->SetBoxExtent(LocalBox.GetExtent());
	Box->SetCanEverAffectNavigation(false);
	Box->bIsEditorOnly = false;
	Shapes.Add(Box);
	return Box;
}

void UOnyxTileCollisionComponent::Rebuild()
{
	ClearShapes();
	UPaperTileMapComponent* TileMapComponent = FindTileMapComponent();
	UWorld* World = GetWorld();
	if (!TileMapComponent || !TileMapComponent->TileMap || !World || HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}
	if (bReplacePaperCollision)
	{
		TileMapComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	int32 Width = 0;
	int32 Height = 0;
	const TArray<EOnyxTileCollision> Cells = BuildCellGrid(*TileMapComponent, Width, Height);

	const TArray<FIntRect> SolidRects = FOnyxCollisionMerge::MergeRectangles(Mask(Cells, EOnyxTileCollision::Solid), Width, Height);
	const FVector OneCell = CellRectToLocalBox(*TileMapComponent, FIntRect(0, 0, 1, 1)).GetSize();
	auto IsSlope = [&](int32 X, int32 Y)
	{
		return X >= 0 && Y >= 0 && X < Width && Y < Height
			&& (Cells[Y * Width + X] == EOnyxTileCollision::SlopeUp || Cells[Y * Width + X] == EOnyxTileCollision::SlopeDown);
	};
	auto IsSolid = [&](int32 X, int32 Y)
	{
		// Beyond the map's sides the ground carries on (no rim pokes out there).
		return X < 0 || Y < 0 || X >= Width || Y >= Height || Cells[Y * Width + X] == EOnyxTileCollision::Solid;
	};
	// Solid cells next to a slope tile: their rim (Solid Outset) would poke up through the ramp as a bump at every step,
	// so each of them gets it only on its open sides; all other solid cells merge into boxes as before.
	TArray<bool> NearSlope;
	NearSlope.Init(false, Width * Height);
	bool bAnyNear = false;
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			if (Cells[Y * Width + X] != EOnyxTileCollision::Solid)
			{
				continue;
			}
			for (int32 DY = -1; DY <= 1 && !NearSlope[Y * Width + X]; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					if (IsSlope(X + DX, Y + DY))
					{
						NearSlope[Y * Width + X] = true;
						bAnyNear = true;
						break;
					}
				}
			}
		}
	}
	TArray<bool> PlainSolid = Mask(Cells, EOnyxTileCollision::Solid);
	for (int32 Index = 0; Index < PlainSolid.Num(); ++Index)
	{
		PlainSolid[Index] = PlainSolid[Index] && !NearSlope[Index];
	}
	for (const FIntRect& Rect : bAnyNear ? FOnyxCollisionMerge::MergeRectangles(PlainSolid, Width, Height) : SolidRects)
	{
		const FBox Box = CellRectToLocalBox(*TileMapComponent, Rect).ExpandBy(FVector(OneCell.X * SolidOutset, 0.f, OneCell.Z * SolidOutset));
		UPrimitiveComponent* Solid = AddBox(*TileMapComponent, UBoxComponent::StaticClass(), Box);
		Solid->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Solid->RegisterComponent();
	}
	for (int32 Y = 0; bAnyNear && Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			if (!NearSlope[Y * Width + X])
			{
				continue;
			}
			// Local Z runs up while rows run down: the top side is Max.Z.
			FBox Box = CellRectToLocalBox(*TileMapComponent, FIntRect(X, Y, X + 1, Y + 1));
			const float RimX = OneCell.X * SolidOutset;
			const float RimZ = OneCell.Z * SolidOutset;
			auto Open = [&](int32 NX, int32 NY) { return !IsSolid(NX, NY) && !IsSlope(NX, NY); };
			Box.Max.Z += Open(X, Y - 1) ? RimZ : 0.f;
			Box.Min.Z -= Open(X, Y + 1) ? RimZ : 0.f;
			Box.Min.X -= Open(X - 1, Y) ? RimX : 0.f;
			Box.Max.X += Open(X + 1, Y) ? RimX : 0.f;
			UPrimitiveComponent* Solid = AddBox(*TileMapComponent, UBoxComponent::StaticClass(), Box);
			Solid->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Solid->RegisterComponent();
		}
	}
	const TArray<TPair<FVector, FVector>> Ramps = BuildRamps(*TileMapComponent, Cells, Width, Height);
	UpdateLightOccluders(TileMapComponent, SolidRects, Ramps);

	for (const FIntRect& Rect : FOnyxCollisionMerge::MergeRows(Mask(Cells, EOnyxTileCollision::OneWay), Width, Height))
	{
		FBox Box = CellRectToLocalBox(*TileMapComponent, Rect);
		Box.Min.Z = Box.Max.Z - (Box.Max.Z - Box.Min.Z) * OneWayHeight; // keep only the top slice
		UPrimitiveComponent* OneWay = AddBox(*TileMapComponent, UBoxComponent::StaticClass(), Box);
		OneWay->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		OneWay->RegisterComponent();
		OneWayShapes.Add(OneWay);
	}

	for (const FIntRect& Rect : FOnyxCollisionMerge::MergeRectangles(Mask(Cells, EOnyxTileCollision::Hazard), Width, Height))
	{
		FBox Box = CellRectToLocalBox(*TileMapComponent, Rect);
		const FVector CellSize = CellRectToLocalBox(*TileMapComponent, FIntRect(0, 0, 1, 1)).GetSize();
		Box = Box.ExpandBy(FVector(-CellSize.X * HazardInset, 0.f, -CellSize.Z * HazardInset));
		UOnyxHitboxComponent* Hazard = Cast<UOnyxHitboxComponent>(AddBox(*TileMapComponent, UOnyxHitboxComponent::StaticClass(), Box));
		Hazard->bContinuous = true;
		Hazard->Damage = HazardDamage;
		Hazard->Knockback = FVector2D(300.f, 700.f);
		Hazard->HitStopDuration = 0.f;
		Hazard->DamageType = TEXT("Hazard");
		Hazard->RegisterComponent();
	}

	// Edge tiles: a thin wall along each blocked side, joined along rows (north and south sides) and columns (east, west).
	{
		const UPaperTileMap* Map = TileMapComponent->TileMap;
		TArray<uint8> Edges;
		Edges.SetNumZeroed(Width * Height);
		bool bAny = false;
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				if (Cells[Y * Width + X] != EOnyxTileCollision::Solid)
				{
					Edges[Y * Width + X] = FOnyxTileCollision::ResolveCellEdges(Map, X, Y);
					bAny |= Edges[Y * Width + X] != 0;
				}
			}
		}
		auto AddWall = [&](const FIntRect& Area, uint8 Side)
		{
			FBox Box = CellRectToLocalBox(*TileMapComponent, Area);
			const float Across = (Side == EOnyxTileEdge::North || Side == EOnyxTileEdge::South) ? OneCell.Z * EdgeThickness : OneCell.X * EdgeThickness;
			switch (Side)
			{
			case EOnyxTileEdge::North: Box.Min.Z = Box.Max.Z - Across; break;
			case EOnyxTileEdge::South: Box.Max.Z = Box.Min.Z + Across; break;
			case EOnyxTileEdge::West:  Box.Max.X = Box.Min.X + Across; break;
			default:                   Box.Min.X = Box.Max.X - Across; break;
			}
			UPrimitiveComponent* Wall = AddBox(*TileMapComponent, UBoxComponent::StaticClass(), Box);
			Wall->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Wall->RegisterComponent();
		};
		for (int32 Pass = 0; bAny && Pass < 4; ++Pass)
		{
			const uint8 Side = Pass == 0 ? EOnyxTileEdge::North : Pass == 1 ? EOnyxTileEdge::South : Pass == 2 ? EOnyxTileEdge::West : EOnyxTileEdge::East;
			const bool bAlongRows = Side == EOnyxTileEdge::North || Side == EOnyxTileEdge::South;
			const int32 Lines = bAlongRows ? Height : Width, Length = bAlongRows ? Width : Height;
			for (int32 Line = 0; Line < Lines; ++Line)
			{
				int32 Start = -1;
				for (int32 At = 0; At <= Length; ++At)
				{
					const bool bBlocked = At < Length && (Edges[bAlongRows ? Line * Width + At : At * Width + Line] & Side);
					if (bBlocked && Start < 0)
					{
						Start = At;
					}
					else if (!bBlocked && Start >= 0)
					{
						AddWall(bAlongRows ? FIntRect(Start, Line, At, Line + 1) : FIntRect(Line, Start, Line + 1, At), Side);
						Start = -1;
					}
				}
			}
		}
	}

	if (bBlockLevelEdges)
	{
		// One cell wide, just outside the map, from the bottom row up past the top.
		for (const int32 Column : { -1, Width })
		{
			const FIntRect Rect(Column, -EdgeWallOverhang, Column + 1, Height);
			UPrimitiveComponent* Wall = AddBox(*TileMapComponent, UBoxComponent::StaticClass(), CellRectToLocalBox(*TileMapComponent, Rect));
			Wall->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Wall->RegisterComponent();
		}
	}

	UpdateRegistry(true);
}

TArray<TPair<FVector, FVector>> UOnyxTileCollisionComponent::BuildRamps(UPaperTileMapComponent& TileMapComponent, const TArray<EOnyxTileCollision>& Cells, int32 Width, int32 Height)
{
	TArray<TPair<FVector, FVector>> Surfaces;
	const FVector OneCell = CellRectToLocalBox(TileMapComponent, FIntRect(0, 0, 1, 1)).GetSize();
	auto Is = [&](int32 X, int32 Y, EOnyxTileCollision Type) { return X >= 0 && Y >= 0 && X < Width && Y < Height && Cells[Y * Width + X] == Type; };
	auto Corner = [&](int32 X, int32 Y) { return TileMapComponent.GetTileCornerPosition(X, Y, 0, false); };
	for (const EOnyxTileCollision Type : { EOnyxTileCollision::SlopeDown, EOnyxTileCollision::SlopeUp })
	{
		// Going down to the right a row of them steps (+1, +1) (rows run down); going up, (+1, -1).
		const int32 Step = Type == EOnyxTileCollision::SlopeDown ? 1 : -1;
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				if (!Is(X, Y, Type) || Is(X - 1, Y - Step, Type))
				{
					continue; // not a slope tile, or not the first of its row
				}
				int32 Count = 1;
				while (Is(X + Count, Y + Count * Step, Type))
				{
					++Count;
				}
				// The line through the row: from the first tile's top-left corner to the last one's bottom-right (going down),
				// or from the first's bottom-left to the last's top-right (going up); raised by the rim like the flat tops.
				const FVector Rim(0.f, 0.f, OneCell.Z * SolidOutset);
				const FVector A = (Type == EOnyxTileCollision::SlopeDown ? Corner(X, Y) : Corner(X, Y + 1)) + Rim;
				const FVector B = (Type == EOnyxTileCollision::SlopeDown ? Corner(X + Count, Y + Count) : Corner(X + Count, Y + 1 - Count)) + Rim;
				const FVector Along = (B - A).GetSafeNormal();
				FVector Up = FVector::CrossProduct(Along, FVector(0.f, 1.f, 0.f)).GetSafeNormal();
				if (Up.Z < 0.f)
				{
					Up = -Up;
				}
				// A box under the line, deep enough to fill down to the stepped solid tiles beneath it.
				const float Depth = OneCell.Z;
				UBoxComponent* Ramp = NewObject<UBoxComponent>(GetOwner(), UBoxComponent::StaticClass(), NAME_None, RF_Transient | RF_TextExportTransient | RF_DuplicateTransient);
				Ramp->SetupAttachment(&TileMapComponent);
				Ramp->SetMobility(TileMapComponent.Mobility);
				Ramp->SetRelativeLocation((A + B) * 0.5f - Up * Depth * 0.5f);
				Ramp->SetRelativeRotation(FRotationMatrix::MakeFromXZ(Along, Up).Rotator());
				Ramp->SetBoxExtent(FVector(FVector::Dist(A, B) * 0.5f, Thickness * 0.5f, Depth * 0.5f));
				Ramp->SetCanEverAffectNavigation(false);
				Ramp->bIsEditorOnly = false;
				Ramp->ComponentTags.Add(TEXT("Onyx.Slope"));
				Ramp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
				Ramp->RegisterComponent();
				Shapes.Add(Ramp);
				Surfaces.Emplace(A, B);
			}
		}
	}
	return Surfaces;
}

void UOnyxTileCollisionComponent::UpdateLightOccluders(const UPaperTileMapComponent* TileMapComponent, const TArray<FIntRect>& SolidRects, const TArray<TPair<FVector, FVector>>& RampSurfaces)
{
	UOnyxLightingSubsystem* Lighting = GetWorld() ? GetWorld()->GetSubsystem<UOnyxLightingSubsystem>() : nullptr;
	if (!Lighting || !TileMapComponent)
	{
		return;
	}
	if (!bCastLightShadows)
	{
		Lighting->ClearOccluders(this);
		return;
	}
	const FTransform& ToWorld = TileMapComponent->GetComponentTransform();
	auto Corner = [&](int32 X, int32 Y)
	{
		const FVector World = ToWorld.TransformPosition(TileMapComponent->GetTileCornerPosition(X, Y, 0, false));
		return FVector2D(World.X, World.Z);
	};
	const float CellSize = FMath::Abs(Corner(1, 0).X - Corner(0, 0).X);
	const float Soak = LightPenetration * CellSize;
	TArray<FOnyxOccluderSegment> Segments;
	Segments.Reserve(SolidRects.Num() * 4);
	for (const FIntRect& Rect : SolidRects)
	{
		const FVector2D TopLeft = Corner(Rect.Min.X, Rect.Min.Y);
		const FVector2D TopRight = Corner(Rect.Max.X, Rect.Min.Y);
		const FVector2D BottomRight = Corner(Rect.Max.X, Rect.Max.Y);
		const FVector2D BottomLeft = Corner(Rect.Min.X, Rect.Max.Y);
		Segments.Add({ TopLeft, TopRight, Soak });
		Segments.Add({ TopRight, BottomRight, Soak });
		Segments.Add({ BottomRight, BottomLeft, Soak });
		Segments.Add({ BottomLeft, TopLeft, Soak });
	}
	for (const TPair<FVector, FVector>& Surface : RampSurfaces)
	{
		const FVector A = ToWorld.TransformPosition(Surface.Key);
		const FVector B = ToWorld.TransformPosition(Surface.Value);
		Segments.Add({ FVector2D(A.X, A.Z), FVector2D(B.X, B.Z), Soak });
	}
	Lighting->SetOccluders(this, MoveTemp(Segments));
}

void UOnyxTileCollisionComponent::UpdateRegistry(bool bRegister)
{
	UWorld* World = GetWorld();
	UOnyxWorldSubsystem* Registry = World && World->IsGameWorld() ? World->GetSubsystem<UOnyxWorldSubsystem>() : nullptr;
	if (!Registry)
	{
		return;
	}
	for (UPrimitiveComponent* OneWay : OneWayShapes)
	{
		if (IsValid(OneWay))
		{
			bRegister ? Registry->RegisterOneWaySurface(OneWay) : Registry->UnregisterOneWaySurface(OneWay);
		}
	}

	const UPaperTileMapComponent* TileMapComponent = FindTileMapComponent();
	if (bRegister && TileMapComponent && TileMapComponent->TileMap)
	{
		const UPaperTileMap* Map = TileMapComponent->TileMap;
		const FTransform& ToWorld = TileMapComponent->GetComponentTransform();
		FBox Area(ForceInit);
		Area += ToWorld.TransformPosition(TileMapComponent->GetTileCornerPosition(0, 0, 0, false));
		Area += ToWorld.TransformPosition(TileMapComponent->GetTileCornerPosition(Map->MapWidth, Map->MapHeight, 0, false));
		Registry->SetLevelBounds(this, Area);
	}
	else
	{
		Registry->ClearLevelBounds(this);
	}
}
