// Copyright Onyx2D. All Rights Reserved.

#include "Core/OnyxLevelData.h"
#include "Core/Onyx2DSettings.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

const UOnyxLevelData* UOnyxLevelData::Find(const UWorld* World)
{
	const AWorldSettings* Settings = World ? World->GetWorldSettings(false, false) : nullptr;
	return Settings ? const_cast<AWorldSettings*>(Settings)->GetAssetUserData<UOnyxLevelData>() : nullptr;
}

EOnyxGameStyle UOnyxLevelData::GetGameStyle(const UWorld* World)
{
	if (const UOnyxLevelData* Level = Find(World))
	{
		return Level->GameStyle;
	}
	return UOnyx2DSettings::Get()->DefaultGameStyle;
}
