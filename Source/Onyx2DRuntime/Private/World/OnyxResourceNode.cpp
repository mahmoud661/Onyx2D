// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxResourceNode.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Components/BoxComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "EngineUtils.h"
#include "Items/OnyxInventoryComponent.h"
#include "Logic/OnyxDialogueSubsystem.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "World/OnyxFlipbookEffect.h"

AOnyxResourceNode::AOnyxResourceNode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	// A tile's footprint that blocks walking (the ground is X across, Z up the screen).
	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	Blocker->InitBoxExtent(FVector(26.f, 200.f, 22.f));
	Blocker->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Blocker->SetCanEverAffectNavigation(false);
	// Its outline only when selected: the level stays readable with hundreds of them about.
	Blocker->bDrawOnlyIfSelected = true;
	RootComponent = Blocker;

	Sprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Blocker);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AOnyxResourceNode::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Sprite->SetSprite(Look);
	if (Look)
	{
		UOnyx2DLibrary::ConfigureSpriteLighting(Sprite, false);
	}
}

void AOnyxResourceNode::BeginPlay()
{
	Super::BeginPlay();
	SpriteRest = Sprite->GetRelativeLocation();
	if (UOnyxSaveSubsystem* Save = UOnyxSaveSubsystem::Get(this))
	{
		Save->Register(this);
	}
}

void AOnyxResourceNode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UOnyxSaveSubsystem* Save = UOnyxSaveSubsystem::Get(this))
	{
		Save->Unregister(this);
	}
	Super::EndPlay(EndPlayReason);
}

AOnyxResourceNode* AOnyxResourceNode::FindAt(const UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return nullptr;
	}
	AOnyxResourceNode* Nearest = nullptr;
	float Best = 40.f; // within a little over half a tile of where the tool lands
	for (TActorIterator<AOnyxResourceNode> It(World); It; ++It)
	{
		if (It->bCleared)
		{
			continue;
		}
		const float Distance = FVector::Dist2D(FVector(It->GetActorLocation().X, It->GetActorLocation().Z, 0.f), FVector(Location.X, Location.Z, 0.f));
		if (Distance < Best)
		{
			Best = Distance;
			Nearest = *It;
		}
	}
	return Nearest;
}

bool AOnyxResourceNode::TakeHit(EOnyxToolKind WithTool, UOnyxInventoryComponent* Into)
{
	if (bCleared || WithTool != Tool)
	{
		return false;
	}
	if (HitSound)
	{
		UOnyxAudioSubsystem::PlaySoundAt(this, HitSound, GetActorLocation());
	}
	if (++HitsTaken < Hits)
	{
		Shake = 1.f;
		SetActorTickEnabled(true);
		return true;
	}
	if (Into && Drop && DropCount > 0)
	{
		Into->AddItem(Drop, DropCount);
		if (UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(this))
		{
			Dialogue->ShowMessage(FText::Format(INVTEXT("+{0} {1}"), FText::AsNumber(DropCount), Drop->DisplayName), 1.2f);
		}
	}
	Clear(false);
	return true;
}

void AOnyxResourceNode::Clear(bool bQuietly)
{
	bCleared = true;
	if (!bQuietly && ClearEffect)
	{
		AOnyxFlipbookEffect::Spawn(this, ClearEffect, GetActorLocation() + FVector(0.f, 2.f, 8.f));
	}
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}

void AOnyxResourceNode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Knocked side to side, settling.
	Shake = FMath::Max(Shake - DeltaSeconds * 5.f, 0.f);
	Sprite->SetRelativeLocation(SpriteRest + FVector(FMath::Sin(Shake * 30.f) * Shake * 4.f, 0.f, 0.f));
	if (Shake <= 0.f)
	{
		SetActorTickEnabled(false);
	}
}

void AOnyxResourceNode::WriteSave(FArchive& Archive)
{
	Archive << bCleared;
}

void AOnyxResourceNode::ReadSave(FArchive& Archive)
{
	bool bWasCleared = false;
	Archive << bWasCleared;
	if (bWasCleared)
	{
		Clear(true);
	}
}
