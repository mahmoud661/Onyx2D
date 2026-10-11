// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Onyx2DTypes.h"
#include "OnyxAnimatorComponent.generated.h"

class UPaperFlipbook;
class UPaperFlipbookComponent;

USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxAnimation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UPaperFlipbook> Flipbook = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.01"))
	float PlayRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bLoop = true;
};

/** Fires EventName when Animation reaches Frame (e.g. "HitboxOn" on the swing frame). */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxFrameEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FName Animation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0"))
	int32 Frame = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FName EventName;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxAnimationEventSignature, FName, EventName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxAnimationFinishedSignature, FName, Animation);

/**
 * Lightweight flipbook state machine: picks locomotion animations automatically from the
 * Onyx movement component and plays one-shots (attacks, hurt, death) on top.
 * Override ResolveLocomotionAnimation in Blueprint for custom rules.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent))
class ONYX2DRUNTIME_API UOnyxAnimatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxAnimatorComponent();

	/** Animation name -> flipbook. Locomotion uses Idle, Run, Jump, Fall, WallSlide and Dash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Animation")
	TMap<FName, FOnyxAnimation> Animations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Animation")
	TArray<FOnyxFrameEvent> FrameEvents;

	/** Pick Idle/Run/Jump/... automatically each frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Animation")
	bool bAutoLocomotion = true;

	/** Flipbook to drive. Defaults to the first flipbook component on the owner. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Animation")
	void SetTargetFlipbook(UPaperFlipbookComponent* InTarget) { Target = InTarget; }

	/** Plays a looping/state animation (ignored while a one-shot is playing). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Animation")
	bool PlayAnimation(FName Name);

	/** Plays an animation once, on top of locomotion. Returns its length in seconds (0 if missing). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Animation")
	float PlayOneShot(FName Name);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Animation")
	void StopOneShot();

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Animation")
	bool HasAnimation(FName Name) const;

	/** Length in seconds including play rate, or 0. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Animation")
	float GetAnimationLength(FName Name) const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Animation")
	FName GetCurrentAnimation() const { return CurrentName; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Animation")
	bool IsPlayingOneShot() const { return bOneShot; }

	/** Locomotion state -> animation name. Default: the enum name with sensible fallbacks. */
	UFUNCTION(BlueprintNativeEvent, Category = "Onyx2D|Animation")
	FName ResolveLocomotionAnimation(EOnyxLocomotion State) const;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Animation")
	FOnyxAnimationEventSignature OnFrameEvent;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Animation")
	FOnyxAnimationFinishedSignature OnOneShotFinished;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool ApplyAnimation(FName Name, bool bForceRestart);
	void FireFrameEvents();

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbookComponent> Target;

	FName CurrentName;
	int32 LastFrame = INDEX_NONE;
	float OneShotRemaining = 0.f;
	bool bOneShot = false;
};
