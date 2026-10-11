// Copyright Onyx2D. All Rights Reserved.

#include "Core/OnyxGameMode.h"
#include "Character/OnyxPlayerCharacter.h"
#include "Core/Onyx2DSettings.h"
#include "Core/OnyxPlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "World/OnyxLevelBuilder.h"

AOnyxGameMode::AOnyxGameMode()
{
	DefaultPawnClass = AOnyxPlayerCharacter::StaticClass();
	PlayerControllerClass = AOnyxPlayerController::StaticClass();
}

AActor* AOnyxGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	for (TActorIterator<AOnyxLevelBuilder> It(GetWorld()); It; ++It)
	{
		FVector Location;
		if (It->GetPlayerStart(Location))
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Location, FRotator::ZeroRotator, Params);
		}
	}
	return Super::FindPlayerStart_Implementation(Player, IncomingName);
}

UClass* AOnyxGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// A game mode Blueprint that picks its own pawn (one level, one hero) wins over the project setting.
	if (DefaultPawnClass && DefaultPawnClass != AOnyxPlayerCharacter::StaticClass())
	{
		return DefaultPawnClass;
	}
	if (UClass* Configured = UOnyx2DSettings::Get()->DefaultPlayerClass.TryLoadClass<APawn>())
	{
		return Configured;
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}
