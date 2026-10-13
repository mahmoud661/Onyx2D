// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxSign.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Logic/OnyxAction.h"
#include "Logic/OnyxDialogueSubsystem.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "UObject/ConstructorHelpers.h"

AOnyxSign::AOnyxSign()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->InitBoxExtent(FVector(50.f, 100.f, 60.f));
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetCanEverAffectNavigation(false);
	RootComponent = Trigger;

	Sprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Trigger);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// A wooden board standing on the actor's origin, like the art (bottom pivot): changing the look never lifts the sign.
	Placeholder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Placeholder"));
	Placeholder->SetupAttachment(Trigger);
	Placeholder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Placeholder->SetCastShadow(false);
	Placeholder->SetRelativeLocation(FVector(0.f, 0.f, 18.f));
	Placeholder->SetRelativeScale3D(FVector(0.6f, 0.05f, 0.36f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Placeholder->SetStaticMesh(Cube.Object);

	Pages.Add(INVTEXT("Write what the sign says in Details > Sign > Pages."));
}

void AOnyxSign::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Sprite->SetSprite(Look);
	Sprite->SetVisibility(Look != nullptr);
	Placeholder->SetVisibility(Look == nullptr);
	if (Look)
	{
		UOnyx2DLibrary::ConfigureSpriteLighting(Sprite, false);
	}
	else
	{
		UOnyx2DLibrary::ApplySolidColor(Placeholder, FLinearColor(0.55f, 0.36f, 0.2f));
	}
}

void AOnyxSign::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AOnyxSign::HandleOverlap);
}

void AOnyxSign::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!bReadByWalkingPast || !Pawn || !Pawn->IsPlayerControlled() || OtherComp != OtherActor->GetRootComponent() || Pages.Num() == 0)
	{
		return;
	}
	if (UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(this))
	{
		Dialogue->ShowMessage(Pages[0], 3.f);
	}
	FinishReading(OtherActor);
}

void AOnyxSign::Read(AActor* Reader)
{
	UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(this);
	if (!Dialogue)
	{
		return;
	}
	TArray<FOnyxDialogueLine> Lines;
	for (const FText& Page : Pages)
	{
		Lines.AddDefaulted_GetRef().Text = Page;
	}
	TWeakObjectPtr<AOnyxSign> WeakThis(this);
	TWeakObjectPtr<AActor> WeakReader(Reader);
	Dialogue->QueueDialogue(Lines, 60.f, [WeakThis, WeakReader]()
	{
		if (AOnyxSign* Sign = WeakThis.Get())
		{
			Sign->FinishReading(WeakReader.Get());
		}
	});
}

void AOnyxSign::FinishReading(AActor* Reader)
{
	if (bRead)
	{
		return;
	}
	bRead = true;
	FOnyxActionContext Context;
	Context.Instigator = Reader;
	Context.Source = this;
	UOnyxAction::RunAll(AfterReading, Context);
}

void AOnyxSign::Interact_Implementation(AActor* User)
{
	Read(User);
}

bool AOnyxSign::IsInteractionLocked_Implementation(AActor* User) const
{
	return bReadByWalkingPast || Pages.Num() == 0;
}

FText AOnyxSign::GetInteractionPrompt_Implementation() const
{
	return Prompt;
}
