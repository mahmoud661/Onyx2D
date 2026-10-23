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
 * Plays a character's Moveset on its sprite: picks the movement clips by itself (Idle, Walk, Run, Jump, Fall, Dash, Roll,
 * Slide, WallSlide, Crouch, CrouchWalk, LedgeHang, LedgeClimb) from the Onyx movement component, plays one-shots on top
 * (attacks, hurt, death) and fires the playing clip's events and windows. Between movement clips it plays transitions
 * when the Moveset has them: Land after a hard landing, AirJump for a jump in the air, WallGrab on catching a wall.
 * Override Resolve Locomotion Clip in Blueprint for your own rules.
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

	/** Landing at least this fast plays the Land clip (when the Moveset has one) until the character moves, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Moveset", meta = (ClampMin = "0"))
	float LandClipMinSpeed = 650.f;

	/** Flipbook to drive. Defaults to the first flipbook component on the owner. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Moveset")
	void SetTargetFlipbook(UPaperFlipbookComponent* InTarget) { Target = InTarget; }

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Moveset")
	UPaperFlipbookComponent* GetTargetFlipbook() const { return Target; }

	/**
	 * Plays each role as drawn with this ending, when the Moveset has it (Suffix _Legs: Run as Run_Legs, while something else
	 * draws the upper body over the legs); None: as named. The clip playing now changes at once, at the same place in its cycle.
	 */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Moveset")
	void SetClipVariant(FName Suffix);

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Moveset")
	FName GetClipVariant() const { return ClipVariant; }

	/** The playing clip is the role drawn with the Clip Variant's ending (Run_Legs), not the role itself. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Moveset")
	bool IsPlayingVariant() const { return bPlayingVariant; }

	/** Plays its looping clips backwards (a walk played backwards backs away: aiming one way, going the other). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Moveset")
	void SetPlayBackwards(bool bBackwards);

	/** Plays the clip of a role, looping or holding as the clip says (ignored while a one-shot plays). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Moveset")
	bool PlayClip(FName Role);

	/** Plays the clip of a role once, on top of locomotion. Returns its length in seconds (0 if missing). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Moveset")
	float PlayOneShot(FName Role);

	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Moveset")
	void StopOneShot();

	/** Plays the clip of a role once between movement clips, for as long as the movement state stays one of States. */
	bool PlayTransition(FName Role, std::initializer_list<EOnyxLocomotion> States);

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

	/**
	 * Top down: the direction the character looks (set by the character). Its clips are played as drawn for it (Walk_Up,
	 * Walk_Side...), and a looping clip turns with it, keeping its place in the cycle. None: plain roles (side view).
	 */
	void SetDirection(EOnyxDirection InDirection);
	EOnyxDirection GetDirection() const { return Direction; }

	/** The direction the playing clip was drawn in (None: a plain role). */
	EOnyxDirection GetShownDirection() const { return ShownDirection; }

	/** Its Moveset has clips drawn diagonally, so a top-down character looks in eight directions (else four). */
	bool HasDiagonalClips() const;

	/** Another clip started (a role, or the same role drawn in another direction). */
	FSimpleMulticastDelegate OnClipApplied;

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
	UFUNCTION()
	void HandleAirJumped();

	/**
	 * Side view, swimming some way: the swim drawn nearest the way it goes (Swim_Up90, Swim_Up45, Swim for along, Swim_Down45,
	 * Swim_Down90; any of Up/Down 15 to 90 a Moveset has), held a little past halfway to the next so it doesn't flick between
	 * two. None when it has no such drawings or is hardly moving.
	 */
	FName ResolveSwimDirection(const FVector& Velocity) const;
	/** Swim or one of its drawings the way it goes, and the way (degrees up); false for anything else. */
	static bool SwimDirectionOf(FName Role, float& OutDegrees);
	/** Plays Role on from the place in the cycle the playing clip is at (turning while swimming keeps the stroke going). */
	bool SwapKeepingPlace(FName Role);

	/** The movement state changed: a transition to play between the two? */
	void StartTransition(EOnyxLocomotion From, EOnyxLocomotion To, const class UOnyxMovementComponent& Movement);

	/** The clip of Role (the Clip Variant's drawing of it first); bOutVariant: it is the variant. */
	UOnyxClip* FindPlayableClip(FName Role, bool* bOutVariant = nullptr) const;
	bool ApplyClip(FName Role, bool bForceRestart);
	void FireEvents(float DeltaTime);
	/** Closes every window still open (the clip changed or the component stops). */
	void CloseOpenWindows();

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbookComponent> Target;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxClip> CurrentClip;

	FName CurrentRole;
	FName ClipVariant;
	bool bPlayingVariant = false;
	bool bBackwards = false;
	/** The clip's speed, backwards when asked (looping clips only: a one-time clip would stop at once). */
	float PlayRateOf(const UOnyxClip& Clip) const;
	EOnyxDirection Direction = EOnyxDirection::None;
	EOnyxDirection ShownDirection = EOnyxDirection::None;
	float LastEventTime = -1.f;
	TArray<TWeakObjectPtr<const UOnyxClipWindow>> OpenWindows;
	float FacingSign = 1.f;
	float OneShotRemaining = 0.f;
	bool bOneShot = false;

	EOnyxLocomotion LastState = EOnyxLocomotion::Idle;
	bool bHasLastState = false;
	bool bTransition = false;
	float TransitionRemaining = 0.f;
	/** The movement states the transition plays through (one bit each). */
	uint32 TransitionStates = 0;
};
