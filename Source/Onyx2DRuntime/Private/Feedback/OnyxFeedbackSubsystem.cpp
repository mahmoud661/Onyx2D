// Copyright Onyx2D. All Rights Reserved.

#include "Feedback/OnyxFeedbackSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "PaperFlipbookComponent.h"
#include "PaperSpriteComponent.h"
#include "TimerManager.h"

UOnyxFeedbackSubsystem* UOnyxFeedbackSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UOnyxFeedbackSubsystem>() : nullptr;
}

void UOnyxFeedbackSubsystem::Deinitialize()
{
	if (HitStopTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HitStopTicker);
		HitStopTicker.Reset();
	}
	Super::Deinitialize();
}

void UOnyxFeedbackSubsystem::HitStop(float Duration, float TimeDilation)
{
	UWorld* World = GetWorld();
	if (!World || Duration <= 0.f || World->IsPaused())
	{
		return;
	}

	HitStopEndTime = FMath::Max(HitStopEndTime, FPlatformTime::Seconds() + Duration);
	UGameplayStatics::SetGlobalTimeDilation(World, FMath::Clamp(TimeDilation, 0.001f, 1.f));

	if (!HitStopTicker.IsValid())
	{
		HitStopTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UOnyxFeedbackSubsystem::TickHitStop));
	}
}

bool UOnyxFeedbackSubsystem::TickHitStop(float DeltaTime)
{
	if (FPlatformTime::Seconds() < HitStopEndTime)
	{
		return true;
	}
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
	}
	HitStopTicker.Reset();
	return false;
}

void UOnyxFeedbackSubsystem::AddTrauma(float Amount)
{
	OnTrauma.Broadcast(FMath::Clamp(Amount, 0.f, 1.f));
}

void UOnyxFeedbackSubsystem::FlashActor(AActor* Actor, FLinearColor Color, float Duration)
{
	if (!Actor || !GetWorld())
	{
		return;
	}

	TArray<UPaperFlipbookComponent*> Flipbooks;
	Actor->GetComponents(Flipbooks);
	TArray<UPaperSpriteComponent*> Sprites;
	Actor->GetComponents(Sprites);

	for (UPaperFlipbookComponent* Flipbook : Flipbooks)
	{
		Flipbook->SetSpriteColor(Color);
	}
	for (UPaperSpriteComponent* Sprite : Sprites)
	{
		Sprite->SetSpriteColor(Color);
	}

	TWeakObjectPtr<AActor> WeakActor(Actor);
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [WeakActor]()
	{
		if (AActor* Target = WeakActor.Get())
		{
			TArray<UPaperFlipbookComponent*> RestoreFlipbooks;
			Target->GetComponents(RestoreFlipbooks);
			for (UPaperFlipbookComponent* Flipbook : RestoreFlipbooks)
			{
				Flipbook->SetSpriteColor(FLinearColor::White);
			}
			TArray<UPaperSpriteComponent*> RestoreSprites;
			Target->GetComponents(RestoreSprites);
			for (UPaperSpriteComponent* Sprite : RestoreSprites)
			{
				Sprite->SetSpriteColor(FLinearColor::White);
			}
		}
	}), Duration, false);
}
