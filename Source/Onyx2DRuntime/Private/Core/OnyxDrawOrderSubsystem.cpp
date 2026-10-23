// Copyright Onyx2D. All Rights Reserved.

#include "Core/OnyxDrawOrderSubsystem.h"
#include "Character/OnyxPlayerControlComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Core/OnyxLevelData.h"
#include "GameFramework/Character.h"
#include "Core/OnyxDrawOrderComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "PaperTileMapComponent.h"
#include "World/OnyxFlipbookEffect.h"
#include "World/OnyxPickup.h"

namespace
{
	/** How often the whole world is checked again (cheap: only changed values are written). */
	constexpr float RefreshInterval = 0.25f;

	/** The Level Painter's object-layer tags (Onyx.Layer.<Name>). */
	const FName BackgroundLayerTag(TEXT("Onyx.Layer.Background"));
	const FName DecorLayerTag(TEXT("Onyx.Layer.Decor"));

	void ApplyBias(UMeshComponent* Mesh, float Bias)
	{
		const TArray<float>& Data = Mesh->GetCustomPrimitiveData().Data;
		if (Data.IsValidIndex(Onyx2D::DrawOrderDataIndex) && FMath::IsNearlyEqual(Data[Onyx2D::DrawOrderDataIndex], Bias, 1.e-5f))
		{
			return;
		}
		Mesh->SetCustomPrimitiveDataFloat(Onyx2D::DrawOrderDataIndex, Bias);
	}
}

UOnyxDrawOrderSubsystem* UOnyxDrawOrderSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UOnyxDrawOrderSubsystem>() : nullptr;
}

void UOnyxDrawOrderSubsystem::RefreshActor(AActor* Actor)
{
	if (UOnyxDrawOrderSubsystem* Subsystem = Actor ? Get(Actor) : nullptr)
	{
		Subsystem->Refresh(Actor);
	}
}

bool UOnyxDrawOrderSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

TStatId UOnyxDrawOrderSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOnyxDrawOrderSubsystem, STATGROUP_Tickables);
}

void UOnyxDrawOrderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UWorld* World = GetWorld();
	SpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UOnyxDrawOrderSubsystem::HandleActorAdded));
#if WITH_EDITOR
	// Placed or moved in the editor (moving a Blueprint rebuilds its components, which resets their bias).
	if (GEngine && World->WorldType == EWorldType::Editor)
	{
		EditorAddedHandle = GEngine->OnLevelActorAdded().AddUObject(this, &UOnyxDrawOrderSubsystem::HandleActorAdded);
		EditorMovedHandle = GEngine->OnActorMoved().AddUObject(this, &UOnyxDrawOrderSubsystem::HandleActorAdded);
	}
#endif
}

void UOnyxDrawOrderSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(SpawnedHandle);
	}
#if WITH_EDITOR
	if (GEngine)
	{
		GEngine->OnLevelActorAdded().Remove(EditorAddedHandle);
		GEngine->OnActorMoved().Remove(EditorMovedHandle);
	}
#endif
	Super::Deinitialize();
}

void UOnyxDrawOrderSubsystem::OnWorldComponentsUpdated(UWorld& InWorld)
{
	Super::OnWorldComponentsUpdated(InWorld);
	RefreshAll();
}

void UOnyxDrawOrderSubsystem::HandleActorAdded(AActor* Actor)
{
	if (Actor && Actor->GetWorld() == GetWorld())
	{
		Refresh(Actor);
	}
}

void UOnyxDrawOrderSubsystem::Tick(float DeltaTime)
{
	if (!Movers.IsEmpty())
	{
		RefreshMovers();
	}
	SinceRefresh += DeltaTime;
	if (SinceRefresh >= RefreshInterval)
	{
		SinceRefresh = 0.f;
		RefreshAll();
	}
}

EOnyxDrawLayer UOnyxDrawOrderSubsystem::FindLayer(const AActor* Actor)
{
	if (!Actor)
	{
		return Onyx2D::DrawOrderBaseLayer;
	}
	if (const UOnyxDrawOrderComponent* Override = Actor->FindComponentByClass<UOnyxDrawOrderComponent>())
	{
		return Override->Layer;
	}
	if (Actor->Tags.Contains(BackgroundLayerTag))
	{
		return EOnyxDrawLayer::BehindTiles;
	}
	if (Actor->Tags.Contains(DecorLayerTag))
	{
		return EOnyxDrawLayer::InFront;
	}
	if (Actor->FindComponentByClass<UPaperTileMapComponent>())
	{
		return EOnyxDrawLayer::Tiles;
	}
	const APawn* Pawn = Cast<APawn>(Actor);
	if ((Pawn && Pawn->IsPlayerControlled()) || Actor->FindComponentByClass<UOnyxPlayerControlComponent>())
	{
		return EOnyxDrawLayer::Player;
	}
	if (Pawn)
	{
		return EOnyxDrawLayer::Characters;
	}
	if (Actor->IsA<AOnyxPickup>() || Actor->IsA<AOnyxFlipbookEffect>())
	{
		return EOnyxDrawLayer::Items;
	}
	return EOnyxDrawLayer::Props;
}

bool UOnyxDrawOrderSubsystem::IsSortedByFeet(EOnyxDrawLayer Layer)
{
	return Layer == EOnyxDrawLayer::Props || Layer == EOnyxDrawLayer::Items || Layer == EOnyxDrawLayer::Characters || Layer == EOnyxDrawLayer::Player;
}

float UOnyxDrawOrderSubsystem::GetTopDownBias(EOnyxDrawLayer Layer, float FeetZ, int32 Slot, int32 ComponentPriority)
{
	const int32 Priority = FMath::Clamp(ComponentPriority, -Onyx2D::DrawOrderMaxComponentPriority, Onyx2D::DrawOrderMaxComponentPriority);
	const float Inside = Priority * Onyx2D::TopDownComponentStep;
	const float Beyond = Onyx2D::TopDownSortRange + Onyx2D::DrawLayerStep;
	switch (Layer)
	{
	case EOnyxDrawLayer::BehindTiles: return -Beyond - Onyx2D::DrawLayerStep + Inside;
	case EOnyxDrawLayer::Tiles:       return -Beyond + Inside;
	case EOnyxDrawLayer::InFront:     return Beyond + Inside;
	default:
	{
		// Lower on the screen, nearer. Two at the same height keep their slots' order (all the slots within half a pixel).
		const float SlotOffset = (FMath::Clamp(Slot, 0, Onyx2D::DrawOrderSlots - 1) - Onyx2D::DrawOrderSlots / 2) * Onyx2D::TopDownSlotStep;
		return FMath::Clamp(-FeetZ * Onyx2D::TopDownSortScale, -Onyx2D::TopDownSortRange, Onyx2D::TopDownSortRange) + Inside + SlotOffset;
	}
	}
}

float UOnyxDrawOrderSubsystem::FindFeetZ(const AActor* Actor)
{
	if (const ACharacter* Character = Cast<ACharacter>(Actor))
	{
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		return Character->GetActorLocation().Z - (Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f);
	}
	float Bottom = TNumericLimits<float>::Max();
	TInlineComponentArray<UMeshComponent*> Meshes(Actor);
	for (const UMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->IsRegistered() && Mesh->IsVisible())
		{
			Bottom = FMath::Min(Bottom, float(Mesh->Bounds.Origin.Z - Mesh->Bounds.BoxExtent.Z));
		}
	}
	return Bottom < TNumericLimits<float>::Max() ? Bottom : float(Actor->GetActorLocation().Z);
}

bool UOnyxDrawOrderSubsystem::IsTopDownWorld() const
{
	return UOnyxLevelData::GetGameStyle(GetWorld()) == EOnyxGameStyle::TopDown;
}

void UOnyxDrawOrderSubsystem::RefreshMovers()
{
	for (auto It = Movers.CreateIterator(); It; ++It)
	{
		if (AActor* Actor = It->Get())
		{
			Refresh(Actor);
		}
		else
		{
			It.RemoveCurrent();
		}
	}
}

float UOnyxDrawOrderSubsystem::GetBias(EOnyxDrawLayer Layer, int32 Slot, int32 ComponentPriority)
{
	const int32 LayerOffset = int32(Layer) - int32(Onyx2D::DrawOrderBaseLayer);
	const int32 SlotOffset = FMath::Clamp(Slot, 0, Onyx2D::DrawOrderSlots - 1) - Onyx2D::DrawOrderSlots / 2;
	const int32 Priority = FMath::Clamp(ComponentPriority, -Onyx2D::DrawOrderMaxComponentPriority, Onyx2D::DrawOrderMaxComponentPriority);
	return LayerOffset * Onyx2D::DrawLayerStep + SlotOffset * Onyx2D::DrawOrderSlotStep + Priority * Onyx2D::DrawOrderComponentStep;
}

int32 UOnyxDrawOrderSubsystem::FindSlot(const AActor* Actor, EOnyxDrawLayer Layer)
{
	if (const UOnyxDrawOrderComponent* Override = Actor->FindComponentByClass<UOnyxDrawOrderComponent>())
	{
		return Override->Order + Onyx2D::DrawOrderSlots / 2;
	}
	// Handed out in turn and kept, so an actor never swaps places with another while they move.
	if (const int32* Known = Slots.Find(Actor))
	{
		return *Known;
	}
	// Top down the layers sorted by their feet share one depth scale, so they take turns together too (else the player and
	// the first prop would both get the first slot, and tie where they stand level).
	const EOnyxDrawLayer Turns = IsSortedByFeet(Layer) && IsTopDownWorld() ? EOnyxDrawLayer::Props : Layer;
	int32& Next = NextSlot[FMath::Clamp(int32(Turns), 0, int32(UE_ARRAY_COUNT(NextSlot)) - 1)];
	const int32 Slot = Next;
	Next = (Next + 1) % Onyx2D::DrawOrderSlots;
	Slots.Add(Actor, Slot);
	return Slot;
}

void UOnyxDrawOrderSubsystem::Refresh(AActor* Actor)
{
	if (!Actor || Actor->IsActorBeingDestroyed())
	{
		return;
	}
	TInlineComponentArray<UMeshComponent*> Meshes(Actor);
	if (Meshes.IsEmpty())
	{
		return;
	}
	const EOnyxDrawLayer Layer = FindLayer(Actor);
	const int32 Slot = FindSlot(Actor, Layer);
	if (IsTopDownWorld())
	{
		const bool bByFeet = IsSortedByFeet(Layer);
		const float FeetZ = bByFeet ? FindFeetZ(Actor) : 0.f;
		for (UMeshComponent* Mesh : Meshes)
		{
			if (Mesh && Mesh->IsRegistered())
			{
				ApplyBias(Mesh, GetTopDownBias(Layer, FeetZ, Slot, Mesh->TranslucencySortPriority));
			}
		}
		// Things that move are sorted again every frame as they go.
		const USceneComponent* Root = Actor->GetRootComponent();
		if (bByFeet && Root && Root->Mobility == EComponentMobility::Movable && GetWorld()->IsGameWorld())
		{
			Movers.Add(Actor);
		}
		return;
	}
	Movers.Remove(Actor);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->IsRegistered())
		{
			ApplyBias(Mesh, GetBias(Layer, Slot, Mesh->TranslucencySortPriority));
		}
	}
}

void UOnyxDrawOrderSubsystem::RefreshAll()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		Refresh(*It);
	}
	// Forget actors that are gone.
	for (auto It = Slots.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
