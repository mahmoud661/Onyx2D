// Copyright Onyx2D. All Rights Reserved.

#include "Character/OnyxBehaviourLibrary.h"
#include "AI/OnyxEnemyAIComponent.h"
#include "AI/OnyxPatrolComponent.h"
#include "Camera/OnyxCameraComponent.h"
#include "Character/OnyxPlayerControlComponent.h"
#include "Character/OnyxRespawnComponent.h"
#include "Combat/OnyxContactDamageComponent.h"
#include "Combat/OnyxStompableComponent.h"
#include "Character/OnyxTalkComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

UClass* UOnyxBehaviourLibrary::GetComponentClass(EOnyxBehaviour Behaviour)
{
	switch (Behaviour)
	{
	case EOnyxBehaviour::PlayerControl: return UOnyxPlayerControlComponent::StaticClass();
	case EOnyxBehaviour::Camera: return UOnyxCameraComponent::StaticClass();
	case EOnyxBehaviour::Respawn: return UOnyxRespawnComponent::StaticClass();
	case EOnyxBehaviour::Patrol: return UOnyxPatrolComponent::StaticClass();
	case EOnyxBehaviour::ContactDamage: return UOnyxContactDamageComponent::StaticClass();
	case EOnyxBehaviour::Stompable: return UOnyxStompableComponent::StaticClass();
	case EOnyxBehaviour::Talk: return UOnyxTalkComponent::StaticClass();
	case EOnyxBehaviour::EnemyAI: return UOnyxEnemyAIComponent::StaticClass();
	default: return nullptr;
	}
}

const TArray<EOnyxBehaviour>& UOnyxBehaviourLibrary::GetAll()
{
	static const TArray<EOnyxBehaviour> All = { EOnyxBehaviour::PlayerControl, EOnyxBehaviour::Camera, EOnyxBehaviour::Respawn,
		EOnyxBehaviour::Patrol, EOnyxBehaviour::ContactDamage, EOnyxBehaviour::Stompable, EOnyxBehaviour::EnemyAI, EOnyxBehaviour::Talk };
	return All;
}

TArray<UActorComponent*> UOnyxBehaviourLibrary::AddBehaviours(AActor* Actor, int32 Behaviours)
{
	TArray<UActorComponent*> Added;
	if (!Actor)
	{
		return Added;
	}
	for (const EOnyxBehaviour Behaviour : GetAll())
	{
		UClass* Class = GetComponentClass(Behaviour);
		if (!(Behaviours & int32(Behaviour)) || !Class || Actor->FindComponentByClass(Class))
		{
			continue;
		}
		UActorComponent* Component = NewObject<UActorComponent>(Actor, Class, MakeUniqueObjectName(Actor, Class, Class->GetFName()));
		if (USceneComponent* Scene = Cast<USceneComponent>(Component); Scene && Actor->GetRootComponent())
		{
			Scene->SetupAttachment(Actor->GetRootComponent());
		}
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		Added.Add(Component);
	}
	return Added;
}
