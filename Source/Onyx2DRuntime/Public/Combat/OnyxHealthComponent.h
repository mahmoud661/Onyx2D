// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/OnyxCombatTypes.h"
#include "OnyxHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnyxHealthChangedSignature, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxDamagedSignature, const FOnyxDamageInfo&, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxDeathSignature, AActor*, Killer);

/** Health, invulnerability frames and death. Knows nothing about visuals or movement. */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent))
class ONYX2DRUNTIME_API UOnyxHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Health", meta = (ClampMin = "1"))
	float MaxHealth = 5.f;

	/** Seconds of invulnerability after taking damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Health", meta = (ClampMin = "0"))
	float InvulnerabilityTime = 0.6f;

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Health")
	bool ApplyDamage(const FOnyxDamageInfo& Damage);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Health")
	void Heal(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Health")
	void Kill(AActor* Killer = nullptr);

	/** Restores full health and clears the dead state. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Health")
	void Revive();

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Health")
	void SetInvulnerableFor(float Seconds);

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Health")
	float GetHealthFraction() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Health")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Health")
	bool IsInvulnerable() const;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Health")
	FOnyxHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Health")
	FOnyxDamagedSignature OnDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Health")
	FOnyxDeathSignature OnDeath;

protected:
	virtual void BeginPlay() override;

private:
	void SetHealth(float NewHealth);

	UPROPERTY(VisibleInstanceOnly, Category = "Onyx2D|Health")
	float Health = 0.f;

	double InvulnerableUntil = 0.0;
	bool bDead = false;
};
