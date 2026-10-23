// Copyright Onyx2D. All Rights Reserved.

#include "Combat/OnyxHitboxComponent.h"
#include "PaperFlipbook.h"
#include "Sound/SoundBase.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "World/OnyxFlipbookEffect.h"
#include "Combat/OnyxCombatLibrary.h"
#include "Core/Onyx2DLibrary.h"
#include "Movement/OnyxMovementComponent.h"
#include "Feedback/OnyxFeedbackSubsystem.h"

UOnyxHitboxComponent::UOnyxHitboxComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	InitBoxExtent(FVector(40.f, 40.f, 40.f));
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SetGenerateOverlapEvents(true);
	SetCanEverAffectNavigation(false);
	ShapeColor = FColor(255, 80, 60);
}

void UOnyxHitboxComponent::BeginPlay()
{
	Super::BeginPlay();
	OnComponentBeginOverlap.AddUniqueDynamic(this, &UOnyxHitboxComponent::HandleOverlap);
	if (bContinuous)
	{
		BeginHitWindow();
	}
}

void UOnyxHitboxComponent::BeginHitWindow()
{
	HitThisWindow.Reset();
	bWindowOpen = true;
	RehitTimer = RehitInterval;
	SetComponentTickEnabled(bContinuous);
	HitOverlappingActors();
}

void UOnyxHitboxComponent::EndHitWindow()
{
	bWindowOpen = false;
	HitThisWindow.Reset();
	SetComponentTickEnabled(false);
}

void UOnyxHitboxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	RehitTimer -= DeltaTime;
	if (bWindowOpen && bContinuous && RehitTimer <= 0.f)
	{
		RehitTimer = RehitInterval;
		HitThisWindow.Reset();
		HitOverlappingActors();
	}
}

void UOnyxHitboxComponent::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryHit(OtherActor);
}

void UOnyxHitboxComponent::HitOverlappingActors()
{
	TArray<AActor*> Overlapping;
	GetOverlappingActors(Overlapping);
	for (AActor* Actor : Overlapping)
	{
		TryHit(Actor);
	}
}

void UOnyxHitboxComponent::TryHit(AActor* Victim)
{
	AActor* Attacker = GetOwner();
	if (!bWindowOpen || !Victim || Victim == Attacker || HitThisWindow.Contains(Victim) || !UOnyxCombatLibrary::AreHostile(Attacker, Victim))
	{
		return;
	}

	if (ShouldHit && !ShouldHit(Victim))
	{
		return; // not recorded: it may hit later in this window
	}

	FOnyxDamageInfo Info;
	Info.Amount = Damage;
	Info.Instigator = Attacker;
	const UOnyxMovementComponent* Moving = Attacker ? Attacker->FindComponentByClass<UOnyxMovementComponent>() : nullptr;
	if (Moving ? Moving->IsTopDown() : UOnyx2DLibrary::IsTopDown(this))
	{
		// From above: thrown straight away from whoever struck, across the ground (no upward part).
		const FVector From = Attacker ? Attacker->GetActorLocation() : GetComponentLocation();
		const FVector Apart = (Victim->GetActorLocation() - From) * FVector(1.f, 0.f, 1.f);
		Info.Knockback = (Apart.IsNearlyZero() ? GetForwardVector() : Apart.GetSafeNormal()) * Knockback.X;
	}
	else
	{
		const float AwaySign = (Victim->GetActorLocation().X >= GetComponentLocation().X) ? 1.f : -1.f;
		Info.Knockback = FVector(AwaySign * Knockback.X, 0.f, Knockback.Y);
	}
	Info.DamageType = DamageType;

	HitThisWindow.Add(Victim);
	if (!UOnyxCombatLibrary::ApplyDamage(Victim, Info))
	{
		return;
	}

	// Where the blow lands: the point of the victim's body nearest the weapon (at the weapon's height, not its feet).
	const USceneComponent* Body = Victim->GetRootComponent();
	const FVector Impact = Body ? Body->Bounds.GetBox().GetClosestPointTo(GetComponentLocation()) : (GetComponentLocation() + Victim->GetActorLocation()) * 0.5f;
	UOnyxAudioSubsystem::PlaySoundAt(this, HitSound, Impact, EOnyxSoundCategory::Effects, 1.f, FMath::FRandRange(0.95f, 1.05f));
	if (HitEffect)
	{
		AOnyxFlipbookEffect::Spawn(this, HitEffect, Impact);
	}
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
	{
		Feedback->HitStop(HitStopDuration);
		Feedback->AddTrauma(HitTrauma);
	}
	OnHit.Broadcast(Victim);
}
