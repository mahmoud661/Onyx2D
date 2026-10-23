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

	/**
	 * The HUD in levels that use this game mode: a Widget Blueprint made from Onyx HUD Widget. Empty = the project's HUD
	 * (Project Settings > Onyx2D UI > HUD Class).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onyx2D", meta = (MetaClass = "/Script/Onyx2DUI.OnyxHUDWidget"))
	FSoftClassPath HUDWidgetClass;

	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
};
