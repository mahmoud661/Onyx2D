// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OnyxItem.generated.h"

class UOnyxCrop;
class UOnyxHeldLook;
class UPaperSprite;
class UTexture2D;

/** What kind of thing an item is: it decides what using it does. */
UENUM(BlueprintType)
enum class EOnyxItemKind : uint8
{
	/** Wood, stone, fibre: kept, sold, crafted with. */
	Material,
	/** Used on the ground in front of the player (see Tool). */
	Tool,
	/** Planted in tilled soil (see Crop). */
	Seeds,
	/** What a crop gives. */
	Produce,
	/** Swung by the Attack input as usual. */
	Weapon,
	Food
};

/** Which tool. */
UENUM(BlueprintType)
enum class EOnyxToolKind : uint8
{
	None,
	/** Tills soil. */
	Hoe,
	/** Waters tilled soil. */
	WateringCan,
	/** Cuts weeds and harvests crops. */
	Scythe,
	Axe,
	Pickaxe
};

/**
 * Something the player can carry: a tool, seeds, a crop, a material. One Data Asset per item; inventories hold stacks of
 * them (Onyx Inventory).
 */
UCLASS(BlueprintType, meta = (DisplayName = "Onyx Item"))
class ONYX2DRUNTIME_API UOnyxItem : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText Description;

	/** Its picture in the hotbar and the backpack (a small pixel-art texture, drawn at whole-pixel scale). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon;

	/**
	 * How it looks in the hand when it has no Held Look: tools swing it through an arc in front of the user (a sprite pivoted at
	 * its handle).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UPaperSprite> HeldSprite;

	/** The clip its user plays when using it (a Moveset role: Till, Water, Sow); none: the inventory's Use Clip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use")
	FName UseClip;

	/** Seconds into Use Clip when it lands: the soil turns, the water reaches it, the seed goes in. 0: at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use", meta = (ClampMin = "0", Units = "s"))
	float UseLandsAfter = 0.f;

	/**
	 * How it looks in the hands while Use Clip plays (drawn frame for frame with the character's clips, which hold empty hands).
	 * Another look is another skin: an inventory can also show a look of its own for an item (Onyx Inventory > Set Held Look).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use")
	TObjectPtr<UOnyxHeldLook> HeldLook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EOnyxItemKind Kind = EOnyxItemKind::Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (EditCondition = "Kind == EOnyxItemKind::Tool"))
	EOnyxToolKind Tool = EOnyxToolKind::None;

	/** What these seeds grow into. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (EditCondition = "Kind == EOnyxItemKind::Seeds"))
	TObjectPtr<UOnyxCrop> Crop;

	/** How many fit in one slot (tools: 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStack = 99;

	/** What a shop pays for one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0"))
	int32 Price = 0;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("OnyxItem"), GetFName()); }
};
