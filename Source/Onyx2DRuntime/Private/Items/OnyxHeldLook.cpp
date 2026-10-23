// Copyright Onyx2D. All Rights Reserved.

#include "Items/OnyxHeldLook.h"
#include "Animation/OnyxMoveset.h"

const FOnyxHeldFrames* UOnyxHeldLook::Find(FName Role, EOnyxDirection Direction) const
{
	if (Direction != EOnyxDirection::None)
	{
		if (const FOnyxHeldFrames* Frames = Clips.Find(UOnyxMoveset::MakeDirectionalRole(Role, Direction)))
		{
			return Frames;
		}
	}
	return Clips.Find(Role);
}
