// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxProjectile.generated.h"

class UOnyxLight2DComponent;
class UPaperFlipbook;
class UPaperFlipbookComponent;
class USoundBase;
class USphereComponent;

/** What a projectile looks like and does (a fireball, an arrow, a wisp). */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxProjectileSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	TObjectPtr<UPaperFlipbook> Look;

	/** Speed when fired, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0"))
	float Speed = 520.f;

	/** 0 = flies straight, 1 = falls like everything else. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0", UIMax = "1"))
	float Gravity = 0.f;

	/** The air slows it by this share of its speed each second (an arrow loses a little; 0: none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0", UIMax = "1"))
	float Drag = 0.f;

	/**
	 * Striking a wall, a floor or a platform it sticks there, its point sunk in (an arrow), for this long, seconds, then fades
	 * away; 0: it bursts on them. It hurts nobody once it is stuck.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0"))
	float StickTime = 0.f;

	/** How far its point sinks into what it sticks in, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0"))
	float StickDepth = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0"))
	float Damage = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FVector2D Knockback = FVector2D(380.f, 240.f);

	/** Collision radius, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "1"))
	float Radius = 16.f;

	/**
	 * Its burst hurts everything hostile this close (a bomb), cm, instead of only what it hits; 0: only what it hits. Each is
	 * thrown back from the middle of the burst.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0"))
	float BurstRadius = 0.f;

	/** Bursts by itself after this long, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.1"))
	float Lifetime = 4.f;

	/** The picture is drawn flying right and turns with its travel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bFaceTravel = true;

	/** A 2D light travelling with it (intensity 0 = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Light")
	FLinearColor LightColor = FLinearColor(1.f, 0.55f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Light", meta = (ClampMin = "0"))
	float LightIntensity = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Light", meta = (ClampMin = "1"))
	float LightRadius = 260.f;

	/** Played where it bursts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Effects")
	TObjectPtr<UPaperFlipbook> ImpactEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Effects")
	TObjectPtr<USoundBase> LaunchSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Effects")
	TObjectPtr<USoundBase> ImpactSound;
};

/**
 * Onyx Projectile: flies from whoever fired it (falling as much as its Gravity says, turning with its travel), hurts only
 * those hostile to them, and bursts on what it hits (walls, floors, platforms, its target) or when its time is up; with a
 * Stick Time it sticks in walls, floors and platforms instead (an arrow), and fades after a while. Fire one with Fire.
 */
UCLASS(meta = (DisplayName = "Onyx Projectile"))
class ONYX2DRUNTIME_API AOnyxProjectile : public AActor
{
	GENERATED_BODY()

public:
	AOnyxProjectile();

	/** Fires one from From towards Direction (only X and Z count) for Shooter. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Combat", meta = (WorldContext = "WorldContext"))
	static AOnyxProjectile* Fire(UObject* WorldContext, const FOnyxProjectileSpec& Spec, FVector From, FVector Direction, AActor* Shooter);

	/** Bursts where it is (effect, sound) and goes away. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Combat")
	void Burst();

	/** Stuck in a wall, a floor or a platform (an arrow that landed). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Combat")
	bool IsStuck() const { return bStuck; }

	/** Whoever fired it (null once they are gone). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Combat")
	AActor* GetShooter() const { return Shooter.Get(); }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UPaperFlipbookComponent> Look;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UOnyxLight2DComponent> Glow;

	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);

	void Launch(const FOnyxProjectileSpec& InSpec, const FVector& Direction, AActor* InShooter);
	void FaceTravel();
	/** Stops where it struck, its point sunk into the surface. */
	void Stick(const FHitResult& Hit);

	FOnyxProjectileSpec Spec;
	FVector Velocity = FVector::ZeroVector;
	TWeakObjectPtr<AActor> Shooter;
	float Age = 0.f;
	bool bBurst = false;
	bool bStuck = false;
	float StuckFor = 0.f;
	float GlowWhenFlying = 0.f;
	/** Fired in a Top Down level: it flies straight across the ground (no gravity). */
	bool bTopDown = false;
};
