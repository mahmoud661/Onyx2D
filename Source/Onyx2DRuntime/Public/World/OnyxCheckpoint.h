// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxCheckpoint.generated.h"

class UBoxComponent;
class USoundBase;
class UOnyxLight2DComponent;
class UOnyxLightFlickerComponent;
class UPaperFlipbook;
class UPaperFlipbookComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxCheckpointSignature, AActor*, Player);

/**
 * Touching it makes it the player's respawn point and heals them (optional). It can be drawn (a shrine, a lamp): a look
 * before it is reached and one after, an effect and a flare of its glow as it lights. Without art it is a plain post.
 */
UCLASS(meta = (DisplayName = "Onyx Checkpoint"))
class ONYX2DRUNTIME_API AOnyxCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	AOnyxCheckpoint();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxLight2DComponent> Glow;

	/** Its picture: Dormant Look until it is reached, Active Look after (standing on its lowest pixel). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperFlipbookComponent> Look;

	/** The lit glow's flicker (a flame). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxLightFlickerComponent> GlowFlicker;

	/** What it looks like before it is reached: a cold lamp, a dark shrine (none: the plain post). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperFlipbook> DormantLook;

	/** ...and once reached: its flame lit (none: Dormant Look stays). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperFlipbook> ActiveLook;

	/** Played at its glow as it is first reached (embers bursting, a ring of light). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperFlipbook> ActivateEffect;

	/** Played at its glow each time it heals the player again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	TObjectPtr<UPaperFlipbook> RestEffect;

	/** Its glow before it is reached: a faint ember, so it can be found in the dark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look", meta = (ClampMin = "0"))
	float DormantGlow = 0.f;

	/** Its glow once reached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look", meta = (ClampMin = "0"))
	float ActiveGlow = 1.4f;

	/** As it lights, its glow flares this many times brighter, settling over Kindle Time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look", meta = (ClampMin = "1"))
	float KindleFlare = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look", meta = (ClampMin = "0.05", Units = "s"))
	float KindleTime = 1.2f;

	/**
	 * How much its picture is kept from washing out (see Onyx Character's Light Guard): its glow is its own, lighting the
	 * ground and the walls around it more than its own stone, which keeps its shape in the light.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look", meta = (ClampMin = "0", ClampMax = "1"))
	float LightGuard = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	bool bHealOnActivate = true;

	/** Coming back to it later heals again (a resting place), not only the first time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (EditCondition = "bHealOnActivate"))
	bool bHealOnEveryVisit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	FLinearColor InactiveColor = FLinearColor(0.25f, 0.27f, 0.32f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	FLinearColor ActiveColor = FLinearColor(0.3f, 0.95f, 0.9f);

	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	bool IsActivated() const { return bActivated; }

	/** Played when the player reaches it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	TObjectPtr<USoundBase> ActivateSound;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D")
	FOnyxCheckpointSignature OnActivated;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ApplyLook();
	void PlayEffect(UPaperFlipbook* Effect);

	bool bActivated = false;
	/** The kindle flare still to settle, seconds. */
	float FlareLeft = 0.f;
};
