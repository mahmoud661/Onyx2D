// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnyxOneWayPlatform.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** Platform you can jump up through and drop down from (Down + Jump, or Down). */
UCLASS(meta = (DisplayName = "Onyx One-Way Platform"))
class ONYX2DRUNTIME_API AOnyxOneWayPlatform : public AActor
{
	GENERATED_BODY()

public:
	AOnyxOneWayPlatform();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Collision;

	/** Placeholder visual; swap the mesh or hide it and add your own sprite. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Width (X) and thickness (Z) in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	FVector2D Size = FVector2D(300.f, 24.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D", meta = (ClampMin = "1"))
	float Depth = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D")
	FLinearColor Color = FLinearColor(0.32f, 0.4f, 0.48f);

	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	float GetTopZ() const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D")
	float GetHalfWidth() const { return Size.X * 0.5f * GetActorScale3D().X; }

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
