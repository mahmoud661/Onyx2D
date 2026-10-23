// Copyright Onyx2D. All Rights Reserved.

#include "Feedback/OnyxFeedbackSubsystem.h"
#include "Core/Onyx2DTypes.h"
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
	if (!HitStopTicker.IsValid())
	{
		// Remembered once per freeze (an overlapping hit would read the freeze's own speed).
		HitStopRestoreDilation = UGameplayStatics::GetGlobalTimeDilation(World);
		HitStopTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UOnyxFeedbackSubsystem::TickHitStop));
	}
	UGameplayStatics::SetGlobalTimeDilation(World, FMath::Clamp(TimeDilation, 0.001f, 1.f) * HitStopRestoreDilation);
}

bool UOnyxFeedbackSubsystem::TickHitStop(float DeltaTime)
{
	if (FPlatformTime::Seconds() < HitStopEndTime)
	{
		return true;
	}
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, HitStopRestoreDilation);
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
	// The sprite materials (Onyx lit sprite) turn the flash colour, lit or not, while the amount is up.
	auto SetFlash = [](AActor* Target, const FLinearColor& FlashColor, float Amount)
	{
		TArray<UPrimitiveComponent*> Primitives;
		Target->GetComponents(Primitives);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive->IsA<UPaperFlipbookComponent>() || Primitive->IsA<UPaperSpriteComponent>())
			{
				Primitive->SetCustomPrimitiveDataVector4(Onyx2D::FlashColorDataIndex, FVector4(FlashColor.R, FlashColor.G, FlashColor.B, 1.f));
				Primitive->SetCustomPrimitiveDataFloat(Onyx2D::FlashDataIndex, Amount);
			}
		}
	};
	SetFlash(Actor, Color, 1.f);
	TWeakObjectPtr<AActor> WeakActor(Actor);
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [WeakActor, Color, SetFlash]()
	{
		if (AActor* Target = WeakActor.Get())
		{
			SetFlash(Target, Color, 0.f);
		}
	}), Duration, false);
}
