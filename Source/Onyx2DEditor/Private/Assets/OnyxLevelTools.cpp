// Copyright Onyx2D. All Rights Reserved.

#include "Assets/OnyxLevelTools.h"
#include "Assets/OnyxTileBuilder.h"
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

namespace
{
	constexpr float PlacementGrid = 50.f;

	UWorld* EditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

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

AActor* FOnyxLevelTools::PlaceInFrontOfCamera(UClass* Class)
{
	UWorld* World = EditorWorld();
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
	UWorld* World = EditorWorld();
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
	World->MarkPackageDirty();
	return Added;
}
