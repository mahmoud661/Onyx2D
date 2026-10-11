// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "OnyxHitboxComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxHitSignature, AActor*, Victim);

/**
 * Damage volume. Either opened for short windows by attacks (BeginHitWindow/EndHitWindow)
 * or always active for contact damage (bContinuous). Each victim is hit once per window.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent))
class ONYX2DRUNTIME_API UOnyxHitboxComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UOnyxHitboxComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Hitbox", meta = (ClampMin = "0"))
	float Damage = 1.f;

	/** X = away from the attacker, Y = upward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Hitbox")
	FVector2D Knockback = FVector2D(420.f, 260.f);

	/** Real-time freeze when this hitbox connects (0 = off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Hitbox", meta = (ClampMin = "0"))
	float HitStopDuration = 0.06f;

	/** Screen shake added on hit (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Hitbox", meta = (ClampMin = "0", ClampMax = "1"))
	float HitTrauma = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Hitbox")
	FName DamageType = TEXT("Melee");

	/** Always active (enemy bodies, spikes...). Victims can be hit again every RehitInterval seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Hitbox")
	bool bContinuous = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Hitbox", meta = (ClampMin = "0.05", EditCondition = "bContinuous"))
	float RehitInterval = 0.5f;

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Hitbox")
	void BeginHitWindow();

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Hitbox")
	void EndHitWindow();

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Hitbox")
	bool IsHitWindowOpen() const { return bWindowOpen; }

	/** Optional veto (C++): return false to skip hurting this victim, e.g. because it stomped the owner. */
	TFunction<bool(AActor* Victim)> ShouldHit;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Hitbox")
	FOnyxHitSignature OnHit;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void HitOverlappingActors();
	void TryHit(AActor* Victim);

	TSet<TWeakObjectPtr<AActor>> HitThisWindow;
	float RehitTimer = 0.f;
	bool bWindowOpen = false;
};
