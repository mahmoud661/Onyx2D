// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/OnyxInteractable.h"
#include "OnyxSign.generated.h"

class UBoxComponent;
class UOnyxAction;
class UPaperSprite;
class UPaperSpriteComponent;
class UStaticMeshComponent;

/**
 * A sign the player reads: walk up, press Interact, and its pages show in the dialogue box. Or set Read By Walking Past
 * to show the first page as a message with no button. Shows a sprite (or a wooden stand-in until it has art).
 */
UCLASS(meta = (DisplayName = "Onyx Sign"))
class ONYX2DRUNTIME_API AOnyxSign : public AActor, public IOnyxInteractable
{
	GENERATED_BODY()

public:
	AOnyxSign();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperSpriteComponent> Sprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UStaticMeshComponent> Placeholder;

	/** What the sign says. Each entry is one page; the player presses Jump or Interact to turn it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Sign", meta = (MultiLine = true))
	TArray<FText> Pages;

	/** Show the first page as a message when the player walks past, instead of waiting for Interact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Sign")
	bool bReadByWalkingPast = false;

	/** What happens once the player has read it (the first time only). */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Onyx2D|Sign")
	TArray<TObjectPtr<UOnyxAction>> AfterReading;

	/** The word on the Interact prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Sign")
	FText Prompt = INVTEXT("Read");

	/** The sign's art (empty = a wooden stand-in). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperSprite> Look;

	/** Shows the pages now, as if the player pressed Interact. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Sign")
	void Read(AActor* Reader);

	// IOnyxInteractable
	virtual void Interact_Implementation(AActor* User) override;
	virtual bool IsInteractionLocked_Implementation(AActor* User) const override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void FinishReading(AActor* Reader);

	bool bRead = false;
};
