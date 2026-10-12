// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OnyxMoveset.generated.h"

class UOnyxClip;

/**
 * Everything a character can play: its Clips by role (Idle, Run, Jump, Attack1, Hurt...) and the Messages its clips can
 * send. The Moveset component plays Idle, Run, Jump, Fall, Dash, WallSlide, Crouch and Swim by itself; Hurt, Death and attacks
 * are played by the character; any other role by name from Blueprint (Play Clip). Several characters can share one.
 * Open it in the Moveset Editor.
 */
UCLASS(BlueprintType)
class ONYX2DRUNTIME_API UOnyxMoveset : public UObject
{
	GENERATED_BODY()

public:
	/** Role -> clip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moveset")
	TMap<FName, TObjectPtr<UOnyxClip>> Clips;

	/** Named gameplay events this character's clips can send (Message events). The actor runs the function of that name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moveset")
	TArray<FName> Messages;

	UFUNCTION(BlueprintPure, Category = "Onyx2D|Moveset")
	UOnyxClip* FindClip(FName Role) const;

	/** The role of Clip in this moveset (None if it isn't in it). */
	UFUNCTION(BlueprintPure, Category = "Onyx2D|Moveset")
	FName FindRole(const UOnyxClip* Clip) const;

	/** The roles the Moveset component and character use, in a sensible order. */
	static const TArray<FName>& GetStandardRoles();

#if WITH_EDITOR
	/** Puts Clip in Role (replacing what was there) and makes the clip point back here. Call inside a transaction. */
	void SetClip(FName Role, UOnyxClip* Clip);

	/** Takes a role out of the moveset (the clip asset stays). */
	void RemoveRole(FName Role);

	/** Renames a role; false if NewRole is taken. */
	bool RenameRole(FName OldRole, FName NewRole);

	bool AddMessage(FName Message);

	/** Renames a message here and in every Message event of this moveset's clips. */
	bool RenameMessage(FName OldName, FName NewName);

	/** Removes a message and every Message event that sends it. */
	void RemoveMessage(FName Message);

	/** A message name not used yet ("Message", "Message1"...). */
	FName MakeMessageName(FName Base) const;
#endif
};
