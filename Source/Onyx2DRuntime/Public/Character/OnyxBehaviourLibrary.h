// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OnyxBehaviourLibrary.generated.h"

class UActorComponent;

/** Ready-made behaviours a character can have (each one is a component). */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EOnyxBehaviour : uint8
{
	None = 0 UMETA(Hidden),
	/** The player drives it (Player Control component). */
	PlayerControl = 1 << 0,
	/** A camera follows it (Onyx Camera component). */
	Camera = 1 << 1,
	/** Comes back after dying, at the last checkpoint (Respawn component). */
	Respawn = 1 << 2,
	/** Walks, hops or flies back and forth (Patrol component). */
	Patrol = 1 << 3,
	/** Hurts whoever touches it (Contact Damage component). */
	ContactDamage = 1 << 4,
	/** Jumping on its head defeats it (Stompable component). */
	Stompable = 1 << 5,
	/** The player can talk to it with Interact (Talk component). */
	Talk = 1 << 6,
};
ENUM_CLASS_FLAGS(EOnyxBehaviour);

/** Adds behaviours to characters: the same components the editor's New Character dialog puts in a Blueprint. */
UCLASS()
class ONYX2DRUNTIME_API UOnyxBehaviourLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Adds the components for Behaviours (a mask of EOnyxBehaviour) that Actor doesn't have yet. Returns the new ones. */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|Behaviours")
	static TArray<UActorComponent*> AddBehaviours(AActor* Actor, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Onyx2DRuntime.EOnyxBehaviour")) int32 Behaviours);

	/** The component class of one behaviour. */
	static UClass* GetComponentClass(EOnyxBehaviour Behaviour);

	/** Every behaviour, in display order. */
	static const TArray<EOnyxBehaviour>& GetAll();

	/** Behaviours of a character the player drives. */
	static constexpr EOnyxBehaviour PlayerPreset = EOnyxBehaviour::PlayerControl | EOnyxBehaviour::Camera | EOnyxBehaviour::Respawn;

	/** Behaviours of a classic platformer foe. */
	static constexpr EOnyxBehaviour PatrolFoePreset = EOnyxBehaviour::Patrol | EOnyxBehaviour::ContactDamage | EOnyxBehaviour::Stompable;
};
