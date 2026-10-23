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

	/**
	 * Laid over the Mapping Context while the player plays a Top Down level: keys that mean something else from above
	 * (W and the Up arrow walk up instead of jumping). Optional.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> TopDownMappingContext;

	/** Axis1D: -1 left, +1 right. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Move;

	/** Axis1D: -1 down, +1 up (the stick): steers swimming. On keys, Jump steers up and Down steers down. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Vertical;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Jump;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Dash;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Attack;

	/** Hold to aim up: up-slashes, climbing onto ledges (W / Up when the Onyx2D settings' Up Keys Jump is off). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Up;

	/** Hold to mend wounds with gathered energy (Onyx Energy). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Mend;

	/** Hold to drop through one-way platforms. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Down;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Interact;

	/** Hold to draw and aim, let go to loose (Onyx Ranged's Shoot: a bow). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Shoot;

	/** Onyx Ranged's Throw: a bomb, a stone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Throw;

	/** Axis2D: where Onyx Ranged aims (the right stick; the mouse aims by itself). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Aim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Pause;

	/** The hotbar (Onyx Inventory): the next and previous slot (mouse wheel, Q / R, gamepad right trigger). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> HotbarNext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> HotbarPrevious;

	/** Axis1D: a hotbar slot by number (keys 1-9 and 0 give 1-10). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> HotbarSlot;

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

	/** Lays the Top Down keys over the controls (the player's character plays a Top Down level), or takes them off. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Input")
	void SetTopDown(bool bTopDown);

private:
	bool bTopDownKeys = false;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxInputConfig> DefaultConfig;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxInputConfig> ActiveConfig;
};
