// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OnyxSaveSubsystem.h"
#include "GameFramework/Actor.h"
#include "Items/OnyxItem.h"
#include "OnyxResourceNode.generated.h"

class UBoxComponent;
class UOnyxInventoryComponent;
class UPaperFlipbook;
class UPaperSprite;
class UPaperSpriteComponent;
class USoundBase;

/**
 * Something in the way that a tool clears (top down): a stone the pickaxe breaks, weeds the scythe cuts, a stump or twigs
 * the axe chops. It blocks walking until cleared, takes Hits blows of the right tool (it shakes at each), then gives its
 * drops to the one who cleared it, with an effect and a sound. Cleared nodes stay cleared in the save.
 */
UCLASS(meta = (DisplayName = "Onyx Resource Node"))
class ONYX2DRUNTIME_API AOnyxResourceNode : public AActor, public IOnyxSaveable
{
	GENERATED_BODY()

public:
	AOnyxResourceNode();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Blocker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperSpriteComponent> Sprite;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Resource")
	TObjectPtr<UPaperSprite> Look;

	/** The tool that clears it, and how many blows it takes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Resource")
	EOnyxToolKind Tool = EOnyxToolKind::Pickaxe;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Resource", meta = (ClampMin = "1"))
	int32 Hits = 1;

	/** What clearing it gives. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Resource")
	TObjectPtr<UOnyxItem> Drop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Resource", meta = (ClampMin = "0"))
	int32 DropCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Resource|Feel")
	TObjectPtr<UPaperFlipbook> ClearEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D|Resource|Feel")
	TObjectPtr<USoundBase> HitSound;

	/** The node a tool lands on at a world location (top down), if any. */
	static AOnyxResourceNode* FindAt(const UWorld* World, const FVector& Location);

	/** Whether a blow with this tool would land on it (the right tool, and it isn't cleared yet). */
	bool WouldTakeHit(EOnyxToolKind WithTool) const { return !bCleared && WithTool == Tool; }

	/** A blow with a tool: true if it was the right tool (it shakes, or clears on its last blow). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Resource")
	bool TakeHit(EOnyxToolKind WithTool, UOnyxInventoryComponent* Into);

	// IOnyxSaveable
	virtual FString GetSaveKey() const override { return TEXT("Onyx.Node.") + GetName(); }
	virtual void WriteSave(FArchive& Archive) override;
	virtual void ReadSave(FArchive& Archive) override;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Clear(bool bQuietly);

	int32 HitsTaken = 0;
	bool bCleared = false;
	float Shake = 0.f;
	FVector SpriteRest = FVector::ZeroVector;
};
