// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OnyxPatrolComponent.generated.h"

/** Walks back and forth, turning at walls and ledges; optionally chases the player when close. */
/** How an enemy gets around. */
UENUM(BlueprintType)
enum class EOnyxPatrolStyle : uint8
{
	/** Walks back and forth, turns at walls and ledges, chases the player. */
	Walk,
	/** Waits, then hops (frogs, slimes); turns at walls. */
	Hop,
	/** Hovers in the air around where it was placed (birds, bats, ghosts). */
	Fly
};

UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent))
class ONYX2DRUNTIME_API UOnyxPatrolComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxPatrolComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Patrol")
	EOnyxPatrolStyle Style = EOnyxPatrolStyle::Walk;

	/** +1 starts walking right, -1 left. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Patrol")
	float Direction = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Patrol")
	bool bTurnAtLedges = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Patrol", meta = (ClampMin = "0"))
	float PauseAtTurn = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Patrol")
	bool bChasePlayer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Patrol", meta = (ClampMin = "0", EditCondition = "bChasePlayer"))
	float ChaseRange = 450.f;

	/** Hop: seconds on the ground between hops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Patrol|Hop", meta = (ClampMin = "0.1", EditCondition = "Style == EOnyxPatrolStyle::Hop", EditConditionHides))
	float HopInterval = 2.f;

	/** Hop: launch velocity (X forwards, Z up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Patrol|Hop", meta = (EditCondition = "Style == EOnyxPatrolStyle::Hop", EditConditionHides))
	FVector2D HopVelocity = FVector2D(300.f, 650.f);

	/** Hop: change direction after every hop (back and forth on the spot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Patrol|Hop", meta = (EditCondition = "Style == EOnyxPatrolStyle::Hop", EditConditionHides))
	bool bAlternateHops = true;

	/** Fly: how far it drifts from its start (X sideways, Z up and down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Patrol|Fly", meta = (EditCondition = "Style == EOnyxPatrolStyle::Fly", EditConditionHides))
	FVector2D FlyRange = FVector2D(0.f, 200.f);

	/** Fly: seconds for one full cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Patrol|Fly", meta = (ClampMin = "0.1", EditCondition = "Style == EOnyxPatrolStyle::Fly", EditConditionHides))
	float FlyPeriod = 4.f;

	/** Fly: always look at the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Patrol|Fly", meta = (EditCondition = "Style == EOnyxPatrolStyle::Fly", EditConditionHides))
	bool bFacePlayer = true;

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Patrol")
	void TurnAround();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool IsBlockedAhead(const class ACharacter* Character) const;
	bool HasGroundAhead(const class ACharacter* Character) const;
	bool TryChase(const class ACharacter* Character);

	void TickWalk(class ACharacter* Character);
	void TickHop(class ACharacter* Character, float DeltaTime);
	void TickFly(class ACharacter* Character, float DeltaTime);

	float PauseRemaining = 0.f;
	float HopTimer = 0.f;
	float FlyTime = 0.f;
	FVector Home = FVector::ZeroVector;
};
