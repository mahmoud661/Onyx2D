// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxGate.generated.h"

class UBoxComponent;
class UPaperFlipbook;
class UPaperSprite;
class UPaperSpriteComponent;
class USoundBase;

/**
 * Onyx Gate: a barrier that slams down to shut a way and slides up to open it (a portcullis, a boss arena's doors, a
 * locked passage). Closed, it blocks characters and projectiles. Size its Block to the doorway and give it a Look.
 */
UCLASS(meta = (DisplayName = "Onyx Gate"))
class ONYX2DRUNTIME_API AOnyxGate : public AActor
{
	GENERATED_BODY()

public:
	AOnyxGate();

	/** What moves: the block and the look slide on it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<USceneComponent> Leaf;

	/** Blocks the way while closed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Block;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperSpriteComponent> Look;

	/** Shut from the start (opened by its arena, a switch...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Gate")
	bool bStartsClosed = false;

	/** How far up it slides when open, cm (into the wall above the doorway). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Gate", meta = (ClampMin = "0"))
	float Travel = 280.f;

	/** Seconds to slam shut... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Gate", meta = (ClampMin = "0.02", Units = "s"))
	float CloseTime = 0.22f;

	/** ...and to grind open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Gate", meta = (ClampMin = "0.02", Units = "s"))
	float OpenTime = 1.2f;

	/** Screen shake as it slams shut (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Gate", meta = (ClampMin = "0", ClampMax = "1"))
	float SlamTrauma = 0.35f;

	/** Played at its foot as it slams shut (dust). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Gate")
	TObjectPtr<UPaperFlipbook> SlamEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Gate")
	TObjectPtr<USoundBase> CloseSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Gate")
	TObjectPtr<USoundBase> OpenSound;

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Gate")
	void Open();

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Gate")
	void Close();

	/** Back to how it started, at once (an arena resetting after the player fell). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Gate")
	void ResetGate();

	/** Shut, or shutting. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Gate")
	bool IsClosed() const { return bWantClosed; }

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void SetShut(float NewShut);

	/** 0 open (up), 1 shut (down). */
	float Shut = 0.f;
	bool bWantClosed = false;
};
