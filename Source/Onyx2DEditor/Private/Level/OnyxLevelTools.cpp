// Copyright Onyx2D. All Rights Reserved.

#include "Level/OnyxLevelTools.h"
#include "Core/OnyxLevelData.h"
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
#include "GameMapsSettings.h"
#include "LevelEditorViewport.h"
#include "Lighting/OnyxLight2D.h"
#include "ScopedTransaction.h"
#include "World/OnyxLevelBuilder.h"
#include "ActorEditorUtils.h"
#include "Algo/Find.h"
#include "GameFramework/Volume.h"
#include "Camera/OnyxCameraBounds.h"
#include "Engine/Light.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Pawn.h"
#include "Lighting/OnyxLightFlash.h"
#include "PaperFlipbookActor.h"
#include "PaperSpriteActor.h"
#include "PaperTileMapComponent.h"
#include "World/OnyxBreakable.h"
#include "World/OnyxLightSwitch.h"
#include "World/OnyxParallaxBackground.h"
#include "World/OnyxPhysicsProp.h"
#include "World/OnyxPickup.h"
#include "World/OnyxRain.h"
#include "World/OnyxWater.h"

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

DEFINE_LOG_CATEGORY_STATIC(LogOnyxLevelTools, Log, All);

// World Outliner folders (named: unity builds put several files' helpers together).
namespace OnyxOutlinerFiling
{

	const TCHAR* LevelFolder = TEXT("Level");
	const TCHAR* const KindFolders[] = { TEXT("Characters"), TEXT("Lights"), TEXT("Props"), TEXT("Pickups"), TEXT("Water"), TEXT("Gameplay"), TEXT("Backdrop"), TEXT("Setup") };

	/** For the whole level wherever it stands: never filed under a map. */
	bool IsLevelWide(const AActor* Actor)
	{
		return Actor->IsA<AOnyxParallaxBackground>() || Actor->IsA<AOnyxLightingEnvironment>() || Actor->IsA<AOnyxRain>()
			|| Actor->IsA<AOnyxLevelAudio>() || Actor->IsA<AOnyxCameraBounds>() || Actor->IsA<AOnyxLevelBuilder>()
			|| (Actor->IsA<ALight>() && !Actor->IsA<AOnyxLight2D>()) || Actor->IsA<AVolume>();
	}

	const TCHAR* KindFolder(const AActor* Actor)
	{
		if (Actor->IsA<APawn>() || Actor->IsA<APlayerStart>())
		{
			return TEXT("Characters");
		}
		if (Actor->IsA<AOnyxPickup>())
		{
			return TEXT("Pickups");
		}
		if (Actor->IsA<AOnyxLight2D>() || Actor->IsA<AOnyxLightFlash>() || Actor->IsA<AOnyxLightSwitch>())
		{
			return TEXT("Lights");
		}
		if (Actor->IsA<AOnyxWater>())
		{
			return TEXT("Water");
		}
		if (Actor->IsA<AOnyxParallaxBackground>())
		{
			return TEXT("Backdrop");
		}
		if (IsLevelWide(Actor))
		{
			return TEXT("Setup");
		}
		if (Actor->IsA<APaperSpriteActor>() || Actor->IsA<APaperFlipbookActor>() || Actor->IsA<AOnyxBreakable>() || Actor->IsA<AOnyxPhysicsProp>()
			|| Actor->IsA<AStaticMeshActor>())
		{
			return TEXT("Props");
		}
		return TEXT("Gameplay");
	}

	/** A folder name from an actor label ("/" would make it a path). */
	FString FolderName(const AActor* Actor)
	{
		FString Name = Actor->GetActorLabel();
		Name.ReplaceInline(TEXT("/"), TEXT("-"));
		return Name.IsEmpty() ? FString(TEXT("Map")) : Name;
	}

	struct FOutlinerMap
	{
		const APaperTileMapActor* Actor = nullptr;
		FString Folder;
		FBox Area;
	};

	TArray<FOutlinerMap> FindOutlinerMaps(UWorld* World)
	{
		TArray<FOutlinerMap> Maps;
		for (TActorIterator<APaperTileMapActor> It(World); It; ++It)
		{
			const UPaperTileMapComponent* Map = It->GetRenderComponent();
			if (!Map || !Map->TileMap)
			{
				continue;
			}
			// What stands on its top row or just past its ends is still in it.
			FBox Area = Map->Bounds.GetBox();
			Area = Area.ExpandBy(FVector(150.0, 0.0, 300.0));
			Maps.Add({ *It, FolderName(*It), Area });
		}
		return Maps;
	}

	/** Ours to move: not in a folder yet, or in one this filing made (a map's, or Level's, by kind). */
	bool IsFiledByUs(const AActor* Actor, const TArray<FOutlinerMap>& Maps)
	{
		const FString Path = Actor->GetFolderPath().ToString();
		if (Actor->GetFolderPath().IsNone() || Path.IsEmpty())
		{
			return true;
		}
		FString TopFolder = Path;
		FString Rest;
		Path.Split(TEXT("/"), &TopFolder, &Rest);
		const bool bOurRoot = TopFolder == LevelFolder || Maps.ContainsByPredicate([&TopFolder](const FOutlinerMap& Map) { return Map.Folder == TopFolder; });
		const bool bOurLeaf = Rest.IsEmpty() || Algo::FindByPredicate(KindFolders, [&Rest](const TCHAR* Kind) { return Rest == Kind; }) != nullptr;
		return bOurRoot && bOurLeaf;
	}

	bool ShouldFile(const AActor* Actor)
	{
		return Actor && IsValid(Actor) && !Actor->HasAnyFlags(RF_Transient) && Actor->IsEditable() && Actor->IsListedInSceneOutliner()
			&& !Actor->IsA<AWorldSettings>() && !FActorEditorUtils::IsABuilderBrush(Actor) && !Actor->IsChildActor() && !Actor->GetAttachParentActor();
	}

	FName FolderFor(const AActor* Actor, const TArray<FOutlinerMap>& Maps)
	{
		if (const APaperTileMapActor* MapActor = Cast<APaperTileMapActor>(Actor))
		{
			const FOutlinerMap* Own = Maps.FindByPredicate([MapActor](const FOutlinerMap& Map) { return Map.Actor == MapActor; });
			return Own ? FName(*Own->Folder) : FName(*FString::Printf(TEXT("%s/Maps"), LevelFolder));
		}
		const TCHAR* Kind = KindFolder(Actor);
		if (!IsLevelWide(Actor))
		{
			// The smallest map it stands in (a room inside a bigger map is the better home).
			const FVector Location = Actor->GetActorLocation();
			const FOutlinerMap* Home = nullptr;
			for (const FOutlinerMap& Map : Maps)
			{
				const bool bInside = Location.X >= Map.Area.Min.X && Location.X <= Map.Area.Max.X && Location.Z >= Map.Area.Min.Z && Location.Z <= Map.Area.Max.Z;
				const double Size = Map.Area.GetSize().X * Map.Area.GetSize().Z;
				if (bInside && (!Home || Size < Home->Area.GetSize().X * Home->Area.GetSize().Z))
				{
					Home = &Map;
				}
			}
			if (Home)
			{
				return FName(*FString::Printf(TEXT("%s/%s"), *Home->Folder, Kind));
			}
		}
		return FName(*FString::Printf(TEXT("%s/%s"), LevelFolder, Kind));
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

bool FOnyxLevelTools::SetGameStyle(UWorld* World, EOnyxGameStyle Style)
{
	AWorldSettings* Settings = World ? World->GetWorldSettings() : nullptr;
	if (!Settings)
	{
		return false;
	}
	UOnyxLevelData* Level = Settings->GetAssetUserData<UOnyxLevelData>();
	if (Level && Level->GameStyle == Style)
	{
		return true;
	}
	const FScopedTransaction Transaction(INVTEXT("Set Game Style"));
	Settings->Modify();
	if (!Level)
	{
		Level = NewObject<UOnyxLevelData>(Settings, NAME_None, RF_Transactional);
		Settings->AddAssetUserData(Level);
	}
	Level->Modify();
	Level->GameStyle = Style;
	Settings->MarkPackageDirty();
	UE_LOG(LogOnyxLevelTools, Display, TEXT("%s is played %s"), *World->GetMapName(), Style == EOnyxGameStyle::TopDown ? TEXT("top down") : TEXT("side view"));
	return true;
}

TArray<FString> FOnyxLevelTools::SetupCurrentLevel()
{
	TArray<FString> Added;
	UWorld* World = FOnyxLevelTools::GetEditorWorld();
	if (!World)
	{
		return Added;
	}
	FScopedTransaction Transaction(INVTEXT("Set Up Onyx2D Level"));

	if (AWorldSettings* Settings = World->GetWorldSettings())
	{
		// Only when the level would not play with an Onyx game mode: a game's own (a child of Onyx Game Mode, with its hero
		// and HUD), on the level or as the project's default, is kept. Replacing it with the bare Onyx Game Mode would spawn
		// the placeholder player instead of the game's hero.
		const UClass* LevelMode = Settings->DefaultGameMode.Get();
		const UClass* ProjectMode = FSoftClassPath(UGameMapsSettings::GetGlobalDefaultGameMode()).TryLoadClass<AGameModeBase>();
		const bool bPlaysOnyx = LevelMode ? LevelMode->IsChildOf(AOnyxGameMode::StaticClass()) : ProjectMode && ProjectMode->IsChildOf(AOnyxGameMode::StaticClass());
		if (!bPlaysOnyx)
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
	// One actor owns the level's look: global light, the 2D sun, the mood's film settings and the 2D light composite.
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
	if (Added.IsEmpty())
	{
		Transaction.Cancel(); // nothing to undo, and the level isn't marked as changed
		return Added;
	}
	World->MarkPackageDirty();
	return Added;
}

int32 FOnyxLevelTools::OrganizeOutliner(UWorld* World)
{
	using namespace OnyxOutlinerFiling;
	if (!World)
	{
		return 0;
	}
	const TArray<FOutlinerMap> Maps = FindOutlinerMaps(World);
	int32 Moved = 0;
	TMap<FName, int32> Counts;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!ShouldFile(Actor) || !IsFiledByUs(Actor, Maps))
		{
			continue;
		}
		const FName Folder = FolderFor(Actor, Maps);
		++Counts.FindOrAdd(Folder);
		if (Actor->GetFolderPath() != Folder)
		{
			Actor->SetFolderPath_Recursively(Folder);
			++Moved;
		}
	}
	Counts.KeySort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	TArray<FString> Summary;
	for (const TPair<FName, int32>& Count : Counts)
	{
		Summary.Add(FString::Printf(TEXT("%s %d"), *Count.Key.ToString(), Count.Value));
	}
	UE_LOG(LogOnyxLevelTools, Display, TEXT("Onyx2D: World Outliner filed by map and kind (%d moved): %s"), Moved, *FString::Join(Summary, TEXT(", ")));
	return Moved;
}

void FOnyxLevelTools::FileInOutliner(AActor* Actor)
{
	using namespace OnyxOutlinerFiling;
	if (!ShouldFile(Actor) || !Actor->GetWorld())
	{
		return;
	}
	const TArray<FOutlinerMap> Maps = FindOutlinerMaps(Actor->GetWorld());
	if (IsFiledByUs(Actor, Maps))
	{
		Actor->SetFolderPath_Recursively(FolderFor(Actor, Maps));
	}
}
