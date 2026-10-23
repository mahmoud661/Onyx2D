// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OnyxSaveSubsystem.h"
#include "GameFramework/Actor.h"
#include "Items/OnyxInventoryComponent.h"
#include "World/OnyxInteractable.h"
#include "OnyxShippingBin.generated.h"

class UBoxComponent;
class UPaperSprite;
class UPaperSpriteComponent;
class USoundBase;

/**
 * Where the farm's goods are sold: Interact (prompt "Ship") puts the stack in the selected hotbar slot into the bin if it
 * has a price; overnight (a new day on the Onyx Calendar) everything in it is sold for its price in coins and the player
 * is told what it fetched. What's in the bin is kept in the save.
 */
UCLASS(meta = (DisplayName = "Onyx Shipping Bin"))
class ONYX2DRUNTIME_API AOnyxShippingBin : public AActor, public IOnyxInteractable, public IOnyxSaveable
{
	GENERATED_BODY()

public:
	AOnyxShippingBin();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperSpriteComponent> Sprite;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Shipping")
	TObjectPtr<UPaperSprite> Look;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Shipping")
	TObjectPtr<USoundBase> ShipSound;

	/** What's waiting to be sold tonight. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Onyx2D|Shipping")
	TArray<FOnyxItemStack> Contents;

	/** What tonight's goods will fetch. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Shipping")
	int32 GetValue() const;

	/** Sells everything in the bin now (what the night does). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Shipping")
	int32 SellAll();

	// IOnyxInteractable
	virtual void Interact_Implementation(AActor* User) override;
	virtual FText GetInteractionPrompt_Implementation() const override { return INVTEXT("Ship"); }

	// IOnyxSaveable
	virtual FString GetSaveKey() const override { return TEXT("Onyx.ShippingBin.") + GetName(); }
	virtual void WriteSave(FArchive& Archive) override;
	virtual void ReadSave(FArchive& Archive) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleNewDay(int32 Day);
};
