// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxLightSwitch.h"
#include "Sound/SoundBase.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Lighting/OnyxLight2D.h"
#include "Lighting/OnyxLight2DComponent.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "UObject/ConstructorHelpers.h"

AOnyxLightSwitch::AOnyxLightSwitch()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->InitBoxExtent(FVector(40.f, 100.f, 60.f));
	Trigger->SetCollisionProfileName(UCollisionProfile::DefaultProjectile_ProfileName);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = Trigger;

	Sprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Trigger);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sprite->SetMobility(EComponentMobility::Movable);

	Placeholder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Placeholder"));
	Placeholder->SetupAttachment(Trigger);
	Placeholder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Placeholder->SetMobility(EComponentMobility::Movable);
	Placeholder->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.6f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Placeholder->SetStaticMesh(Cube.Object);
}

void AOnyxLightSwitch::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	bIsOn = bStartOn;
	ApplyLook();
}

void AOnyxLightSwitch::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AOnyxLightSwitch::HandleOverlap);
	SetOn(bStartOn, true);
}

void AOnyxLightSwitch::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (bSwitchOnTouch && Pawn && Pawn->IsPlayerControlled())
	{
		SetOn(!bIsOn);
	}
}

void AOnyxLightSwitch::Interact_Implementation(AActor* User)
{
	SetOn(!bIsOn);
}

FText AOnyxLightSwitch::GetInteractionPrompt_Implementation() const
{
	return bIsOn ? INVTEXT("Turn off") : INVTEXT("Turn on");
}

void AOnyxLightSwitch::SetOn(bool bOn, bool bInstant)
{
	const bool bChanged = bOn != bIsOn;
	bIsOn = bOn;
	const float Fade = bInstant ? 0.f : FadeTime;
	for (AActor* Target : Lights)
	{
		if (!Target)
		{
			continue;
		}
		// Onyx Light 2D actors switch their halo and sound too; other actors just their light components.
		if (AOnyxLight2D* Light = Cast<AOnyxLight2D>(Target))
		{
			Light->SetLightEnabled(bOn, Fade);
			continue;
		}
		TArray<UOnyxLight2DComponent*> Components;
		Target->GetComponents(Components);
		for (UOnyxLight2DComponent* Component : Components)
		{
			Component->SetLightEnabled(bOn, Fade);
		}
	}
	ApplyLook();
	if (bChanged && !bInstant)
	{
		UOnyxAudioSubsystem::PlaySoundAt(this, SwitchSound, GetActorLocation());
		OnSwitched.Broadcast(bOn);
	}
}

void AOnyxLightSwitch::ApplyLook()
{
	UPaperSprite* On = OnSprite.Get();
	UPaperSprite* Off = OffSprite.Get();
	UPaperSprite* Look = bIsOn ? On : Off;
	Sprite->SetSprite(Look ? Look : (On ? On : Off)); // one sprite for both states is fine
	const bool bHasArt = On || Off;
	Sprite->SetVisibility(bHasArt);
	Placeholder->SetVisibility(!bHasArt);
	if (!bHasArt)
	{
		// A lever leaning left when off, right when on.
		Placeholder->SetRelativeRotation(FRotator(bIsOn ? -25.f : 25.f, 0.f, 0.f));
		UOnyx2DLibrary::ApplySolidColor(Placeholder, bIsOn ? FLinearColor(1.f, 0.82f, 0.3f) : FLinearColor(0.35f, 0.35f, 0.4f));
	}
}
