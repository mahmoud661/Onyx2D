// Copyright Onyx2D. All Rights Reserved.

#include "Level/OnyxBrushLibrary.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Ticker.h"
#include "Level/OnyxActorPreview.h"
#include "Misc/PackageName.h"
#include "Camera/OnyxCameraBounds.h"
#include "AI/OnyxPatrolComponent.h"
#include "Character/OnyxBehaviourLibrary.h"
#include "Character/OnyxCharacter.h"
#include "World/OnyxLightSwitch.h"
#include "World/OnyxSign.h"
#include "Logic/OnyxTriggerZone.h"
#include "Combat/OnyxCombatTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Lighting/OnyxLight2D.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookActor.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "PaperSpriteActor.h"
#include "PaperSpriteComponent.h"
#include "Style/OnyxEditorStyle.h"
#include "Styling/SlateIconFinder.h"
#include "World/OnyxCheckpoint.h"
#include "World/OnyxBreakable.h"
#include "World/OnyxHazardVolume.h"
#include "World/OnyxLevelExit.h"
#include "World/OnyxMovingPlatform.h"
#include "World/OnyxOneWayPlatform.h"
#include "World/OnyxPhysicsProp.h"
#include "World/OnyxPickup.h"
#include "World/OnyxWater.h"

const FName FOnyxBrushLibrary::EntitiesLayer(TEXT("Entities"));
const FName FOnyxBrushLibrary::DecorLayer(TEXT("Decor"));
const FName FOnyxBrushLibrary::BackgroundLayer(TEXT("Background"));

namespace
{
	const FString BrushTagPrefix(TEXT("Onyx.Brush."));

	template <class T>
	TSharedRef<FOnyxPainterBrush> MakeBuiltin(const TCHAR* Id, const FText& Label, const FLinearColor& Color, bool bStandOnFloor = false)
	{
		TSharedRef<FOnyxPainterBrush> Brush = MakeShared<FOnyxPainterBrush>();
		Brush->bStandOnFloor = bStandOnFloor;
		Brush->Id = Id;
		Brush->Label = Label;
		Brush->Class = T::StaticClass();
		Brush->Color = Color;
		Brush->DefaultLayer = FOnyxBrushLibrary::EntitiesLayer;
		Brush->Matches = [](const AActor* Actor) { return Actor->IsA<T>(); };
		return Brush;
	}

	/** A plain character with the Patrol, Contact Damage and Stompable behaviours (a grey stand-in until it gets art). */
	TSharedRef<FOnyxPainterBrush> MakePatrollingFoe()
	{
		TSharedRef<FOnyxPainterBrush> Brush = MakeBuiltin<AOnyxCharacter>(TEXT("Foe"), INVTEXT("Patrolling Foe"), FLinearColor(1.f, 0.33f, 0.41f), true);
		Brush->Configure = [](AActor* Actor)
		{
			AOnyxCharacter* Foe = CastChecked<AOnyxCharacter>(Actor);
			Foe->TeamId = OnyxTeams::Enemy;
			UOnyxBehaviourLibrary::AddBehaviours(Foe, int32(UOnyxBehaviourLibrary::PatrolFoePreset));
		};
		Brush->Matches = [](const AActor* Actor) { return Actor->IsA<AOnyxCharacter>() && Actor->FindComponentByClass<UOnyxPatrolComponent>(); };
		return Brush;
	}

	TSharedRef<FOnyxPainterBrush> MakePickup(const TCHAR* Id, const FText& Label, EOnyxPickupType Type, const FLinearColor& Color, FName Icon = NAME_None)
	{
		TSharedRef<FOnyxPainterBrush> Brush = MakeBuiltin<AOnyxPickup>(Id, Label, Color);
		Brush->Icon = Icon;
		Brush->Configure = [Type](AActor* Actor)
		{
			AOnyxPickup* Pickup = CastChecked<AOnyxPickup>(Actor);
			Pickup->Type = Type;
			Pickup->RerunConstructionScripts();
		};
		Brush->Matches = [Type](const AActor* Actor)
		{
			const AOnyxPickup* Pickup = Cast<AOnyxPickup>(Actor);
			return Pickup && Pickup->Type == Type;
		};
		return Brush;
	}

	/**
	 * Any other actor that shows sprites or flipbooks (scenery dragged in from the Content Browser, a builder's props):
	 * recognised so the canvas draws it and it can be moved, without being offered in the Objects list.
	 */
	TSharedRef<FOnyxPainterBrush> MakeSceneryFallback()
	{
		TSharedRef<FOnyxPainterBrush> Brush = MakeShared<FOnyxPainterBrush>();
		Brush->Id = TEXT("Scenery");
		Brush->Label = INVTEXT("Sprite");
		Brush->Class = APaperSpriteActor::StaticClass();
		Brush->bStandOnFloor = true;
		Brush->bHiddenInPalette = true;
		Brush->DefaultLayer = FOnyxBrushLibrary::DecorLayer;
		Brush->Color = FLinearColor(0.6f, 0.75f, 0.6f);
		Brush->Matches = [](const AActor* Actor)
		{
			TInlineComponentArray<UMeshComponent*> Meshes(Actor);
			return Meshes.ContainsByPredicate([](const UMeshComponent* Mesh)
			{
				return Mesh->IsA<UPaperSpriteComponent>() || Mesh->IsA<UPaperFlipbookComponent>();
			});
		};
		return Brush;
	}

	/** Water you drag out over cells: its surface is the top of the area. */
	TSharedRef<FOnyxPainterBrush> MakeWater()
	{
		TSharedRef<FOnyxPainterBrush> Brush = MakeBuiltin<AOnyxWater>(TEXT("Water"), INVTEXT("Water"), FLinearColor(0.25f, 0.6f, 1.f));
		Brush->GetArea = [](const AActor* Actor)
		{
			const AOnyxWater* Water = CastChecked<AOnyxWater>(Actor);
			const FVector Surface = Water->GetActorLocation();
			return FBox(FVector(Surface.X - Water->Size.X * 0.5f, Surface.Y, Surface.Z - Water->Size.Y), FVector(Surface.X + Water->Size.X * 0.5f, Surface.Y, Surface.Z));
		};
		Brush->SetArea = [](AActor* Actor, const FBox& Area)
		{
			AOnyxWater* Water = CastChecked<AOnyxWater>(Actor);
			Water->Modify();
			Water->Size = FVector2D(Area.Max.X - Area.Min.X, Area.Max.Z - Area.Min.Z);
			Water->SetActorLocation(FVector(Area.GetCenter().X, Water->GetActorLocation().Y, Area.Max.Z));
			Water->RerunConstructionScripts();
		};
		Brush->GetAreaLook = [](const AActor* Actor)
		{
			const AOnyxWater* Water = CastChecked<AOnyxWater>(Actor);
			FOnyxAreaLook Look;
			Look.Top = Water->ShallowColor;
			Look.Bottom = Water->DeepColor;
			Look.Edge = Water->SurfaceColor;
			Look.bWavyEdge = Water->bWaves;
			return Look;
		};
		return Brush;
	}

	/** A trigger zone you drag out over cells: an invisible box that runs its actions. */
	TSharedRef<FOnyxPainterBrush> MakeTriggerZone()
	{
		TSharedRef<FOnyxPainterBrush> Brush = MakeBuiltin<AOnyxTriggerZone>(TEXT("TriggerZone"), INVTEXT("Trigger Zone"), FLinearColor(0.35f, 0.9f, 0.55f));
		Brush->DefaultAreaCells = FIntPoint(3, 3);
		Brush->GetArea = [](const AActor* Actor)
		{
			const AOnyxTriggerZone* Zone = CastChecked<AOnyxTriggerZone>(Actor);
			const FVector Center = Zone->GetActorLocation();
			const FVector Half(Zone->Size.X * 0.5f, 0.f, Zone->Size.Y * 0.5f);
			return FBox(Center - Half, Center + Half);
		};
		Brush->SetArea = [](AActor* Actor, const FBox& Area)
		{
			AOnyxTriggerZone* Zone = CastChecked<AOnyxTriggerZone>(Actor);
			Zone->Modify();
			Zone->Size = FVector2D(Area.Max.X - Area.Min.X, Area.Max.Z - Area.Min.Z);
			const FVector Center = Area.GetCenter();
			Zone->SetActorLocation(FVector(Center.X, Zone->GetActorLocation().Y, Center.Z));
			Zone->RerunConstructionScripts();
		};
		Brush->GetAreaLook = [](const AActor* Actor)
		{
			const AOnyxTriggerZone* Zone = CastChecked<AOnyxTriggerZone>(Actor);
			FOnyxAreaLook Look;
			Look.Top = Zone->EditorColor.CopyWithNewOpacity(0.16f);
			Look.Bottom = Zone->EditorColor.CopyWithNewOpacity(0.26f);
			Look.Edge = Zone->EditorColor;
			return Look;
		};
		return Brush;
	}

	const UPaperSprite* PreviewSpriteOf(const UObject* Asset)
	{
		if (const UPaperSprite* Sprite = Cast<UPaperSprite>(Asset))
		{
			return Sprite;
		}
		const UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(Asset);
		return Flipbook && Flipbook->GetNumKeyFrames() > 0 ? Flipbook->GetSpriteAtFrame(0) : nullptr;
	}
}

FOnyxBrushLibrary& FOnyxBrushLibrary::Get()
{
	static FOnyxBrushLibrary Library;
	return Library;
}

FOnyxBrushLibrary::FOnyxBrushLibrary()
{
	Rebuild();
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	Registry.OnAssetAdded().AddRaw(this, &FOnyxBrushLibrary::OnAssetsChanged);
	Registry.OnAssetRemoved().AddRaw(this, &FOnyxBrushLibrary::OnAssetsChanged);
	Registry.OnAssetRenamed().AddLambda([this](const FAssetData& Asset, const FString&) { OnAssetsChanged(Asset); });
}

FOnyxBrushLibrary::~FOnyxBrushLibrary()
{
	if (FAssetRegistryModule* Module = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
	{
		IAssetRegistry& Registry = Module->Get();
		Registry.OnAssetAdded().RemoveAll(this);
		Registry.OnAssetRemoved().RemoveAll(this);
		Registry.OnAssetRenamed().RemoveAll(this);
	}
}

void FOnyxBrushLibrary::OnAssetsChanged(const FAssetData& Asset)
{
	if (bRebuildQueued || !Asset.AssetClassPath.GetAssetName().ToString().Contains(TEXT("Blueprint")))
	{
		return;
	}
	bRebuildQueued = true;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float)
	{
		bRebuildQueued = false;
		Rebuild();
		return false;
	}));
}

void FOnyxBrushLibrary::AddProjectCharacters()
{
	// Every character Blueprint in the project is a brush: paint your hero and your foes straight away.
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(TEXT("/Game"));
	Filter.bRecursivePaths = true;
	TArray<FAssetData> Blueprints;
	Registry.GetAssets(Filter, Blueprints);
	const TArray<FSoftObjectPath>& Custom = GetDefault<UOnyxPainterSettings>()->CustomBrushes;
	for (const FAssetData& Asset : Blueprints)
	{
		FString ParentPath;
		Asset.GetTagValue(FBlueprintTags::NativeParentClassPath, ParentPath);
		const UClass* Parent = FSoftClassPath(FPackageName::ExportTextPathToObjectPath(ParentPath)).ResolveClass();
		if (!Parent || !Parent->IsChildOf(AOnyxCharacter::StaticClass()) || Custom.Contains(Asset.GetSoftObjectPath()))
		{
			continue;
		}
		if (TSharedPtr<FOnyxPainterBrush> Brush = MakeCustomBrush(Asset.GetSoftObjectPath()))
		{
			FString Label = Asset.AssetName.ToString();
			Label.RemoveFromStart(TEXT("BP_"));
			Brush->Label = FText::FromString(Label);
			Brush->bProject = true;
			Brush->Color = FLinearColor(0.5f, 0.47f, 0.87f);
			UClass* Class = Brush->Class;
			Brush->Matches = [Class](const AActor* Actor) { return Actor->GetClass() == Class; };
			Brushes.Add(Brush.ToSharedRef());
		}
	}
}

void FOnyxBrushLibrary::Rebuild()
{
	// The project's characters first: they match their placed actors before the generic built-ins do.
	Brushes.Reset();
	AddProjectCharacters();
	Brushes.Append(TArray<TSharedRef<FOnyxPainterBrush>>{
		MakeBuiltin<APlayerStart>(TEXT("PlayerStart"), INVTEXT("Player Start"), FLinearColor(0.22f, 0.71f, 1.f), true),
		MakePatrollingFoe(),
		MakePickup(TEXT("Coin"), INVTEXT("Coin"), EOnyxPickupType::Coin, FLinearColor(1.f, 0.76f, 0.2f)),
		MakePickup(TEXT("Health"), INVTEXT("Health"), EOnyxPickupType::Health, FLinearColor(1.f, 0.45f, 0.55f), TEXT("Onyx2D.Entity.Health")),
		MakePickup(TEXT("Key"), INVTEXT("Key"), EOnyxPickupType::Item, FLinearColor(0.3f, 0.85f, 1.f), TEXT("Onyx2D.Entity.Key")),
		MakeBuiltin<AOnyxCheckpoint>(TEXT("Checkpoint"), INVTEXT("Checkpoint"), FLinearColor(0.36f, 0.82f, 0.55f), true),
		MakeBuiltin<AOnyxLevelExit>(TEXT("Exit"), INVTEXT("Level Exit"), FLinearColor(0.65f, 0.48f, 1.f), true),
		MakeBuiltin<AOnyxMovingPlatform>(TEXT("MovingPlatform"), INVTEXT("Moving Platform"), FLinearColor(0.75f, 0.78f, 0.85f)),
		MakeBuiltin<AOnyxOneWayPlatform>(TEXT("OneWay"), INVTEXT("One-Way Platform"), FLinearColor(0.5f, 0.75f, 0.95f)),
		MakeBuiltin<AOnyxHazardVolume>(TEXT("Hazard"), INVTEXT("Hazard"), FLinearColor(1.f, 0.4f, 0.2f)),
		MakeWater(),
		MakeBuiltin<AOnyxPhysicsProp>(TEXT("PhysicsProp"), INVTEXT("Physics Prop"), FLinearColor(0.8f, 0.58f, 0.35f), true),
		MakeBuiltin<AOnyxBreakable>(TEXT("Breakable"), INVTEXT("Breakable"), FLinearColor(0.86f, 0.7f, 0.36f), true),
		MakeBuiltin<AOnyxLight2D>(TEXT("Light"), INVTEXT("2D Light"), FLinearColor(1.f, 0.9f, 0.45f)),
		MakeBuiltin<AOnyxLightSwitch>(TEXT("LightSwitch"), INVTEXT("Light Switch"), FLinearColor(1.f, 0.82f, 0.3f), true),
		MakeTriggerZone(),
		MakeBuiltin<AOnyxSign>(TEXT("Sign"), INVTEXT("Sign"), FLinearColor(0.85f, 0.63f, 0.4f), true),
	});
	for (const FSoftObjectPath& Path : GetDefault<UOnyxPainterSettings>()->CustomBrushes)
	{
		if (TSharedPtr<FOnyxPainterBrush> Brush = MakeCustomBrush(Path))
		{
			Brushes.Add(Brush.ToSharedRef());
		}
	}
	Brushes.Add(MakeSceneryFallback());
	OnChanged.Broadcast();
}

TSharedPtr<FOnyxPainterBrush> FOnyxBrushLibrary::MakeCustomBrush(const FSoftObjectPath& Path) const
{
	UObject* Asset = Path.TryLoad();
	if (!Asset)
	{
		return nullptr;
	}
	TSharedRef<FOnyxPainterBrush> Brush = MakeShared<FOnyxPainterBrush>();
	Brush->Id = *Path.ToString();
	Brush->Label = FText::FromString(Asset->GetName());
	Brush->Asset = Path;
	Brush->bCustom = true;
	Brush->bStandOnFloor = true;
	Brush->Color = FLinearColor(0.85f, 0.85f, 0.9f);

	if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		if (!Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
		{
			return nullptr;
		}
		Brush->Class = Blueprint->GeneratedClass;
		Brush->DefaultLayer = EntitiesLayer;
		Brush->Color = FLinearColor(0.38f, 0.62f, 1.f);
	}
	else if (UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(Asset))
	{
		Brush->Class = APaperFlipbookActor::StaticClass();
		Brush->DefaultLayer = DecorLayer;
		Brush->Configure = [Flipbook](AActor* Actor)
		{
			UPaperFlipbookComponent* Component = CastChecked<APaperFlipbookActor>(Actor)->GetRenderComponent();
			Component->SetMobility(EComponentMobility::Movable); // static components ignore SetFlipbook after registering
			Component->SetFlipbook(Flipbook);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			UOnyx2DLibrary::ConfigureSpriteLighting(Component, false);
		};
	}
	else if (UPaperSprite* Sprite = Cast<UPaperSprite>(Asset))
	{
		Brush->Class = APaperSpriteActor::StaticClass();
		Brush->DefaultLayer = DecorLayer;
		Brush->Configure = [Sprite](AActor* Actor)
		{
			UPaperSpriteComponent* Component = CastChecked<APaperSpriteActor>(Actor)->GetRenderComponent();
			Component->SetMobility(EComponentMobility::Movable); // static components ignore SetSprite after registering
			Component->SetSprite(Sprite);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			UOnyx2DLibrary::ConfigureSpriteLighting(Component, false);
		};
	}
	else if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
	{
		Brush->Class = AStaticMeshActor::StaticClass();
		Brush->DefaultLayer = DecorLayer;
		Brush->Configure = [Mesh](AActor* Actor)
		{
			UStaticMeshComponent* Component = CastChecked<AStaticMeshActor>(Actor)->GetStaticMeshComponent();
			Component->SetMobility(EComponentMobility::Movable);
			Component->SetStaticMesh(Mesh);
		};
	}
	else
	{
		return nullptr;
	}
	return Brush;
}

bool FOnyxBrushLibrary::CanBeBrush(const FAssetData& Asset)
{
	const UClass* Class = Asset.GetClass();
	if (!Class)
	{
		return false;
	}
	if (Class->IsChildOf(UBlueprint::StaticClass()))
	{
		FString ParentPath;
		Asset.GetTagValue(FBlueprintTags::NativeParentClassPath, ParentPath);
		const UClass* Parent = FSoftClassPath(FPackageName::ExportTextPathToObjectPath(ParentPath)).ResolveClass();
		return Parent && Parent->IsChildOf(AActor::StaticClass());
	}
	return Class->IsChildOf(UPaperFlipbook::StaticClass()) || Class->IsChildOf(UPaperSprite::StaticClass()) || Class->IsChildOf(UStaticMesh::StaticClass());
}

int32 FOnyxBrushLibrary::AddAssets(const TArray<FAssetData>& Assets)
{
	UOnyxPainterSettings* Settings = GetMutableDefault<UOnyxPainterSettings>();
	int32 Added = 0;
	for (const FAssetData& Asset : Assets)
	{
		const FSoftObjectPath Path = Asset.GetSoftObjectPath();
		if (CanBeBrush(Asset) && !Settings->CustomBrushes.Contains(Path))
		{
			Settings->CustomBrushes.Add(Path);
			++Added;
		}
	}
	if (Added > 0)
	{
		Settings->SaveConfig();
		Rebuild();
	}
	return Added;
}

void FOnyxBrushLibrary::Remove(FName Id)
{
	UOnyxPainterSettings* Settings = GetMutableDefault<UOnyxPainterSettings>();
	if (Settings->CustomBrushes.RemoveAll([Id](const FSoftObjectPath& Path) { return FName(*Path.ToString()) == Id; }) > 0)
	{
		Settings->SaveConfig();
		Rebuild();
	}
}

TSharedPtr<FOnyxPainterBrush> FOnyxBrushLibrary::FindById(FName Id) const
{
	const TSharedRef<FOnyxPainterBrush>* Found = Brushes.FindByPredicate([Id](const TSharedRef<FOnyxPainterBrush>& Brush) { return Brush->Id == Id; });
	return Found ? TSharedPtr<FOnyxPainterBrush>(*Found) : nullptr;
}

TSharedPtr<FOnyxPainterBrush> FOnyxBrushLibrary::FindFor(const AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}
	for (const FName& Tag : Actor->Tags)
	{
		const FString TagString = Tag.ToString();
		if (TagString.StartsWith(BrushTagPrefix))
		{
			// A brush that is gone (a removed custom brush, one made by a builder) falls back to matching below.
			if (TSharedPtr<FOnyxPainterBrush> Tagged = FindById(*TagString.RightChop(BrushTagPrefix.Len())))
			{
				return Tagged;
			}
			break;
		}
	}
	const TSharedRef<FOnyxPainterBrush>* Found = Brushes.FindByPredicate([Actor](const TSharedRef<FOnyxPainterBrush>& Brush) { return Brush->Matches && Brush->Matches(Actor); });
	return Found ? TSharedPtr<FOnyxPainterBrush>(*Found) : nullptr;
}

AActor* FOnyxBrushLibrary::Spawn(const FOnyxPainterBrush& Brush, UWorld* World, const FVector& Location) const
{
	if (!World || !Brush.Class)
	{
		return nullptr;
	}
	AActor* Actor = GEditor->AddActor(World->GetCurrentLevel(), Brush.Class, FTransform(Location), true, RF_Transactional, false);
	if (Actor)
	{
		if (Brush.Configure)
		{
			Brush.Configure(Actor);
		}
		Actor->Tags.AddUnique(*(BrushTagPrefix + Brush.Id.ToString()));
		if (Brush.bCustom)
		{
			Actor->SetActorLabel(Brush.Label.ToString());
		}
	}
	return Actor;
}

const FSlateBrush* FOnyxBrushLibrary::GetIcon(const FOnyxPainterBrush& Brush) const
{
	if (!Brush.Icon.IsNone())
	{
		return FOnyxEditorStyle::Get().GetBrush(*(Brush.Icon.ToString() + TEXT(".Small")));
	}
	return FSlateIconFinder::FindIconBrushForClass(Brush.Class, "ClassIcon.Actor");
}

bool FOnyxBrushLibrary::GetPreviewSprite(const FOnyxPainterBrush& Brush, UTexture2D*& OutTexture, FIntRect& OutRect, double Time) const
{
	const UObject* Asset = Brush.bCustom ? Brush.Asset.ResolveObject() : nullptr;
	const UPaperSprite* Sprite = Cast<UPaperSprite>(Asset);
	if (const UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(Asset); Flipbook && Flipbook->GetNumKeyFrames() > 0)
	{
		const float Duration = Flipbook->GetTotalDuration();
		Sprite = Flipbook->GetSpriteAtTime(Duration > 0.f ? float(FMath::Fmod(Time, double(Duration))) : 0.f);
	}
	FOnyxSpriteDraw Draw;
	const bool bHasDraw = Sprite ? OnyxActorPreview::MakeDraw(Sprite, FTransform::Identity, Draw) : OnyxActorPreview::GetClassSprite(Brush.Class, Time, Draw);
	if (!bHasDraw || !Draw.Texture)
	{
		OutTexture = nullptr;
		return false;
	}
	OutTexture = Draw.Texture;
	const FVector2f TextureSize(Draw.Texture->GetSurfaceWidth(), Draw.Texture->GetSurfaceHeight());
	const FIntPoint Min(FMath::RoundToInt(Draw.UV.Min.X * TextureSize.X), FMath::RoundToInt(Draw.UV.Min.Y * TextureSize.Y));
	OutRect = FIntRect(Min, Min + Draw.Pixels);
	return Draw.Pixels.X > 0 && Draw.Pixels.Y > 0;
}
