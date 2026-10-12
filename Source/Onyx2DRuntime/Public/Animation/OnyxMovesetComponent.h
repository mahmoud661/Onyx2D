// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Onyx2DTypes.h"
#include "OnyxMovesetComponent.generated.h"

class UOnyxClip;
class UOnyxClipWindow;
class UOnyxMoveset;
class UPaperFlipbookComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxClipMessageSignature, FName, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxClipFinishedSignature, FName, Role);

/**
 * Plays a character's Moveset on its sprite: picks the movement clips by itself (Idle, Run, Jump, Fall, Dash, WallSlide,
 * Crouch) from the Onyx movement component, plays one-shots on top (attacks, hurt, death) and fires the playing clip's
 * events and windows. Override Resolve Locomotion Clip in Blueprint for your own rules.
 */
UCLASS(ClassGroup = (Onyx2D), meta = (BlueprintSpawnableComponent, DisplayName = "Onyx Moveset"))
class ONYX2DRUNTIME_API UOnyxMovesetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnyxMovesetComponent();

	/** The clips this character plays, by role. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Moveset")
	TObjectPtr<UOnyxMoveset> Moveset;

	/** Pick Idle / Run / Jump... automatically each frame from the movement state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Moveset")
	bool bAutoLocomotion = true;

	/** Flipbook to drive. Defaults to the first flipbook component on the owner. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Moveset")
	void SetTargetFlipbook(UPaperFlipbookComponent* InTarget) { Target = InTarget; }

	/** Plays the clip of a role, looping or holding as the clip says (ignored while a one-shot plays). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Moveset")
	bool PlayClip(FName Role);

	/** Plays the clip of a role once, on top of locomotion. Returns its length in seconds (0 if missing). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Moveset")
	float PlayOneShot(FName Role);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Moveset")
	void StopOneShot();

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Moveset")
	bool HasClip(FName Role) const;

	/** Length in seconds including speed, or 0. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Moveset")
	float GetClipLength(FName Role) const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Moveset")
	FName GetCurrentRole() const { return CurrentRole; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Moveset")
	UOnyxClip* GetCurrentClip() const;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Moveset")
	bool IsPlayingOneShot() const { return bOneShot; }

	/** +1 facing right, -1 facing left (set by the character; events mirror their offsets with it). */
	void SetFacingSign(float Sign) { FacingSign = Sign < 0.f ? -1.f : 1.f; }
	float GetFacingSign() const { return FacingSign; }

	/** Movement state -> role. Default: the state's name with sensible fallbacks (Jump -> Fall -> Idle). */
	UFUNCTION(BlueprintNativeEvent, Category = "Onyx2D|Moveset")
	FName ResolveLocomotionClip(EOnyxLocomotion State) const;

	/** A Message event of the playing clip fired. */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Moveset")
	FOnyxClipMessageSignature OnClipMessage;

	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Moveset")
	FOnyxClipFinishedSignature OnOneShotFinished;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UOnyxClip* FindPlayableClip(FName Role) const;
	bool ApplyClip(FName Role, bool bForceRestart);
	void FireEvents(float DeltaTime);
	/** Closes every window still open (the clip changed or the component stops). */
	void CloseOpenWindows();

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbookComponent> Target;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxClip> CurrentClip;

	FName CurrentRole;
	float LastEventTime = -1.f;
	TArray<TWeakObjectPtr<const UOnyxClipWindow>> OpenWindows;
	float FacingSign = 1.f;
	float OneShotRemaining = 0.f;
	bool bOneShot = false;
};
