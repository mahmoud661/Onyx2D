// Copyright Onyx2D. All Rights Reserved.

#include "Core/OnyxSeeThroughSubsystem.h"
#include "Character/OnyxCharacter.h"
#include "Components/MeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Core/Onyx2DSettings.h"
#include "Core/Onyx2DTypes.h"
#include "Core/OnyxDrawOrderSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Movement/OnyxMovementComponent.h"

namespace
{
	const FName NoSeeThroughTag(TEXT("Onyx.NoSeeThrough"));
	/** Seconds to fade fully in or out. */
	constexpr float SeeThroughFadeTime = 0.15f;
	/** How much of the player's body a prop must cover to fade. */
	constexpr float SeeThroughCover = 0.2f;
}

bool UOnyxSeeThroughSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UOnyxSeeThroughSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOnyxSeeThroughSubsystem, STATGROUP_Tickables);
}

float UOnyxSeeThroughSubsystem::GetSeeThrough(const AActor* Actor) const
{
	const float* Amount = Faded.Find(Actor);
	return Amount ? *Amount : 0.f;
}

void UOnyxSeeThroughSubsystem::GatherProps()
{
	Props.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->ActorHasTag(NoSeeThroughTag) || Actor->IsA<APawn>() || UOnyxDrawOrderSubsystem::FindLayer(Actor) != EOnyxDrawLayer::Props)
		{
			continue;
		}
		TInlineComponentArray<UMeshComponent*> Meshes(Actor);
		if (Meshes.Num() > 0)
		{
			Props.Add(Actor);
		}
	}
}

void UOnyxSeeThroughSubsystem::Apply(AActor* Actor, float Amount)
{
	TInlineComponentArray<UMeshComponent*> Meshes(Actor);
	for (UMeshComponent* Mesh : Meshes)
	{
		Mesh->SetCustomPrimitiveDataFloat(Onyx2D::SeeThroughDataIndex, Amount);
	}
}

void UOnyxSeeThroughSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	const UOnyx2DSettings* Settings = UOnyx2DSettings::Get();
	const bool bOn = World && Settings && Settings->bSeeThroughProps && UOnyx2DLibrary::IsTopDown(World);
	// The bodies to keep in sight: each player's, from its feet up (its standing capsule's size).
	TArray<FBox2D, TInlineAllocator<2>> Bodies;
	TArray<float, TInlineAllocator<2>> BodyFeet;
	if (bOn)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const AOnyxCharacter* Player = It->IsValid() ? Cast<AOnyxCharacter>((*It)->GetPawn()) : nullptr;
			const UOnyxMovementComponent* Movement = Player ? Player->GetOnyxMovement() : nullptr;
			if (!Movement)
			{
				continue;
			}
			const FVector2D Standing = Movement->GetStandingCapsule();
			const float Feet = UOnyxDrawOrderSubsystem::FindFeetZ(Player);
			const float X = Player->GetActorLocation().X;
			Bodies.Add(FBox2D(FVector2D(X - Standing.X, Feet), FVector2D(X + Standing.X, Feet + Standing.Y * 2.f)));
			BodyFeet.Add(Feet);
		}
		SinceGather += DeltaTime;
		if (SinceGather > 1.f || Props.IsEmpty())
		{
			SinceGather = 0.f;
			GatherProps();
		}
	}

	const float Amount = Settings ? FMath::Clamp(Settings->SeeThroughAmount, 0.f, 1.f) : 0.f;
	const float Step = DeltaTime / SeeThroughFadeTime;
	TSet<AActor*> Hiding;
	for (const TWeakObjectPtr<AActor>& Weak : Props)
	{
		AActor* Prop = Weak.Get();
		if (!Prop || Bodies.IsEmpty())
		{
			continue;
		}
		const FBox Bounds = Prop->GetComponentsBoundingBox(true);
		const FBox2D Picture(FVector2D(Bounds.Min.X, Bounds.Min.Z), FVector2D(Bounds.Max.X, Bounds.Max.Z));
		const float PropFeet = UOnyxDrawOrderSubsystem::FindFeetZ(Prop);
		for (int32 Index = 0; Index < Bodies.Num(); ++Index)
		{
			// In front (standing lower on the screen) and over enough of the body.
			if (PropFeet >= BodyFeet[Index] - 1.f || !Picture.Intersect(Bodies[Index]))
			{
				continue;
			}
			const FBox2D Overlap(FVector2D::Max(Picture.Min, Bodies[Index].Min), FVector2D::Min(Picture.Max, Bodies[Index].Max));
			const float BodyArea = FMath::Max(Bodies[Index].GetArea(), 1.f);
			if (Overlap.GetArea() >= BodyArea * SeeThroughCover)
			{
				Hiding.Add(Prop);
				break;
			}
		}
	}
	for (AActor* Prop : Hiding)
	{
		if (!Faded.Contains(Prop))
		{
			Faded.Add(Prop, 0.f);
		}
	}
	for (auto It = Faded.CreateIterator(); It; ++It)
	{
		AActor* Prop = It.Key().Get();
		if (!Prop)
		{
			It.RemoveCurrent();
			continue;
		}
		const float Target = Hiding.Contains(Prop) ? Amount : 0.f;
		const float Next = FMath::Abs(Target - It.Value()) <= Step * FMath::Max(Amount, 0.01f) ? Target : It.Value() + FMath::Sign(Target - It.Value()) * Step * FMath::Max(Amount, 0.01f);
		if (Next != It.Value())
		{
			It.Value() = Next;
			Apply(Prop, Next);
		}
		if (Next <= 0.f && Target <= 0.f)
		{
			It.RemoveCurrent();
		}
	}
}
