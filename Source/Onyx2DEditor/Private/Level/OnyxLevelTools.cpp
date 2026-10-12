// Copyright Onyx2D. All Rights Reserved.

#include "Level/OnyxLevelTools.h"
#include "Audio/OnyxLevelAudio.h"
#include "Builders/OnyxTileBuilder.h"
#include "PaperTileMapActor.h"
#include "World/OnyxTileCollisionComponent.h"
#include "Core/OnyxGameMode.h"
#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Lighting/OnyxLightingEnvironment.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "LevelEditorViewport.h"
#include "Lighting/OnyxLight2D.h"
#include "ScopedTransaction.h"
#include "World/OnyxLevelBuilder.h"

namespace
{
	constexpr float PlacementGrid = 50.f;

	template <class T>
	bool LevelHas(UWorld* World)
	{
		return TActorIterator<T>(World) ? true : false;
	}

	AActor* Spawn(UWorld* World, UClass* Class, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		return GEditor->AddActor(World->GetCurrentLevel(), Class, FTransform(Rotation, Location), true, RF_Transactional, false);
	}
}

FVector FOnyxLevelTools::GetViewportFocusOnPlane()
{
	FVector Point = FVector::ZeroVector;
	if (const FLevelEditorViewportClient* Viewport = GCurrentLevelEditingViewportClient)
	{
		const FVector Origin = Viewport->GetViewLocation();
		const FVector Direction = Viewport->GetViewRotation().Vector();
		Point = FMath::Abs(Direction.Y) > KINDA_SMALL_NUMBER && (-Origin.Y / Direction.Y) > 0.f
			? Origin + Direction * (-Origin.Y / Direction.Y)
			: FVector(Origin.X, 0.f, Origin.Z);
	}
	return FVector(FMath::GridSnap(Point.X, PlacementGrid), 0.f, FMath::GridSnap(Point.Z, PlacementGrid));
}

bool FOnyxLevelTools::IsLevel2D(UWorld* World)
{
	return World && (LevelHas<APaperTileMapActor>(World) || LevelHas<AOnyxLevelBuilder>(World) || LevelHas<AOnyxLightingEnvironment>(World));
}

FBox FOnyxLevelTools::GetLevelBounds2D(UWorld* World)
{
	FBox Bounds(ForceInit);
	if (!World)
	{
		return Bounds;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->IsA<APaperTileMapActor>() || It->IsA<AOnyxLevelBuilder>())
		{
			// Visible parts only: a Level Builder's background block is far wider than its level.
			const FBox ActorBounds = It->IsA<AOnyxLevelBuilder>() ? CastChecked<AOnyxLevelBuilder>(*It)->GetLayoutBounds() : It->GetComponentsBoundingBox(false);
			Bounds += ActorBounds.IsValid ? ActorBounds : FBox(ForceInit);
		}
	}
	return Bounds;
}

void FOnyxLevelTools::ShowSideView(const FBox& Area)
{
	if (!GEditor)
	{
		return;
	}
	for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
	{
		if (!Client->IsPerspective() && Client->GetViewportType() != LVT_OrthoXZ)
		{
			continue; // a viewport the user set to another side or top view stays
		}
		Client->SetViewportType(LVT_OrthoXZ);
		Client->SetViewModes(VMI_Lit, VMI_Lit);
		Client->SetRealtime(true); // water, flipbooks and lights animate
		const FIntPoint Size = Client->Viewport ? Client->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
		const float UnitsPerPixel = Size.X > 0 && Size.Y > 0 ? Client->GetOrthoUnitsPerPixel(Client->Viewport) : 0.f;
		if (Area.IsValid && UnitsPerPixel > KINDA_SMALL_NUMBER)
		{
			// Scale the zoom from what it shows now (how zoom maps to cm per pixel depends on editor settings), with a margin.
			const FVector Extent = Area.GetExtent();
			const float Wanted = 1.08f * FMath::Max(2.f * Extent.X / Size.X, 2.f * Extent.Z / Size.Y);
			Client->SetOrthoZoom(FMath::Clamp(Client->GetOrthoZoom() * Wanted / UnitsPerPixel, MIN_ORTHOZOOM, MAX_ORTHOZOOM));
			Client->SetViewLocation(FVector(Area.GetCenter().X, 1000.f, Area.GetCenter().Z));
		}
		Client->Invalidate();
	}
}

float FOnyxLevelTools::GetFreeMapLeftEdge(float TileWidthWorld, float GapWorld)
{
	UWorld* World = GetEditorWorld();
	float Right = -TNumericLimits<float>::Max();
	if (World)
	{
		for (TActorIterator<APaperTileMapActor> It(World); It; ++It)
		{
			FVector Origin, Extent;
			It->GetActorBounds(false, Origin, Extent);
			Right = FMath::Max(Right, Origin.X + Extent.X);
		}
	}
	if (Right == -TNumericLimits<float>::Max())
	{
		return 0.f;
	}
	const float Step = FMath::Max(TileWidthWorld, 1.f);
	return FMath::CeilToFloat((Right + GapWorld) / Step) * Step;
}

UWorld* FOnyxLevelTools::GetEditorWorld()
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

AActor* FOnyxLevelTools::PlaceInFrontOfCamera(UClass* Class)
{
	UWorld* World = FOnyxLevelTools::GetEditorWorld();
	if (!World || !Class)
	{
		return nullptr;
	}
	const FScopedTransaction Transaction(FText::Format(INVTEXT("Place {0}"), Class->GetDisplayNameText()));
	AActor* Actor = Spawn(World, Class, GetViewportFocusOnPlane());
	if (Actor)
	{
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(Actor, true, true);
	}
	return Actor;
}

TArray<FString> FOnyxLevelTools::SetupCurrentLevel()
{
	TArray<FString> Added;
	UWorld* World = FOnyxLevelTools::GetEditorWorld();
	if (!World)
	{
		return Added;
	}
	const FScopedTransaction Transaction(INVTEXT("Set Up Onyx2D Level"));

	if (AWorldSettings* Settings = World->GetWorldSettings())
	{
		if (Settings->DefaultGameMode != AOnyxGameMode::StaticClass())
		{
			Settings->Modify();
			Settings->DefaultGameMode = AOnyxGameMode::StaticClass();
			Added.Add(TEXT("Game mode override: Onyx Game Mode"));
		}
	}
	// No camera box here: the camera confines itself to the tile maps (Onyx Tile Collision).
	// Place Camera Zones only where a room needs its own framing.
	for (TActorIterator<APaperTileMapActor> It(World); It; ++It)
	{
		if (!It->FindComponentByClass<UOnyxTileCollisionComponent>())
		{
			FOnyxTileBuilder::EnsureTileCollision(*It);
			Added.Add(FString::Printf(TEXT("Tile collision on %s"), *It->GetActorLabel()));
		}
	}
	if (!LevelHas<APlayerStart>(World))
	{
		Spawn(World, APlayerStart::StaticClass(), FVector(0.f, 0.f, 150.f));
		Added.Add(TEXT("Player start"));
	}
	// One actor owns the level's look: key light, ambient, depth fog and post process (Lighting Mood).
	// Levels that already have their own sun keep it; the Level Check offers to switch.
	if (!LevelHas<AOnyxLightingEnvironment>(World) && !LevelHas<ADirectionalLight>(World))
	{
		Spawn(World, AOnyxLightingEnvironment::StaticClass(), FVector(0.f, 600.f, 1500.f));
		Added.Add(TEXT("Onyx Lighting (Day mood)"));
	}
	// Music and ambience (empty until you pick tracks in its Details).
	if (!LevelHas<AOnyxLevelAudio>(World))
	{
		Spawn(World, AOnyxLevelAudio::StaticClass(), FVector(150.f, 600.f, 1500.f));
		Added.Add(TEXT("Level Audio (pick music in its Details)"));
	}
	World->MarkPackageDirty();
	return Added;
}
