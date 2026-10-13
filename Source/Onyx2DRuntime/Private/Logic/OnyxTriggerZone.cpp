// Copyright Onyx2D. All Rights Reserved.

#include "Logic/OnyxTriggerZone.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/OnyxScoreSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Logic/OnyxAction.h"
#include "TimerManager.h"

namespace
{
	/** Depth of the box across the 2D plane, so everything on the plane overlaps it. */
	constexpr float ZoneDepth = 100.f;
	/** Seconds between two runs of the Otherwise actions, so walking in and out doesn't repeat "Locked" every step. */
	constexpr double OtherwiseRepeat = 1.0;

	FString WhenText(EOnyxTriggerWhen When)
	{
		switch (When)
		{
		case EOnyxTriggerWhen::PlayerEnters: return TEXT("Player enters");
		case EOnyxTriggerWhen::PlayerLeaves: return TEXT("Player leaves");
		case EOnyxTriggerWhen::PlayerInteracts: return TEXT("Player presses Interact");
		case EOnyxTriggerWhen::AnythingEnters: return TEXT("Anything enters");
		case EOnyxTriggerWhen::LevelStarts: return TEXT("Level starts");
		default: return TEXT("Only when fired");
		}
	}
}

AOnyxTriggerZone::AOnyxTriggerZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	Area->InitBoxExtent(FVector(100.f, ZoneDepth, 100.f));
	Area->SetCollisionObjectType(ECC_WorldDynamic);
	Area->SetCollisionResponseToAllChannels(ECR_Ignore);
	Area->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Area->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	Area->SetCanEverAffectNavigation(false);
	Area->SetLineThickness(2.f);
	Area->SetHiddenInGame(true);
	RootComponent = Area;

#if WITH_EDITORONLY_DATA
	Label = CreateEditorOnlyDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	if (Label)
	{
		Label->SetupAttachment(Area);
		Label->SetHiddenInGame(true);
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextTop);
		Label->SetWorldSize(14.f);
		// Faces the 2D camera, which looks along -Y.
		Label->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
		Label->bIsEditorOnly = true;
	}
#endif
}

void AOnyxTriggerZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Area->SetBoxExtent(FVector(Size.X * 0.5f, ZoneDepth, Size.Y * 0.5f));
	Area->ShapeColor = EditorColor.ToFColor(true);
	Area->MarkRenderStateDirty();
#if WITH_EDITORONLY_DATA
	if (Label)
	{
		TArray<FString> Lines = { WhenText(When) + TEXT(":") };
		int32 Step = 1;
		for (const UOnyxAction* Action : Actions)
		{
			if (Action)
			{
				Lines.Add(FString::Printf(TEXT("%d. %s"), Step++, *Action->Describe()));
			}
		}
		if (Step == 1)
		{
			Lines.Add(TEXT("(add Actions in Details)"));
		}
		if (Needs == EOnyxTriggerNeeds::Item)
		{
			Lines.Add(FString::Printf(TEXT("needs %d %s"), NeededAmount, *NeededItem.ToString()));
		}
		else if (Needs == EOnyxTriggerNeeds::Coins)
		{
			Lines.Add(FString::Printf(TEXT("needs %d coins"), NeededAmount));
		}
		Label->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
		Label->SetTextRenderColor(EditorColor.ToFColor(true));
		Label->SetRelativeLocation(FVector(0.f, ZoneDepth + 1.f, Size.Y * 0.5f - 8.f));
	}
#endif
}

void AOnyxTriggerZone::BeginPlay()
{
	Super::BeginPlay();
	bOn = bStartOn;
	Area->OnComponentBeginOverlap.AddUniqueDynamic(this, &AOnyxTriggerZone::HandleBeginOverlap);
	Area->OnComponentEndOverlap.AddUniqueDynamic(this, &AOnyxTriggerZone::HandleEndOverlap);
	if (When == EOnyxTriggerWhen::LevelStarts)
	{
		// Next frame: by then the player has spawned, for actions that need them.
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { Fire(nullptr); }));
	}
}

bool AOnyxTriggerZone::IsPlayer(const AActor* Actor) const
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn && Pawn->IsPlayerControlled();
}

void AOnyxTriggerZone::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Only an actor's main body counts, not its hitboxes or talk range.
	if (!OtherActor || OtherComp != OtherActor->GetRootComponent())
	{
		return;
	}
	if ((When == EOnyxTriggerWhen::PlayerEnters && IsPlayer(OtherActor)) || When == EOnyxTriggerWhen::AnythingEnters)
	{
		Fire(OtherActor);
	}
}

void AOnyxTriggerZone::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor && OtherComp == OtherActor->GetRootComponent() && When == EOnyxTriggerWhen::PlayerLeaves && IsPlayer(OtherActor))
	{
		Fire(OtherActor);
	}
}

bool AOnyxTriggerZone::CanFireNow() const
{
	if (!bOn || IsUsedUp())
	{
		return false;
	}
	const UWorld* World = GetWorld();
	return bOnlyOnce || !World || World->GetTimeSeconds() - LastFireTime >= Cooldown;
}

bool AOnyxTriggerZone::HasWhatItNeeds() const
{
	const UOnyxScoreSubsystem* Score = UOnyxScoreSubsystem::Get(this);
	switch (Needs)
	{
	case EOnyxTriggerNeeds::Item: return Score && Score->HasItem(NeededItem, NeededAmount);
	case EOnyxTriggerNeeds::Coins: return Score && Score->GetCoins() >= NeededAmount;
	default: return true;
	}
}

bool AOnyxTriggerZone::Fire(AActor* Activator)
{
	if (!CanFireNow())
	{
		return false;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	FOnyxActionContext Context;
	Context.Instigator = Activator;
	Context.Source = this;

	if (!HasWhatItNeeds())
	{
		if (Now - LastDeniedTime >= OtherwiseRepeat)
		{
			LastDeniedTime = Now;
			UOnyxAction::RunAll(OtherwiseActions, Context);
		}
		return false;
	}
	if (bUseThemUp)
	{
		if (UOnyxScoreSubsystem* Score = UOnyxScoreSubsystem::Get(this))
		{
			if (Needs == EOnyxTriggerNeeds::Item)
			{
				Score->UseItem(NeededItem, NeededAmount);
			}
			else if (Needs == EOnyxTriggerNeeds::Coins)
			{
				Score->SpendCoins(NeededAmount);
			}
		}
	}
	bFired = true;
	LastFireTime = Now;
	UOnyxAction::RunAll(Actions, Context);
	OnTriggered.Broadcast(Activator);
	return true;
}

void AOnyxTriggerZone::SetZoneOn(bool bNewOn)
{
	bOn = bNewOn;
}

FString AOnyxTriggerZone::Describe() const
{
	return FString::Printf(TEXT("%s: %s"), *WhenText(When), *UOnyxAction::DescribeAll(Actions));
}

void AOnyxTriggerZone::Interact_Implementation(AActor* User)
{
	if (When == EOnyxTriggerWhen::PlayerInteracts)
	{
		Fire(User);
	}
}

bool AOnyxTriggerZone::IsInteractionLocked_Implementation(AActor* User) const
{
	// Only zones set off by Interact show a prompt; a missing key still lets the player try (and hear "Locked").
	return When != EOnyxTriggerWhen::PlayerInteracts || !CanFireNow();
}

FText AOnyxTriggerZone::GetInteractionPrompt_Implementation() const
{
	return InteractPrompt;
}
