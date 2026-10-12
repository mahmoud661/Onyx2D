// Copyright Onyx2D. All Rights Reserved.

#include "Animation/OnyxMoveset.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxClipEvent.h"

UOnyxClip* UOnyxMoveset::FindClip(FName Role) const
{
	const TObjectPtr<UOnyxClip>* Found = Clips.Find(Role);
	return Found ? Found->Get() : nullptr;
}

FName UOnyxMoveset::FindRole(const UOnyxClip* Clip) const
{
	for (const TPair<FName, TObjectPtr<UOnyxClip>>& Pair : Clips)
	{
		if (Clip && Pair.Value == Clip)
		{
			return Pair.Key;
		}
	}
	return NAME_None;
}

const TArray<FName>& UOnyxMoveset::GetStandardRoles()
{
	static const TArray<FName> Roles = { TEXT("Idle"), TEXT("Run"), TEXT("Jump"), TEXT("Fall"), TEXT("Crouch"), TEXT("Dash"), TEXT("WallSlide"),
		TEXT("Attack1"), TEXT("Attack2"), TEXT("Attack3"), TEXT("Hurt"), TEXT("Death") };
	return Roles;
}

#if WITH_EDITOR

void UOnyxMoveset::SetClip(FName Role, UOnyxClip* Clip)
{
	if (Role.IsNone())
	{
		return;
	}
	Modify();
	Clips.Add(Role, Clip);
	if (Clip && Clip->Moveset.Get() != this)
	{
		Clip->Modify();
		Clip->Moveset = this;
	}
}

void UOnyxMoveset::RemoveRole(FName Role)
{
	if (Clips.Contains(Role))
	{
		Modify();
		Clips.Remove(Role);
	}
}

bool UOnyxMoveset::RenameRole(FName OldRole, FName NewRole)
{
	if (NewRole.IsNone() || OldRole == NewRole || Clips.Contains(NewRole) || !Clips.Contains(OldRole))
	{
		return false;
	}
	Modify();
	TObjectPtr<UOnyxClip> Clip = Clips.FindAndRemoveChecked(OldRole);
	Clips.Add(NewRole, Clip);
	return true;
}

bool UOnyxMoveset::AddMessage(FName Message)
{
	if (Message.IsNone() || Messages.Contains(Message))
	{
		return false;
	}
	Modify();
	Messages.Add(Message);
	return true;
}

bool UOnyxMoveset::RenameMessage(FName OldName, FName NewName)
{
	const int32 Index = Messages.IndexOfByKey(OldName);
	if (Index == INDEX_NONE || NewName.IsNone() || Messages.Contains(NewName))
	{
		return false;
	}
	Modify();
	Messages[Index] = NewName;
	for (const TPair<FName, TObjectPtr<UOnyxClip>>& Pair : Clips)
	{
		if (!Pair.Value)
		{
			continue;
		}
		for (UOnyxClipEvent* Event : Pair.Value->Events)
		{
			if (UOnyxClipEvent_Message* Sender = Cast<UOnyxClipEvent_Message>(Event); Sender && Sender->Message == OldName)
			{
				Sender->Modify();
				Sender->Message = NewName;
			}
		}
	}
	return true;
}

void UOnyxMoveset::RemoveMessage(FName Message)
{
	if (!Messages.Contains(Message))
	{
		return;
	}
	Modify();
	Messages.Remove(Message);
	for (const TPair<FName, TObjectPtr<UOnyxClip>>& Pair : Clips)
	{
		UOnyxClip* Clip = Pair.Value;
		if (!Clip)
		{
			continue;
		}
		for (int32 Index = Clip->Events.Num() - 1; Index >= 0; --Index)
		{
			if (const UOnyxClipEvent_Message* Sender = Cast<UOnyxClipEvent_Message>(Clip->Events[Index]); Sender && Sender->Message == Message)
			{
				Clip->Modify();
				Clip->Events.RemoveAt(Index);
			}
		}
	}
}

FName UOnyxMoveset::MakeMessageName(FName Base) const
{
	if (Base.IsNone())
	{
		Base = TEXT("Message");
	}
	if (!Messages.Contains(Base))
	{
		return Base;
	}
	for (int32 Number = 1;; ++Number)
	{
		const FName Candidate(*FString::Printf(TEXT("%s%d"), *Base.ToString(), Number));
		if (!Messages.Contains(Candidate))
		{
			return Candidate;
		}
	}
}

#endif
