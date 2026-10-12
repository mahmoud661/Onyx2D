// Copyright Onyx2D. All Rights Reserved.

#include "Level/OnyxBrushLibrary.h"
#include "AssetRegistry/AssetData.h"
#include "Camera/OnyxCameraBounds.h"
#include "AI/OnyxPatrolComponent.h"
#include "Character/OnyxBehaviourLibrary.h"
#include "Character/OnyxCharacter.h"
#include "World/OnyxLightSwitch.h"
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
#include "World/OnyxHazardVolume.h"
#include "World/OnyxLevelExit.h"
#include "World/OnyxMovingPlatform.h"
#include "World/OnyxOneWayPlatform.h"
#include "World/OnyxPickup.h"

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
}

void FOnyxBrushLibrary::Rebuild()
{
	Brushes = {
		MakeBuiltin<APlayerStart>(TEXT("PlayerStart"), INVTEXT("Player Start"), FLinearColor(0.22f, 0.71f, 1.f), true),
		MakePatrollingFoe(),
		MakePickup(TEXT("Coin"), INVTEXT("Coin"), EOnyxPickupType::Coin, FLinearColor(1.f, 0.76f, 0.2f)),
		MakePickup(TEXT("Health"), INVTEXT("Health"), EOnyxPickupType::Health, FLinearColor(1.f, 0.45f, 0.55f), TEXT("Onyx2D.Entity.Health")),
		MakeBuiltin<AOnyxCheckpoint>(TEXT("Checkpoint"), INVTEXT("Checkpoint"), FLinearColor(0.36f, 0.82f, 0.55f)),
		MakeBuiltin<AOnyxLevelExit>(TEXT("Exit"), INVTEXT("Level Exit"), FLinearColor(0.65f, 0.48f, 1.f)),
		MakeBuiltin<AOnyxMovingPlatform>(TEXT("MovingPlatform"), INVTEXT("Moving Platform"), FLinearColor(0.75f, 0.78f, 0.85f)),
		MakeBuiltin<AOnyxOneWayPlatform>(TEXT("OneWay"), INVTEXT("One-Way Platform"), FLinearColor(0.5f, 0.75f, 0.95f)),
		MakeBuiltin<AOnyxHazardVolume>(TEXT("Hazard"), INVTEXT("Hazard"), FLinearColor(1.f, 0.4f, 0.2f)),
		MakeBuiltin<AOnyxLight2D>(TEXT("Light"), INVTEXT("2D Light"), FLinearColor(1.f, 0.9f, 0.45f)),
		MakeBuiltin<AOnyxLightSwitch>(TEXT("LightSwitch"), INVTEXT("Light Switch"), FLinearColor(1.f, 0.82f, 0.3f), true),
	};
	for (const FSoftObjectPath& Path : GetDefault<UOnyxPainterSettings>()->CustomBrushes)
	{
		if (TSharedPtr<FOnyxPainterBrush> Brush = MakeCustomBrush(Path))
		{
			Brushes.Add(Brush.ToSharedRef());
		}
	}
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
			return FindById(*TagString.RightChop(BrushTagPrefix.Len()));
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

bool FOnyxBrushLibrary::GetPreviewSprite(const FOnyxPainterBrush& Brush, UTexture2D*& OutTexture, FIntRect& OutRect) const
{
	const UPaperSprite* Sprite = Brush.bCustom ? PreviewSpriteOf(Brush.Asset.ResolveObject()) : nullptr;
	OutTexture = Sprite ? Sprite->GetSourceTexture() : nullptr;
	if (!OutTexture)
	{
		return false;
	}
	const FIntPoint Min(FMath::RoundToInt(Sprite->GetSourceUV().X), FMath::RoundToInt(Sprite->GetSourceUV().Y));
	const FIntPoint Size(FMath::RoundToInt(Sprite->GetSourceSize().X), FMath::RoundToInt(Sprite->GetSourceSize().Y));
	OutRect = FIntRect(Min, Min + Size);
	return Size.X > 0 && Size.Y > 0;
}
