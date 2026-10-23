// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/OnyxInteractable.h"
#include "OnyxShop.generated.h"

class AOnyxShop;
class UBoxComponent;
class UOnyxItem;
class UPaperSprite;
class UPaperSpriteComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnyxShopOpenedSignature, AOnyxShop*, AActor*);

/**
 * A shop counter: Interact (prompt "Shop") opens its list (the Onyx UI shop screen), where the player buys its stock with
 * coins (Onyx Score) into their Onyx Inventory. Put it on a counter, a stall or in front of the shopkeeper.
 */
UCLASS(meta = (DisplayName = "Onyx Shop"))
class ONYX2DRUNTIME_API AOnyxShop : public AActor, public IOnyxInteractable
{
	GENERATED_BODY()

public:
	AOnyxShop();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperSpriteComponent> Sprite;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Shop")
	TObjectPtr<UPaperSprite> Look;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Shop")
	FText ShopName = INVTEXT("Shop");

	/** What it sells. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Shop")
	TArray<TObjectPtr<UOnyxItem>> Stock;

	/** What it asks for an item, as a share of the item's price. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Shop", meta = (ClampMin = "0"))
	float PriceScale = 1.f;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Shop")
	int32 GetPrice(const UOnyxItem* Item) const;

	/** Buys one of the stock (by its place in the list) for the buyer. False when they can't pay or carry it. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Shop")
	bool Buy(int32 Index, AActor* Buyer);

	/** A shop was opened: the UI shows its screen. */
	static FOnyxShopOpenedSignature OnShopOpened;

	// IOnyxInteractable
	virtual void Interact_Implementation(AActor* User) override;
	virtual FText GetInteractionPrompt_Implementation() const override { return INVTEXT("Shop"); }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
};
