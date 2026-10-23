// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxShop.h"
#include "Components/BoxComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Core/OnyxScoreSubsystem.h"
#include "Items/OnyxInventoryComponent.h"
#include "Items/OnyxItem.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"

FOnyxShopOpenedSignature AOnyxShop::OnShopOpened;

AOnyxShop::AOnyxShop()
{
	PrimaryActorTick.bCanEverTick = false;
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->InitBoxExtent(FVector(60.f, 100.f, 50.f));
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

void AOnyxShop::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Sprite->SetSprite(Look);
	Sprite->SetVisibility(Look != nullptr);
	if (Look)
	{
		UOnyx2DLibrary::ConfigureSpriteLighting(Sprite, false);
	}
}

int32 AOnyxShop::GetPrice(const UOnyxItem* Item) const
{
	return Item ? FMath::Max(FMath::RoundToInt(Item->Price * PriceScale), 0) : 0;
}

bool AOnyxShop::Buy(int32 Index, AActor* Buyer)
{
	UOnyxItem* Item = Stock.IsValidIndex(Index) ? Stock[Index].Get() : nullptr;
	UOnyxInventoryComponent* Inventory = Buyer ? Buyer->FindComponentByClass<UOnyxInventoryComponent>() : nullptr;
	UOnyxScoreSubsystem* Score = UOnyxScoreSubsystem::Get(this);
	if (!Item || !Inventory || !Score || Score->GetCoins() < GetPrice(Item))
	{
		return false;
	}
	if (Inventory->AddItem(Item, 1) > 0)
	{
		return false; // no room
	}
	Score->SpendCoins(GetPrice(Item));
	return true;
}

void AOnyxShop::Interact_Implementation(AActor* User)
{
	OnShopOpened.Broadcast(this, User);
}
