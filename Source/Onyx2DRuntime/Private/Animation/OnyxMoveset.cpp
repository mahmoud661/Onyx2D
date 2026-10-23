// Copyright Onyx2D. All Rights Reserved.

#include "Animation/OnyxMoveset.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxClipEvent.h"

UOnyxClip* UOnyxMoveset::FindClip(FName Role) const
{
	const TObjectPtr<UOnyxClip>* Found = Clips.Find(Role);
	return Found ? Found->Get() : nullptr;
}

FName UOnyxMoveset::MakeDirectionalRole(FName Role, EOnyxDirection Direction)
{
	const TCHAR* Suffix = nullptr;
	switch (Direction)
	{
	case EOnyxDirection::Down:     Suffix = TEXT("Down"); break;
	case EOnyxDirection::DownSide: Suffix = TEXT("DownSide"); break;
	case EOnyxDirection::Side:     Suffix = TEXT("Side"); break;
	case EOnyxDirection::UpSide:   Suffix = TEXT("UpSide"); break;
	case EOnyxDirection::Up:       Suffix = TEXT("Up"); break;
	default:                       return Role;
	}
	return FName(*FString::Printf(TEXT("%s_%s"), *Role.ToString(), Suffix));
}

FName UOnyxMoveset::GetBaseRole(FName Role, EOnyxDirection* OutDirection)
{
	const FString Name = Role.ToString();
	// Longest first: DownSide before Side.
	const TPair<const TCHAR*, EOnyxDirection> Suffixes[] = { { TEXT("_DownSide"), EOnyxDirection::DownSide }, { TEXT("_UpSide"), EOnyxDirection::UpSide },
		{ TEXT("_Down"), EOnyxDirection::Down }, { TEXT("_Side"), EOnyxDirection::Side }, { TEXT("_Up"), EOnyxDirection::Up } };
	for (const TPair<const TCHAR*, EOnyxDirection>& Suffix : Suffixes)
	{
		if (Name.EndsWith(Suffix.Key) && Name.Len() > FCString::Strlen(Suffix.Key))
		{
			if (OutDirection)
			{
				*OutDirection = Suffix.Value;
			}
			return FName(*Name.LeftChop(FCString::Strlen(Suffix.Key)));
		}
	}
	if (OutDirection)
	{
		*OutDirection = EOnyxDirection::None;
	}
	return Role;
}

UOnyxClip* UOnyxMoveset::FindClip(FName Role, EOnyxDirection Direction, EOnyxDirection* OutShown) const
{
	// The direction itself, then the nearest drawn: a diagonal shows the side, then its up or down; up and down show
	// the plain role (drawn from the side) before the side.
	EOnyxDirection Order[4] = { Direction, EOnyxDirection::None, EOnyxDirection::None, EOnyxDirection::None };
	int32 Count = 1;
	switch (Direction)
	{
	case EOnyxDirection::DownSide: Order[1] = EOnyxDirection::Side; Order[2] = EOnyxDirection::Down; Count = 4; break;
	case EOnyxDirection::UpSide:   Order[1] = EOnyxDirection::Side; Order[2] = EOnyxDirection::Up; Count = 4; break;
	case EOnyxDirection::Down:
	case EOnyxDirection::Up:       Order[2] = EOnyxDirection::Side; Count = 3; break;
	case EOnyxDirection::Side:     Count = 2; break;
	default:                       break;
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (UOnyxClip* Clip = FindClip(MakeDirectionalRole(Role, Order[Index])))
		{
			if (OutShown)
			{
				*OutShown = Order[Index];
			}
			return Clip;
		}
	}
	return nullptr;
}

bool UOnyxMoveset::HasDiagonals() const
{
	for (const TPair<FName, TObjectPtr<UOnyxClip>>& Pair : Clips)
	{
		const FString Role = Pair.Key.ToString();
		if (Role.EndsWith(TEXT("_DownSide")) || Role.EndsWith(TEXT("_UpSide")))
		{
			return true;
		}
	}
	return false;
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
	static const TArray<FName> Roles = { TEXT("Idle"), TEXT("Run"), TEXT("Jump"), TEXT("Fall"), TEXT("Crouch"), TEXT("Dash"), TEXT("WallSlide"), TEXT("Swim"),
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
