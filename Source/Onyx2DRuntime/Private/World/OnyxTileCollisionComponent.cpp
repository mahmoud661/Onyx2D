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
	UpdateLightOccluders(TileMapComponent, SolidRects);
	for (const FIntRect& Rect : SolidRects)
	{
		UPrimitiveComponent* Solid = AddBox(*TileMapComponent, UBoxComponent::StaticClass(), CellRectToLocalBox(*TileMapComponent, Rect));
		Solid->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Solid->RegisterComponent();
	}

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

void UOnyxTileCollisionComponent::UpdateLightOccluders(const UPaperTileMapComponent* TileMapComponent, const TArray<FIntRect>& SolidRects)
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
