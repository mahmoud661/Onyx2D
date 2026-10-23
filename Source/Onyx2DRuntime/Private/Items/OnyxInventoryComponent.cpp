// Copyright Onyx2D. All Rights Reserved.

#include "Items/OnyxInventoryComponent.h"
#include "Animation/OnyxMovesetComponent.h"
#include "Character/OnyxCharacter.h"
#include "Character/OnyxPlayerControlComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/OnyxScoreSubsystem.h"
#include "Engine/World.h"
#include "Farming/OnyxFarmField.h"
#include "World/OnyxResourceNode.h"
#include "Items/OnyxHeldLook.h"
#include "Items/OnyxItem.h"
#include "Movement/OnyxMovementComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "PaperFlipbookComponent.h"
#include "PaperSpriteComponent.h"
#include "TimerManager.h"

UOnyxInventoryComponent::UOnyxInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// After the body's flipbook has moved on this frame, so a held look shows the frame the body shows.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UOnyxInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	Slots.SetNum(FMath::Max(SlotCount, 1));
	UOnyxSaveSubsystem* Save = UOnyxSaveSubsystem::Get(this);
	if (!Save || !Save->HasPending(GetSaveKey()))
	{
		// A new game: what it starts with.
		for (const FOnyxItemStack& Stack : StartingItems)
		{
			AddItem(Stack.Item, Stack.Count);
		}
		if (UOnyxScoreSubsystem* Score = StartingCoins > 0 ? UOnyxScoreSubsystem::Get(this) : nullptr)
		{
			Score->AddCoins(StartingCoins);
		}
	}
	if (UOnyxPlayerControlComponent* Control = GetOwner()->FindComponentByClass<UOnyxPlayerControlComponent>())
	{
		Control->OnInteract.AddUniqueDynamic(this, &UOnyxInventoryComponent::HandleInteract);
	}
	if (Save)
	{
		Save->Register(this);
	}
	OnInventoryChanged.Broadcast();
}

void UOnyxInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LandTimer);
	}
	PendingLand.Reset();
	if (UOnyxSaveSubsystem* Save = UOnyxSaveSubsystem::Get(this))
	{
		Save->Unregister(this);
	}
	Super::EndPlay(EndPlayReason);
}

UOnyxItem* UOnyxInventoryComponent::GetSelectedItem() const
{
	return Slots.IsValidIndex(Selected) && !Slots[Selected].IsEmpty() ? Slots[Selected].Item.Get() : nullptr;
}

int32 UOnyxInventoryComponent::AddItem(UOnyxItem* Item, int32 Count)
{
	if (!Item || Count <= 0)
	{
		return FMath::Max(Count, 0);
	}
	const int32 MaxStack = FMath::Max(Item->MaxStack, 1);
	int32 Left = Count;
	for (FOnyxItemStack& Stack : Slots)
	{
		if (Left > 0 && Stack.Item == Item && Stack.Count < MaxStack)
		{
			const int32 Moved = FMath::Min(MaxStack - Stack.Count, Left);
			Stack.Count += Moved;
			Left -= Moved;
		}
	}
	for (FOnyxItemStack& Stack : Slots)
	{
		if (Left > 0 && Stack.IsEmpty())
		{
			Stack.Item = Item;
			Stack.Count = FMath::Min(MaxStack, Left);
			Left -= Stack.Count;
		}
	}
	if (Left != Count)
	{
		OnInventoryChanged.Broadcast();
	}
	return Left;
}

int32 UOnyxInventoryComponent::RemoveItem(UOnyxItem* Item, int32 Count)
{
	int32 Taken = 0;
	for (int32 Index = Slots.Num() - 1; Index >= 0 && Taken < Count; --Index)
	{
		FOnyxItemStack& Stack = Slots[Index];
		if (Stack.Item == Item && Stack.Count > 0)
		{
			const int32 Moved = FMath::Min(Stack.Count, Count - Taken);
			Stack.Count -= Moved;
			Taken += Moved;
			if (Stack.Count <= 0)
			{
				Stack = FOnyxItemStack();
			}
		}
	}
	if (Taken > 0)
	{
		OnInventoryChanged.Broadcast();
	}
	return Taken;
}

int32 UOnyxInventoryComponent::CountOf(const UOnyxItem* Item) const
{
	int32 Count = 0;
	for (const FOnyxItemStack& Stack : Slots)
	{
		Count += Stack.Item == Item ? Stack.Count : 0;
	}
	return Count;
}

void UOnyxInventoryComponent::SelectSlot(int32 Slot)
{
	const int32 Clamped = FMath::Clamp(Slot, 0, FMath::Min(HotbarSize, Slots.Num()) - 1);
	if (Clamped != Selected)
	{
		Selected = Clamped;
		OnInventoryChanged.Broadcast();
	}
}

void UOnyxInventoryComponent::SelectNext(int32 Step)
{
	const int32 Bar = FMath::Max(FMath::Min(HotbarSize, Slots.Num()), 1);
	SelectSlot(((Selected + Step) % Bar + Bar) % Bar);
}

FVector UOnyxInventoryComponent::GetTargetLocation() const
{
	const AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	if (!Character)
	{
		return GetOwner()->GetActorLocation();
	}
	// From the middle of the cell it stands on, one cell along the way it looks (to the side, up or down).
	const float HalfHeight = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, HalfHeight) + FVector(0.f, 0.f, 12.f);
	const FVector2D Aim = Character->GetAimVector();
	const FVector2D Step = FMath::Abs(Aim.X) >= FMath::Abs(Aim.Y) ? FVector2D(FMath::Sign(Aim.X), 0.f) : FVector2D(0.f, FMath::Sign(Aim.Y));
	FIntPoint Cell;
	if (const AOnyxFarmField* Field = AOnyxFarmField::FindAt(GetWorld(), Feet, Cell))
	{
		return Field->GetCellCenter(Cell + FIntPoint(int32(Step.X), -int32(Step.Y)));
	}
	return Feet + FVector(Step.X, 0.f, Step.Y) * 64.f;
}

bool UOnyxInventoryComponent::UseSelectedItem()
{
	UOnyxItem* Item = GetSelectedItem();
	if (!Item || Item->Kind == EOnyxItemKind::Weapon)
	{
		return false;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < BusyUntil)
	{
		return true;
	}
	FIntPoint Cell;
	const FVector Target = GetTargetLocation();
	const TWeakObjectPtr<AOnyxFarmField> Field = AOnyxFarmField::FindAt(GetWorld(), Target, Cell);
	if (Item->Kind == EOnyxItemKind::Tool)
	{
		const EOnyxToolKind Tool = Item->Tool;
		// Something in the way the tool clears (a stone, weeds, a stump) comes first; the blow lands with the tool.
		if (AOnyxResourceNode* Node = AOnyxResourceNode::FindAt(GetWorld(), Target); Node && Node->WouldTakeHit(Tool))
		{
			const TWeakObjectPtr<AOnyxResourceNode> Hit = Node;
			BusyUntil = Now + PlayUse(Item);
			LandLater(Item, [this, Hit, Tool]()
			{
				if (AOnyxResourceNode* Struck = Hit.Get())
				{
					Struck->TakeHit(Tool, this);
				}
			});
			return true;
		}
		// A tool swings whether or not it hits anything.
		BusyUntil = Now + PlayUse(Item);
		LandLater(Item, [this, Field, Cell, Tool]()
		{
			AOnyxFarmField* Soil = Field.Get();
			switch (Soil ? Tool : EOnyxToolKind::None)
			{
			case EOnyxToolKind::Hoe: Soil->Till(Cell); break;
			case EOnyxToolKind::WateringCan: Soil->Water(Cell); break;
			case EOnyxToolKind::Scythe: Soil->Harvest(Cell, this); break;
			default: break;
			}
		});
		return true;
	}
	if (Item->Kind == EOnyxItemKind::Seeds && Field.IsValid() && Item->Crop)
	{
		const FOnyxSoilCell Soil = Field->GetCell(Cell);
		if (Field->IsInField(Cell) && Soil.bTilled && !Soil.Crop)
		{
			const TWeakObjectPtr<UOnyxItem> Seeds = Item;
			BusyUntil = Now + PlayUse(Item);
			LandLater(Item, [this, Field, Cell, Seeds]()
			{
				UOnyxItem* Sown = Seeds.Get();
				if (Sown && CountOf(Sown) > 0 && Field.IsValid() && Field->Plant(Cell, Sown->Crop))
				{
					RemoveItem(Sown, 1);
				}
			});
			return true;
		}
	}
	// Seeds with nowhere to go, produce, food, materials: nothing happens, and nothing is swung either.
	return true;
}

bool UOnyxInventoryComponent::HarvestInFront()
{
	FIntPoint Cell;
	AOnyxFarmField* Field = AOnyxFarmField::FindAt(GetWorld(), GetTargetLocation(), Cell);
	if (!Field || !Field->Harvest(Cell, this))
	{
		return false;
	}
	AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	if (Character && Character->MovesetComponent && !HarvestClip.IsNone() && Character->MovesetComponent->HasClip(HarvestClip))
	{
		if (UOnyxMovementComponent* Movement = Character->GetOnyxMovement())
		{
			Movement->StopMovementImmediately();
		}
		Character->MovesetComponent->PlayOneShot(HarvestClip);
	}
	return true;
}

void UOnyxInventoryComponent::HandleInteract()
{
	HarvestInFront();
}

void UOnyxInventoryComponent::SetHeldLook(UOnyxItem* Item, UOnyxHeldLook* Look)
{
	if (!Item)
	{
		return;
	}
	if (Look)
	{
		HeldLooks.Add(Item, Look);
	}
	else
	{
		HeldLooks.Remove(Item);
	}
}

UOnyxHeldLook* UOnyxInventoryComponent::GetHeldLook(const UOnyxItem* Item) const
{
	if (!Item)
	{
		return nullptr;
	}
	const TObjectPtr<UOnyxHeldLook>* Skin = HeldLooks.Find(const_cast<UOnyxItem*>(Item));
	return Skin && *Skin ? Skin->Get() : Item->HeldLook.Get();
}

void UOnyxInventoryComponent::LandLater(const UOnyxItem* Item, TFunction<void()> Land)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// A use still waiting to land lands now: a new one never swallows the last.
	if (PendingLand)
	{
		World->GetTimerManager().ClearTimer(LandTimer);
		TFunction<void()> Earlier = MoveTemp(PendingLand);
		PendingLand.Reset();
		Earlier();
	}
	const float After = Item ? Item->UseLandsAfter : 0.f;
	if (After <= 0.f)
	{
		Land();
		return;
	}
	PendingLand = MoveTemp(Land);
	World->GetTimerManager().SetTimer(LandTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		TFunction<void()> Landing = MoveTemp(PendingLand);
		PendingLand.Reset();
		if (Landing)
		{
			Landing();
		}
	}), After, false);
}

void UOnyxInventoryComponent::StartSwing(const UOnyxItem* Item)
{
	AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	if (!Character || !Item || !Item->HeldSprite)
	{
		return;
	}
	if (!SwingSprite)
	{
		SwingSprite = NewObject<UPaperSpriteComponent>(Character, TEXT("HeldItem"));
		SwingSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SwingSprite->SetCastShadow(false);
		SwingSprite->SetupAttachment(Character->GetRootComponent());
		SwingSprite->RegisterComponent();
		UOnyx2DLibrary::ConfigureSpriteLighting(SwingSprite, false);
	}
	SwingSprite->SetSprite(Item->HeldSprite);
	const FVector2D Aim = Character->GetAimVector();
	SwingAim = FMath::Abs(Aim.X) >= FMath::Abs(Aim.Y) ? FVector2D(FMath::Sign(Aim.X), 0.f) : FVector2D(0.f, FMath::Sign(Aim.Y));
	// In front of the body, or behind it when facing away (up the screen).
	SwingSprite->TranslucencySortPriority = SwingAim.Y > 0.5f ? -1 : 1;
	SwingSprite->SetVisibility(true);
	SwingAge = 0.f;
	SetComponentTickEnabled(true);
	TickComponent(0.f, LEVELTICK_All, nullptr);
}

UPaperFlipbookComponent* UOnyxInventoryComponent::MakeHeldLayer(const TCHAR* Name, int32 SortOffset)
{
	AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	UPaperFlipbookComponent* Body = Character ? Character->GetSprite() : nullptr;
	if (!Body)
	{
		return nullptr;
	}
	// On the body itself: the same pivot (the feet), the same turn when it faces left, the same lean.
	UPaperFlipbookComponent* Layer = NewObject<UPaperFlipbookComponent>(Character, Name);
	Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Layer->SetCastShadow(false);
	Layer->SetupAttachment(Body);
	Layer->RegisterComponent();
	Layer->Stop();
	Layer->SetLooping(false);
	Layer->TranslucencySortPriority = Body->TranslucencySortPriority + SortOffset;
	Layer->SetVisibility(false);
	UOnyx2DLibrary::ConfigureSpriteLighting(Layer, false);
	return Layer;
}

void UOnyxInventoryComponent::HideHeldLook()
{
	ShownLook = nullptr;
	ShownRole = NAME_None;
	for (UPaperFlipbookComponent* Layer : { HeldBehind.Get(), HeldInFront.Get() })
	{
		if (Layer)
		{
			Layer->SetVisibility(false);
		}
	}
}

void UOnyxInventoryComponent::UpdateHeldLook()
{
	AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	UOnyxMovesetComponent* Moveset = Character ? Character->MovesetComponent.Get() : nullptr;
	UPaperFlipbookComponent* Body = Character ? Character->GetSprite() : nullptr;
	if (!ShownLook || !Moveset || !Body || !Moveset->IsPlayingOneShot() || Moveset->GetCurrentRole() != ShownRole)
	{
		HideHeldLook();
		return;
	}
	const FOnyxHeldFrames* Frames = ShownLook->Find(ShownRole, Moveset->GetShownDirection());
	auto Show = [Body](UPaperFlipbookComponent* Layer, UPaperFlipbook* Flipbook)
	{
		if (!Layer)
		{
			return;
		}
		if (Layer->GetFlipbook() != Flipbook)
		{
			Layer->SetFlipbook(Flipbook);
		}
		Layer->SetVisibility(Flipbook != nullptr);
		if (Flipbook)
		{
			Layer->SetPlaybackPositionInFrames(Body->GetPlaybackPositionInFrames(), false);
		}
	};
	Show(HeldBehind, Frames ? Frames->Behind.Get() : nullptr);
	Show(HeldInFront, Frames ? Frames->InFront.Get() : nullptr);
}

void UOnyxInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (ShownLook)
	{
		UpdateHeldLook();
	}
	const AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	if (SwingSprite && Character && SwingAge >= 0.f)
	{
		SwingAge += DeltaTime;
		const float Length = FMath::Max(UseTime, 0.05f);
		if (SwingAge >= Length)
		{
			SwingSprite->SetVisibility(false);
			SwingAge = -1.f;
		}
		else
		{
			// Raised over the shoulder, brought down hard: eased in, quick through the middle.
			const float Alpha = FMath::InterpEaseIn(0.f, 1.f, SwingAge / Length, 2.f);
			const float Toward = FMath::RadiansToDegrees(FMath::Atan2(SwingAim.Y, SwingAim.X));
			const float Angle = Toward + SwingArc * (0.5f - Alpha) * (SwingAim.X < 0.f ? -1.f : 1.f);
			const float HalfHeight = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
			const FVector2D Out(FMath::Cos(FMath::DegreesToRadians(Angle)), FMath::Sin(FMath::DegreesToRadians(Angle)));
			// The handle at the hands (a little above the feet), the head pointing out along the arc.
			const FVector Hands(0.f, 0.f, -HalfHeight + 34.f);
			SwingSprite->SetRelativeLocation(Hands + FVector(Out.X, 0.f, Out.Y) * SwingReach * 0.4f);
			SwingSprite->SetRelativeRotation(FRotator(Angle - 90.f, 0.f, 0.f));
		}
	}
	if (!ShownLook && SwingAge < 0.f)
	{
		SetComponentTickEnabled(false);
	}
}

float UOnyxInventoryComponent::PlayClipWithLook(FName Clip, UOnyxHeldLook* Look)
{
	AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	UOnyxMovesetComponent* Moveset = Character ? Character->MovesetComponent.Get() : nullptr;
	if (!Moveset || Clip.IsNone() || !Moveset->HasClip(Clip))
	{
		return 0.f;
	}
	const float Length = Moveset->PlayOneShot(Clip);
	if (Look)
	{
		if (!HeldBehind)
		{
			HeldBehind = MakeHeldLayer(TEXT("HeldBehind"), -1);
		}
		if (!HeldInFront)
		{
			HeldInFront = MakeHeldLayer(TEXT("HeldInFront"), 1);
		}
		ShownLook = Look;
		ShownRole = Clip;
		UpdateHeldLook();
		SetComponentTickEnabled(true);
	}
	else
	{
		HideHeldLook();
	}
	return Length;
}

float UOnyxInventoryComponent::PlayUse(const UOnyxItem* Item)
{
	AOnyxCharacter* Character = Cast<AOnyxCharacter>(GetOwner());
	if (!Character)
	{
		return UseTime;
	}
	// It stops to work the ground.
	if (UOnyxMovementComponent* Movement = Character->GetOnyxMovement())
	{
		Movement->StopMovementImmediately();
	}
	UOnyxMovesetComponent* Moveset = Character->MovesetComponent;
	// Its own clip, with its look in the hands.
	if (Item && Moveset && !Item->UseClip.IsNone() && Moveset->HasClip(Item->UseClip))
	{
		UOnyxHeldLook* Look = GetHeldLook(Item);
		const float Length = PlayClipWithLook(Item->UseClip, Look);
		if (!Look && Item->Kind == EOnyxItemKind::Tool)
		{
			StartSwing(Item);
		}
		return Length > 0.f ? Length : UseTime;
	}
	// No clip of its own: the inventory's, the tool's sprite swung in front.
	if (Item && Item->Kind == EOnyxItemKind::Tool)
	{
		StartSwing(Item);
	}
	if (Moveset && !UseClip.IsNone())
	{
		Moveset->PlayOneShot(UseClip);
	}
	return UseTime;
}

FString UOnyxInventoryComponent::GetSaveKey() const
{
	return TEXT("Onyx.Inventory.") + GetOwner()->GetClass()->GetName();
}

void UOnyxInventoryComponent::WriteSave(FArchive& Archive)
{
	int32 Count = Slots.Num();
	Archive << Count << Selected;
	for (FOnyxItemStack& Stack : Slots)
	{
		FString Path = Stack.Item ? Stack.Item->GetPathName() : FString();
		Archive << Path << Stack.Count;
	}
}

void UOnyxInventoryComponent::ReadSave(FArchive& Archive)
{
	int32 Count = 0;
	Archive << Count << Selected;
	Slots.Reset();
	Slots.SetNum(FMath::Max(SlotCount, 1));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FString Path;
		int32 StackCount = 0;
		Archive << Path << StackCount;
		if (Slots.IsValidIndex(Index) && !Path.IsEmpty() && StackCount > 0)
		{
			Slots[Index].Item = LoadObject<UOnyxItem>(nullptr, *Path);
			Slots[Index].Count = Slots[Index].Item ? StackCount : 0;
		}
	}
	Selected = FMath::Clamp(Selected, 0, FMath::Max(FMath::Min(HotbarSize, Slots.Num()) - 1, 0));
	OnInventoryChanged.Broadcast();
}
