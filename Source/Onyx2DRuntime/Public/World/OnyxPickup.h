// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxPickup.generated.h"

class UPaperFlipbook;
class UPaperFlipbookComponent;
class USphereComponent;
class UStaticMeshComponent;
class USoundBase;

/** What a pickup gives when it is collected. */
UENUM(BlueprintType)
enum class EOnyxPickupType : uint8
{
	Coin,
	Health,
	/** A named item, like a key: Trigger Zones can need it. */
	Item
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxPickupSignature, AActor*, Collector);

/**
 * Coin, health or item (key) pickup that bobs in place and is collected on touch. Give it a flipbook (spinning gem,
 * cherry...) to replace the placeholder, and a collect effect to play where it disappears.
 */
UCLASS(meta = (DisplayName = "Onyx Pickup"))
class ONYX2DRUNTIME_API AOnyxPickup : public AActor
{
	GENERATED_BODY()

public:
	AOnyxPickup();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<USphereComponent> Trigger;

	/** Placeholder visual; replace with a sprite or flipbook in a Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	EOnyxPickupType Type = EOnyxPickupType::Coin;

	/** Coins added, health restored, or how many of the item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (ClampMin = "0"))
	int32 Amount = 1;

	/** The item's name. A Trigger Zone that needs it uses the same name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (EditCondition = "Type == EOnyxPickupType::Item", EditConditionHides))
	FName ItemName = TEXT("Key");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	float BobHeight = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	float BobSpeed = 3.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperFlipbookComponent> Animated;

	/** Animated look (empty = glowing placeholder gem). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperFlipbook> Flipbook;

	/** Played once where the pickup was collected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperFlipbook> CollectEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	TObjectPtr<USoundBase> CollectSound;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D")
	FOnyxPickupSignature OnCollected;

	virtual void OnConstruction(const FTransform& Transform) override;

	/**
	 * Throws it (cm/s across and up): it flies, falls, bounces off walls and lands, then bobs there. Coins bursting from a
	 * foe. In a Top Down level it skips across the ground instead (Across sideways, scattered up and down the screen by Up)
	 * with a hop in the picture.
	 */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D")
	void Pop(FVector2D Velocity);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	USceneComponent* GetBobbingComponent() const;
	void TickPop(float DeltaSeconds);
	/** Collected by a player it already touches (one that was standing there when it landed). */
	void CollectIfTouching();

	FVector2D PopVelocity = FVector2D::ZeroVector;
	bool bPopping = false;
	/** Top down: thrown across the ground, the picture hopping above it (its height and rising speed). */
	void TickPopTopDown(float DeltaSeconds);
	bool bTopDownPop = false;
	float PopHop = 0.f;
	float PopRise = 0.f;

	float Phase = 0.f;
	FVector VisualBase = FVector::ZeroVector;
};
