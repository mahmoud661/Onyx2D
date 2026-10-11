// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "OnyxInputConfig.generated.h"

class APlayerController;
class UInputAction;
class UInputMappingContext;

/** Input actions used by Onyx characters. Create one as a Data Asset to rebind controls. */
UCLASS(BlueprintType)
class ONYX2DRUNTIME_API UOnyxInputConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	/** Axis1D: -1 left, +1 right. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Move;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Jump;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Dash;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Attack;

	/** Hold to drop through one-way platforms. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Down;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Interact;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Pause;

	/** Builds a complete keyboard + gamepad scheme in code, so no assets are required. */
	static UOnyxInputConfig* CreateDefault(UObject* Outer);
};

/** Owns the active input config for a local player and keeps its mapping context applied. */
UCLASS()
class ONYX2DRUNTIME_API UOnyxInputSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static UOnyxInputSubsystem* Get(const APlayerController* PlayerController);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Input")
	UOnyxInputConfig* GetInputConfig() const { return ActiveConfig; }

	/** Swaps the active controls (null restores the built-in defaults). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Input")
	void SetInputConfig(UOnyxInputConfig* Config);

	/** Makes sure the active mapping context is registered with Enhanced Input. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Input")
	void ApplyMappingContext();

private:
	UPROPERTY(Transient)
	TObjectPtr<UOnyxInputConfig> DefaultConfig;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxInputConfig> ActiveConfig;
};
