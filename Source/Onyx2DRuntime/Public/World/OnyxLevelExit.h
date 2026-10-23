// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxLevelExit.generated.h"

class UBoxComponent;
class UOnyxLight2DComponent;
class UOnyxLightFlickerComponent;
class UPaperFlipbook;
class UPaperFlipbookComponent;
class USoundBase;
class UStaticMeshComponent;

/**
 * Door / goal / portal: the player touching it fades out and opens the next level. Give it a Look (a door, a portal, a
 * glowing gate) and a Glow to light what is round it; until it has a look it shows a coloured stand-in box.
 */
UCLASS(meta = (DisplayName = "Onyx Level Exit"))
class ONYX2DRUNTIME_API AOnyxLevelExit : public AActor
{
	GENERATED_BODY()

public:
	AOnyxLevelExit();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Shows Look, standing on its lowest pixel at the bottom of the trigger. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperFlipbookComponent> LookComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxLight2DComponent> Glow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxLightFlickerComponent> GlowPulse;

	/** Its picture (looping): a door, a portal. None: a coloured stand-in box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperFlipbook> Look;

	/** Light it gives round it (0 = none), its colour and reach; it breathes slowly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look", meta = (ClampMin = "0"))
	float GlowIntensity = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	FLinearColor GlowColor = FLinearColor(0.6f, 0.75f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look", meta = (ClampMin = "0"))
	float GlowRadius = 450.f;

	/** Where the light comes from, cm above the bottom of the trigger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	float GlowHeight = 90.f;

	/** Played at the middle of the trigger as the player steps in (the light flares with it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperFlipbook> EnterEffect;

	/** Level to open. Empty = restart the current level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	TSoftObjectPtr<UWorld> NextLevel;

	/** Played when the player steps in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	TObjectPtr<USoundBase> EnterSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (ClampMin = "0"))
	float FadeTime = 0.8f;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void Travel();
	void Flare();

	FTimerHandle TravelTimer;
	FTimerHandle FlareTimer;
	float FlareTime = 0.f;
	bool bTriggered = false;
};
