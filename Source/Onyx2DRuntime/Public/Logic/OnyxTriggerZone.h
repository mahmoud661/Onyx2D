// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/OnyxInteractable.h"
#include "OnyxTriggerZone.generated.h"

class UBoxComponent;
class UOnyxAction;
class UTextRenderComponent;

/** What sets a trigger zone off. */
UENUM(BlueprintType)
enum class EOnyxTriggerWhen : uint8
{
	/** The player walks into it. */
	PlayerEnters UMETA(DisplayName = "Player Enters"),
	/** The player walks out of it. */
	PlayerLeaves UMETA(DisplayName = "Player Leaves"),
	/** The player stands in it and presses Interact (a door, a chest, a lever). */
	PlayerInteracts UMETA(DisplayName = "Player Presses Interact"),
	/** Anything moving walks or falls into it: the player, a foe, a pushed crate (pressure plates). */
	AnythingEnters UMETA(DisplayName = "Anything Enters"),
	/** As soon as the level starts. */
	LevelStarts UMETA(DisplayName = "Level Starts"),
	/** Only when another zone's Control Zones action or a Blueprint fires it. */
	WhenFired UMETA(DisplayName = "Only When Fired"),
};

/** What the player must have for the zone to work. */
UENUM(BlueprintType)
enum class EOnyxTriggerNeeds : uint8
{
	Nothing,
	/** A named item, like a key (from a pickup or a Give action). */
	Item,
	/** A number of coins. */
	Coins,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnyxTriggerSignature, AActor*, Activator);

/**
 * An invisible box that makes things happen, no Blueprint needed: choose When (the player enters, presses Interact,
 * the level starts...) and list the Actions (show a message, open a door, give a key, teleport...). It can need an item
 * first, like a key, and do Otherwise Actions when the player doesn't have it.
 *
 * Drag one out in the Level Painter (Objects > Trigger Zone) or place it from the Place Actors panel.
 */
UCLASS(meta = (DisplayName = "Onyx Trigger Zone"))
class ONYX2DRUNTIME_API AOnyxTriggerZone : public AActor, public IOnyxInteractable
{
	GENERATED_BODY()

public:
	AOnyxTriggerZone();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onyx2D")
	TObjectPtr<UBoxComponent> Area;

	/** Width and height of the zone, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Zone", meta = (ClampMin = "10"))
	FVector2D Size = FVector2D(200.f, 200.f);

	/** What sets it off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Zone")
	EOnyxTriggerWhen When = EOnyxTriggerWhen::PlayerEnters;

	/** What happens, top to bottom. Click + and pick an action. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Onyx2D|Zone")
	TArray<TObjectPtr<UOnyxAction>> Actions;

	/** Works only once, then never again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Zone")
	bool bOnlyOnce = true;

	/** Seconds before it can go off again (when it works more than once). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Zone", meta = (ClampMin = "0", Units = "s", EditCondition = "!bOnlyOnce"))
	float Cooldown = 1.f;

	/** Starts working when the level starts. Turn it off to have another zone turn it on later. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Zone")
	bool bStartOn = true;

	/** The word shown on the Interact prompt ("Open", "Search", "Pull"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Zone", meta = (EditCondition = "When == EOnyxTriggerWhen::PlayerInteracts", EditConditionHides))
	FText InteractPrompt = INVTEXT("Open");

	/** Something the player must have first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Needs")
	EOnyxTriggerNeeds Needs = EOnyxTriggerNeeds::Nothing;

	/** The item's name, as the pickup or Give action names it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Needs", meta = (EditCondition = "Needs == EOnyxTriggerNeeds::Item", EditConditionHides))
	FName NeededItem = TEXT("Key");

	/** How many of the item, or how many coins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Needs", meta = (ClampMin = "1", EditCondition = "Needs != EOnyxTriggerNeeds::Nothing", EditConditionHides))
	int32 NeededAmount = 1;

	/** Take them away when the zone goes off (a key opens one door; coins pay for something). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Needs", meta = (EditCondition = "Needs != EOnyxTriggerNeeds::Nothing", EditConditionHides))
	bool bUseThemUp = true;

	/** What happens when the player doesn't have them (a "Locked" message, a sound). */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Onyx2D|Needs", meta = (EditCondition = "Needs != EOnyxTriggerNeeds::Nothing", EditConditionHides))
	TArray<TObjectPtr<UOnyxAction>> OtherwiseActions;

	/** Colour of the box in the editor (it is invisible in the game). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onyx2D|Look")
	FLinearColor EditorColor = FLinearColor(0.35f, 0.9f, 0.55f);

	/** Ran its actions (after any needs were met). */
	UPROPERTY(BlueprintAssignable, Category = "Onyx2D|Zone")
	FOnyxTriggerSignature OnTriggered;

	/**
	 * Sets the zone off as if When happened: checks its needs, then runs the actions for Activator (who set it off; can
	 * be empty). Returns true if they ran.
	 */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Zone")
	bool Fire(AActor* Activator);

	/** Turns the zone on or off. A zone that is off does nothing. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Zone")
	void SetZoneOn(bool bOn);

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Zone")
	bool IsZoneOn() const { return bOn; }

	/** True once a zone that works only once has gone off. */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Zone")
	bool IsUsedUp() const { return bOnlyOnce && bFired; }

	/** The player has what the zone needs (always true when it needs nothing). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Zone")
	bool HasWhatItNeeds() const;

	/** "Player enters: Show Message, Open Level". */
	FString Describe() const;

	// IOnyxInteractable
	virtual void Interact_Implementation(AActor* User) override;
	virtual bool IsInteractionLocked_Implementation(AActor* User) const override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	bool IsPlayer(const AActor* Actor) const;
	bool CanFireNow() const;

#if WITH_EDITORONLY_DATA
	/** The zone's summary, floating over it in the editor. */
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Label;
#endif

	bool bOn = true;
	bool bFired = false;
	double LastFireTime = -1.0e9;
	double LastDeniedTime = -1.0e9;
};
