// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OnyxCharacterEffectsComponent.generated.h"

class AOnyxCharacter;
class UPaperFlipbook;

/**
 * Character Effects: the small things that make a character feel alive. Dust at its feet when it jumps off the ground or a
 * wall, lands and rolls (an optional effect of its own for a jump in the air);
 * fading afterimages while it dashes; blinking while it can't be hurt after a hit.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Character Effects"))
class ONYX2DRUNTIME_API UOnyxCharacterEffectsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxCharacterEffectsComponent();

	/** Kicked up from the ground when it jumps, and off a wall when it jumps from one (not out of water). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Effects|Dust")
	TObjectPtr<UPaperFlipbook> JumpDust;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Effects|Dust")
	TObjectPtr<UPaperFlipbook> LandDust;

	/** Kicked up by a roll (a dash that starts on the ground); a dash in the air or under water leaves none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Effects|Dust")
	TObjectPtr<UPaperFlipbook> DashDust;

	/** Played at its feet for a jump in the air (a puff of air, a ring). None by default: there is no ground there to raise dust. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Effects|Dust")
	TObjectPtr<UPaperFlipbook> AirJumpEffect;

	/** Fading copies of itself left behind while it dashes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Effects|Afterimages")
	bool bDashAfterimages = true;

	/** Their tint; its alpha is how strong they start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Effects|Afterimages", meta = (EditCondition = "bDashAfterimages"))
	FLinearColor AfterimageColor = FLinearColor(0.55f, 0.8f, 1.f, 0.55f);

	/** One every this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Effects|Afterimages", meta = (ClampMin = "0.01", EditCondition = "bDashAfterimages"))
	float AfterimageInterval = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Effects|Afterimages", meta = (ClampMin = "0.05", EditCondition = "bDashAfterimages"))
	float AfterimageLifetime = 0.25f;

	/** Blinks while it can't be hurt, after taking a hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Effects|Blink")
	bool bBlinkWhileInvulnerable = true;

	/** Blinks per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Effects|Blink", meta = (ClampMin = "1", EditCondition = "bBlinkWhileInvulnerable"))
	float BlinkRate = 15.f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void HandleJumped();

	UFUNCTION()
	void HandleAirJumped();

	UFUNCTION()
	void HandleWallJumped();

	UFUNCTION()
	void HandleLanded();

	UFUNCTION()
	void HandleDashed();

	void SpawnDust(UPaperFlipbook* Dust) const;
	void LeaveAfterimage() const;

	TWeakObjectPtr<AOnyxCharacter> Character;
	float AfterimageTimer = 0.f;
	float BlinkTime = 0.f;
	bool bBlinkHidden = false;
};
