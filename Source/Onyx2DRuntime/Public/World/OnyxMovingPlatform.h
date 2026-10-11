// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxMovingPlatform.generated.h"

class UStaticMeshComponent;

/** Platform that travels between waypoints (drag them in the viewport). Characters ride it automatically. */
UCLASS(meta = (DisplayName = "Onyx Moving Platform"))
class ONYX2DRUNTIME_API AOnyxMovingPlatform : public AActor
{
	GENERATED_BODY()

public:
	AOnyxMovingPlatform();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UStaticMeshComponent> Platform;

	/** Points relative to the actor; the start position is always the first stop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (MakeEditWidget = true))
	TArray<FVector> Waypoints = { FVector(400.f, 0.f, 0.f) };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (ClampMin = "1"))
	float Speed = 200.f;

	/** Seconds to wait at each stop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (ClampMin = "0"))
	float PauseTime = 0.6f;

	/** Go back and forth instead of looping back to the start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	bool bPingPong = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	FVector2D Size = FVector2D(240.f, 30.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	FLinearColor Color = FLinearColor(0.45f, 0.5f, 0.58f);

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	TArray<FVector> Stops;
	int32 TargetIndex = 1;
	int32 Step = 1;
	float WaitRemaining = 0.f;
};
