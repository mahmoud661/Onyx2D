// Copyright Onyx2D. All Rights Reserved.

#include "Core/OnyxDrawOrderSubsystem.h"
#include "Character/OnyxPlayerControlComponent.h"
#include "Components/MeshComponent.h"
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
	int32& Next = NextSlot[FMath::Clamp(int32(Layer), 0, int32(UE_ARRAY_COUNT(NextSlot)) - 1)];
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
