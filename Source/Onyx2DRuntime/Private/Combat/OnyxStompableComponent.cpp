// Copyright Onyx2D. All Rights Reserved.

#include "Combat/OnyxStompableComponent.h"
#include "Combat/OnyxContactDamageComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Feedback/OnyxFeedbackSubsystem.h"
#include "GameFramework/Character.h"

UOnyxStompableComponent::UOnyxStompableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOnyxStompableComponent::BeginPlay()
{
	Super::BeginPlay();
	// A stomp replaces the contact hit.
	if (UOnyxContactDamageComponent* Contact = GetOwner()->FindComponentByClass<UOnyxContactDamageComponent>())
	{
		TWeakObjectPtr<UOnyxStompableComponent> WeakThis(this);
		Contact->ShouldHit = [WeakThis](AActor* Victim) { return !(WeakThis.IsValid() && WeakThis->TryStomp(Victim)); };
	}
}

bool UOnyxStompableComponent::TryStomp(AActor* StomperActor)
{
	ACharacter* Stomper = Cast<ACharacter>(StomperActor);
	UOnyxHealthComponent* Health = GetOwner()->FindComponentByClass<UOnyxHealthComponent>();
	if (!Stomper || !Health || Health->IsDead() || Stomper->GetVelocity().Z > 50.f)
	{
		return false;
	}
	// The stomper's feet must be above our middle: landing on the head, not walking into the body.
	const float Feet = Stomper->GetActorLocation().Z - Stomper->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (Feet < GetOwner()->GetActorLocation().Z)
	{
		return false;
	}
	Health->Kill(Stomper);
	Stomper->LaunchCharacter(FVector(0.f, 0.f, Bounce), false, true);
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
	{
		Feedback->HitStop(0.06f);
		Feedback->AddTrauma(0.25f);
	}
	return true;
}
