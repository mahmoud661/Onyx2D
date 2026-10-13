// Copyright Onyx2D. All Rights Reserved.

// Console commands for testing levels.

#include "Camera/OnyxCameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** The actor called Name: its label in the editor, else its object name. */
	AActor* FindActorNamed(UWorld* World, const FString& Name)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
#if WITH_EDITOR
			if (It->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
#endif
			if (It->GetName().Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GoToCommand(TEXT("Onyx2D.GoTo"),
		TEXT("Puts the player at an actor (by label or name), to test that part of the level: Onyx2D.GoTo <Actor> [OffsetX in cm]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APawn* Player = World && !Args.IsEmpty() ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
			AActor* Target = Player ? FindActorNamed(World, Args[0]) : nullptr;
			if (!Target)
			{
				UE_LOG(LogTemp, Warning, TEXT("Onyx2D.GoTo: no player, or no actor called %s."), Args.IsEmpty() ? TEXT("(nothing)") : *Args[0]);
				return;
			}
			const float OffsetX = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;
			const FVector At = Target->GetActorLocation() + FVector(OffsetX, 0.f, 0.f);
			Player->SetActorLocation(FVector(At.X, Player->GetActorLocation().Y, At.Z), false, nullptr, ETeleportType::TeleportPhysics);
			if (UMovementComponent* Movement = Player->GetMovementComponent())
			{
				Movement->StopMovementImmediately();
			}
			if (UOnyxCameraComponent* Camera = Player->FindComponentByClass<UOnyxCameraComponent>())
			{
				Camera->SnapToTarget();
			}
		}));
}
