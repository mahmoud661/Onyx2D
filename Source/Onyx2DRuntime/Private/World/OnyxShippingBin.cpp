// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxShippingBin.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Components/BoxComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Core/OnyxScoreSubsystem.h"
#include "Items/OnyxItem.h"
#include "Logic/OnyxDialogueSubsystem.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "Time/OnyxCalendarSubsystem.h"

AOnyxShippingBin::AOnyxShippingBin()
{
	PrimaryActorTick.bCanEverTick = false;
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->InitBoxExtent(FVector(50.f, 100.f, 50.f));
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetCanEverAffectNavigation(false);
	Trigger->bDrawOnlyIfSelected = true;
	RootComponent = Trigger;

	Sprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Trigger);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AOnyxShippingBin::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Sprite->SetSprite(Look);
	Sprite->SetVisibility(Look != nullptr);
	if (Look)
	{
		UOnyx2DLibrary::ConfigureSpriteLighting(Sprite, false);
	}
}

void AOnyxShippingBin::BeginPlay()
{
	Super::BeginPlay();
	if (UOnyxCalendarSubsystem* Calendar = UOnyxCalendarSubsystem::Get(this))
	{
		Calendar->OnNewDay.AddUniqueDynamic(this, &AOnyxShippingBin::HandleNewDay);
	}
	if (UOnyxSaveSubsystem* Save = UOnyxSaveSubsystem::Get(this))
	{
		Save->Register(this);
	}
}

void AOnyxShippingBin::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UOnyxSaveSubsystem* Save = UOnyxSaveSubsystem::Get(this))
	{
		Save->Unregister(this);
	}
	Super::EndPlay(EndPlayReason);
}

int32 AOnyxShippingBin::GetValue() const
{
	int32 Value = 0;
	for (const FOnyxItemStack& Stack : Contents)
	{
		Value += Stack.Item ? Stack.Item->Price * Stack.Count : 0;
	}
	return Value;
}

void AOnyxShippingBin::Interact_Implementation(AActor* User)
{
	UOnyxInventoryComponent* Inventory = User ? User->FindComponentByClass<UOnyxInventoryComponent>() : nullptr;
	UOnyxItem* Item = Inventory ? Inventory->GetSelectedItem() : nullptr;
	UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(this);
	if (!Item || Item->Price <= 0 || Item->Kind == EOnyxItemKind::Tool || Item->Kind == EOnyxItemKind::Weapon)
	{
		if (Dialogue)
		{
			Dialogue->ShowMessage(GetValue() > 0
				? FText::Format(INVTEXT("Tonight's shipment will fetch {0} coins."), FText::AsNumber(GetValue()))
				: INVTEXT("Hold something to sell and put it in the bin."), 2.5f);
		}
		return;
	}
	const int32 Count = Inventory->CountOf(Item) > 0 ? Inventory->GetSlots()[Inventory->GetSelectedSlot()].Count : 0;
	const int32 Taken = Inventory->RemoveItem(Item, Count);
	if (Taken <= 0)
	{
		return;
	}
	FOnyxItemStack* Same = Contents.FindByPredicate([Item](const FOnyxItemStack& Stack) { return Stack.Item == Item; });
	if (Same)
	{
		Same->Count += Taken;
	}
	else
	{
		Contents.Add({ Item, Taken });
	}
	if (ShipSound)
	{
		UOnyxAudioSubsystem::PlaySoundAt(this, ShipSound, GetActorLocation());
	}
	if (Dialogue)
	{
		Dialogue->ShowMessage(FText::Format(INVTEXT("{0} x {1} shipped ({2} coins tonight)"), Item->DisplayName, FText::AsNumber(Taken), FText::AsNumber(GetValue())), 2.f);
	}
}

int32 AOnyxShippingBin::SellAll()
{
	const int32 Value = GetValue();
	if (Value > 0)
	{
		if (UOnyxScoreSubsystem* Score = UOnyxScoreSubsystem::Get(this))
		{
			Score->AddCoins(Value);
		}
		if (UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(this))
		{
			Dialogue->ShowMessage(FText::Format(INVTEXT("Last night's shipment sold for {0} coins."), FText::AsNumber(Value)), 4.f);
		}
	}
	Contents.Reset();
	return Value;
}

void AOnyxShippingBin::HandleNewDay(int32 Day)
{
	SellAll();
}

void AOnyxShippingBin::WriteSave(FArchive& Archive)
{
	int32 Count = Contents.Num();
	Archive << Count;
	for (FOnyxItemStack& Stack : Contents)
	{
		FString Path = Stack.Item ? Stack.Item->GetPathName() : FString();
		Archive << Path << Stack.Count;
	}
}

void AOnyxShippingBin::ReadSave(FArchive& Archive)
{
	int32 Count = 0;
	Archive << Count;
	Contents.Reset();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FString Path;
		int32 StackCount = 0;
		Archive << Path << StackCount;
		if (UOnyxItem* Item = Path.IsEmpty() ? nullptr : LoadObject<UOnyxItem>(nullptr, *Path))
		{
			Contents.Add({ Item, StackCount });
		}
	}
}
