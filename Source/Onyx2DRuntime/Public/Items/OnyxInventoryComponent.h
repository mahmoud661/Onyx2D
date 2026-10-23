// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/OnyxSaveSubsystem.h"
#include "OnyxInventoryComponent.generated.h"

class UOnyxHeldLook;
class UOnyxItem;
class UPaperFlipbookComponent;

/** Some of one item in a slot. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UOnyxItem> Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0"))
	int32 Count = 0;

	bool IsEmpty() const { return !Item || Count <= 0; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnyxInventoryChangedSignature);

/**
 * What a character carries (Onyx Inventory): slots of item stacks, the first Hotbar Size of them on the hotbar with one
 * selected. Attack uses the selected item when it is a tool or seeds (the hoe tills the cell in front, the can waters
 * it, seeds sow it, the scythe picks a ready crop); with a weapon or nothing selected Attack swings as usual. Interact
 * on nothing picks a ready crop in front. The hotbar inputs choose the slot. Kept in the save.
 *
 * An item with a Use Clip plays it, its Held Look drawn in the character's hands frame for frame (behind and in front of the
 * body), and lands Use Lands After seconds in. Set Held Look shows another look for an item (a skin) on this character only.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Onyx Inventory"))
class ONYX2DRUNTIME_API UOnyxInventoryComponent : public UActorComponent, public IOnyxSaveable
{
	GENERATED_BODY()

public:
	UOnyxInventoryComponent();

	/** How many slots it has, and how many of them are the hotbar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Inventory", meta = (ClampMin = "1", ClampMax = "200"))
	int32 SlotCount = 36;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Inventory", meta = (ClampMin = "1", ClampMax = "20"))
	int32 HotbarSize = 12;

	/** What it starts with in a new game (a loaded save brings what it had instead), and the coins (Onyx Score) to start with. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Inventory")
	TArray<FOnyxItemStack> StartingItems;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Inventory", meta = (ClampMin = "0"))
	int32 StartingCoins = 0;

	/**
	 * The clip played when using a tool or sowing that has no Use Clip of its own (a Moveset role), and how long using such an
	 * item keeps it busy (one with a Use Clip is busy for that clip's length).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Inventory")
	FName UseClip = TEXT("Attack1");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Inventory", meta = (ClampMin = "0", Units = "s"))
	float UseTime = 0.35f;

	/** The clip played when picking a ready crop in front (Interact on nothing); none: none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Inventory")
	FName HarvestClip;

	/** Tools swing their Held Sprite through this arc (degrees) in front of the user, this far out (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Inventory", meta = (ClampMin = "0", ClampMax = "360"))
	float SwingArc = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Inventory", meta = (ClampMin = "0"))
	float SwingReach = 30.f;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Inventory")
	const TArray<FOnyxItemStack>& GetSlots() const { return Slots; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Inventory")
	int32 GetSelectedSlot() const { return Selected; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Inventory")
	UOnyxItem* GetSelectedItem() const;

	/** Adds items, filling stacks of the same item first. Returns how many didn't fit. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Inventory")
	int32 AddItem(UOnyxItem* Item, int32 Count = 1);

	/** Takes items away (from the last slots first). Returns how many were taken. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Inventory")
	int32 RemoveItem(UOnyxItem* Item, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Inventory")
	int32 CountOf(const UOnyxItem* Item) const;

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Inventory")
	void SelectSlot(int32 Slot);

	/** Steps along the hotbar (+1 the next slot, -1 the previous), wrapping round. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Inventory")
	void SelectNext(int32 Step = 1);

	/** Uses the selected item on the cell in front. False when the item is something to swing (let Attack happen). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Inventory")
	bool UseSelectedItem();

	/** Picks a ready crop in front, if there is one. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Inventory")
	bool HarvestInFront();

	/** Shows Look for Item in this character's hands instead of the item's own Held Look (a skin); none: the item's own again. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Inventory")
	void SetHeldLook(UOnyxItem* Item, UOnyxHeldLook* Look);

	/** The look Item shows in this character's hands: its skin here, else its own Held Look. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Inventory")
	UOnyxHeldLook* GetHeldLook(const UOnyxItem* Item) const;

	/**
	 * Plays a clip on its owner with Look in the hands, frame for frame (a bow drawn, a stone thrown, a lantern raised), the way
	 * using an item does. Returns the clip's length (0 if the owner has no such clip).
	 */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Inventory")
	float PlayClipWithLook(FName Clip, UOnyxHeldLook* Look);

	/** The cell in front of its owner (top down): where tools land. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Inventory")
	FVector GetTargetLocation() const;

	/** The slots or the selection changed. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Inventory")
	FOnyxInventoryChangedSignature OnInventoryChanged;

	// IOnyxSaveable
	virtual FString GetSaveKey() const override;
	virtual void WriteSave(FArchive& Archive) override;
	virtual void ReadSave(FArchive& Archive) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleInteract();

	/** Plays using Item; returns how long it keeps the character busy. */
	float PlayUse(const UOnyxItem* Item);
	/** Starts the held item's swing (tools without a Held Look). */
	void StartSwing(const UOnyxItem* Item);
	/** Runs Land now, or when the use lands (Use Lands After). */
	void LandLater(const UOnyxItem* Item, TFunction<void()> Land);
	/** Keeps the held look's frames on the body's frame and direction; hides it when its clip is over. */
	void UpdateHeldLook();
	void HideHeldLook();
	UPaperFlipbookComponent* MakeHeldLayer(const TCHAR* Name, int32 SortOffset);

	UPROPERTY(Transient)
	TObjectPtr<class UPaperSpriteComponent> SwingSprite;

	/** The held look in the hands while its clip plays, under and over the body. */
	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbookComponent> HeldBehind;

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbookComponent> HeldInFront;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxHeldLook> ShownLook;

	FName ShownRole;

	/** Skins: the looks this character shows for items instead of their own. */
	UPROPERTY(VisibleInstanceOnly, Category = "Onyx2D|Inventory")
	TMap<TObjectPtr<UOnyxItem>, TObjectPtr<UOnyxHeldLook>> HeldLooks;

	FTimerHandle LandTimer;
	TFunction<void()> PendingLand;

	float SwingAge = -1.f;
	FVector2D SwingAim = FVector2D(0.f, -1.f);

	UPROPERTY(VisibleInstanceOnly, Category = "Onyx2D|Inventory")
	TArray<FOnyxItemStack> Slots;

	int32 Selected = 0;
	double BusyUntil = 0.0;
};
