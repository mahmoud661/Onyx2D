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

UENUM(BlueprintType)
enum class EOnyxPickupType : uint8
{
	Coin,
	Health
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxPickupSignature, AActor*, Collector);

/**
 * Coin or health pickup that bobs in place and is collected on touch. Give it a flipbook (spinning gem,
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

	/** Coins added, or health restored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (ClampMin = "0"))
	int32 Amount = 1;

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

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	USceneComponent* GetBobbingComponent() const;

	float Phase = 0.f;
	FVector VisualBase = FVector::ZeroVector;
};
