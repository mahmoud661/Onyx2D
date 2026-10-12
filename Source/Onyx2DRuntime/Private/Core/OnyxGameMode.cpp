// Copyright Onyx2D. All Rights Reserved.

#include "Core/OnyxGameMode.h"
#include "Character/OnyxBehaviourLibrary.h"
#include "Character/OnyxCharacter.h"
#include "Character/OnyxPlayerControlComponent.h"
#include "Combat/OnyxCombatTypes.h"
#include "Core/Onyx2DSettings.h"
#include "Core/OnyxPlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "World/OnyxLevelBuilder.h"

AOnyxGameMode::AOnyxGameMode()
{
	DefaultPawnClass = AOnyxCharacter::StaticClass();
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
	if (DefaultPawnClass && DefaultPawnClass != AOnyxCharacter::StaticClass())
	{
		return DefaultPawnClass;
	}
	if (UClass* Configured = UOnyx2DSettings::Get()->DefaultPlayerClass.TryLoadClass<APawn>())
	{
		return Configured;
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

APawn* AOnyxGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	APawn* Pawn = Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
	// No player character set up yet: the plain stand-in gets the player behaviours so the level is playable at once.
	if (AOnyxCharacter* Character = Cast<AOnyxCharacter>(Pawn); Character && Character->GetClass() == AOnyxCharacter::StaticClass()
		&& !Character->FindComponentByClass<UOnyxPlayerControlComponent>())
	{
		Character->TeamId = OnyxTeams::Player;
		Character->PlaceholderColor = FLinearColor(0.2f, 0.85f, 0.9f);
		UOnyxBehaviourLibrary::AddBehaviours(Character, int32(UOnyxBehaviourLibrary::PlayerPreset));
	}
	return Pawn;
}
