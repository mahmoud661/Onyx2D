// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OnyxGameMode.generated.h"

/** Default Onyx2D game mode: player character + controller; spawns at a level builder's 'P' tile. */
UCLASS()
class ONYX2DRUNTIME_API AOnyxGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOnyxGameMode();

	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
};
