// Copyright Onyx2D. All Rights Reserved.

// Console commands for testing levels.

#include "AI/OnyxBossComponent.h"
#include "Camera/OnyxCameraComponent.h"
#include "World/OnyxBossArena.h"
#include "Character/OnyxCharacter.h"
#include "Character/OnyxPlayerControlComponent.h"
#include "Combat/OnyxCombatComponent.h"
#include "Combat/OnyxCombatTypes.h"
#include "Combat/OnyxEnergyComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "Combat/OnyxRangedComponent.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxMovesetComponent.h"
#include "Movement/OnyxMovementComponent.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Items/OnyxInventoryComponent.h"
#include "Items/OnyxHeldLook.h"
#include "Items/OnyxItem.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Time/OnyxCalendarSubsystem.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Lighting/OnyxLight2DComponent.h"
#include "World/OnyxFog.h"
#include "World/OnyxWater.h"
#include "ProceduralMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Lighting/OnyxLightingSubsystem.h"
#include "Lighting/OnyxPixelLightComponent.h"
#include "Lighting/OnyxPixelLightSet.h"
#include "Core/Onyx2DTypes.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "Components/StaticMeshComponent.h"
#include "World/OnyxParallaxBackground.h"
#include "World/OnyxRain.h"
#include "Lighting/OnyxLightingEnvironment.h"
#include "Lighting/OnyxLightingTypes.h"

namespace
{
	/** The actor called Name: its label in the editor, else its object name. */
	AActor* FindActorNamed(UWorld* World, const FString& Name)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
#if WITH_EDITOR
			if (It->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
#endif
			if (It->GetName().Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs PlayerCommand(TEXT("Onyx2D.Player"),
		TEXT("Logs the player's state, to see what it is doing in a test: where it is, how it moves, the clip it plays, whether it shows, its health."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const AOnyxCharacter* Player = World ? Cast<AOnyxCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
			if (!Player)
			{
				UE_LOG(LogTemp, Display, TEXT("Onyx2D.Player: no Onyx character playing."));
				return;
			}
			const UOnyxMovementComponent* Movement = Player->GetOnyxMovement();
			const UPaperFlipbookComponent* Sprite = Player->GetSprite();
			const TArray<float>& Data = Sprite ? Sprite->GetCustomPrimitiveData().Data : TArray<float>();
			auto DataAt = [&Data](int32 Index) { return Data.IsValidIndex(Index) ? Data[Index] : 0.f; };
			const UOnyxRangedComponent* Ranged = Player->FindComponentByClass<UOnyxRangedComponent>();
			const FName Upper = Ranged ? Ranged->GetUpperClip() : NAME_None;
			const UOnyxClip* Playing = Player->MovesetComponent->GetCurrentClip();
			UE_LOG(LogTemp, Display, TEXT("Onyx2D.Player: at %s, velocity %s, %s, clip %s%s%s (%s), sprite %s at %s, flash %.2f dissolve %.2f order %.3f, health %.1f%s"),
				*Player->GetActorLocation().ToCompactString(), *Player->GetVelocity().ToCompactString(), Movement ? *Movement->GetMovementName() : TEXT("no movement"),
				*Player->MovesetComponent->GetCurrentRole().ToString(), Player->MovesetComponent->IsPlayingVariant() && Playing ? *(FString(TEXT(" as ")) + Playing->GetName()) : TEXT(""),
				Upper.IsNone() ? TEXT("") : *(FString(TEXT(", upper body ")) + Upper.ToString()), *UEnum::GetDisplayValueAsText(Player->GetDirection()).ToString(),
				!Sprite ? TEXT("none") : !Sprite->IsVisible() ? TEXT("hidden") : Sprite->bHiddenInGame ? TEXT("hidden in game") : TEXT("shown"), Sprite ? *Sprite->GetRelativeLocation().ToCompactString() : TEXT("-"),
				DataAt(Onyx2D::FlashDataIndex), DataAt(Onyx2D::DissolveDataIndex), DataAt(Onyx2D::DrawOrderDataIndex), Player->Health->GetHealth(),
				Player->Health->IsDead() ? TEXT(" (dead)") : TEXT(""));
		}));

	FAutoConsoleCommandWithWorldAndArgs WhereCommand(TEXT("Onyx2D.Where"),
		TEXT("Logs where an actor (by label or name) is and how fast it moves, to follow it in a test: Onyx2D.Where <Actor>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const AActor* Actor = World && !Args.IsEmpty() ? FindActorNamed(World, Args[0]) : nullptr;
			if (!Actor)
			{
				UE_LOG(LogTemp, Warning, TEXT("Onyx2D.Where: no actor called %s."), Args.IsEmpty() ? TEXT("(nothing)") : *Args[0]);
				return;
			}
			const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
			UE_LOG(LogTemp, Display, TEXT("Onyx2D.Where: %s at %s, velocity %s%s"), *Args[0], *Actor->GetActorLocation().ToCompactString(),
				*Actor->GetVelocity().ToCompactString(), Root && Root->IsSimulatingPhysics() ? *FString::Printf(TEXT(", simulating (%.0f kg%s)"), Root->GetMass(),
				Root->RigidBodyIsAwake() ? TEXT(", awake") : TEXT(", asleep")) : TEXT(""));
		}));

	FAutoConsoleCommandWithWorldAndArgs MoodCommand(TEXT("Onyx2D.Mood"),
		TEXT("Gives the level another lighting mood, to see it at another hour: Onyx2D.Mood <Day | Dusk | Night | Cave | Dungeon>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const UEnum* Presets = StaticEnum<EOnyxMoodPreset>();
			const int64 Value = Args.IsEmpty() || !Presets ? INDEX_NONE : Presets->GetValueByNameString(Args[0]);
			if (!World || Value == INDEX_NONE)
			{
				UE_LOG(LogTemp, Warning, TEXT("Onyx2D.Mood <Day | Dusk | Night | Cave | Dungeon>"));
				return;
			}
			for (TActorIterator<AOnyxLightingEnvironment> It(World); It; ++It)
			{
				It->Preset = EOnyxMoodPreset(Value);
				It->Mood = FOnyxLightingMood::FromPreset(It->Preset);
				It->Profile = nullptr;
				It->bDayNightCycle = false;
				UE_LOG(LogTemp, Display, TEXT("Onyx2D.Mood: %s is now %s"), *It->GetActorNameOrLabel(), *Args[0]);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs RainCommand(TEXT("Onyx2D.Rain"),
		TEXT("Makes it rain (or stop: 0) to test rain in a level, starting an Onyx Rain if the level has none: Onyx2D.Rain [Intensity 0-1, default 1]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World)
			{
				return;
			}
			const float Intensity = Args.IsEmpty() ? 1.f : FMath::Clamp(FCString::Atof(*Args[0]), 0.f, 1.f);
			TActorIterator<AOnyxRain> It(World);
			AOnyxRain* Rain = It ? *It : nullptr;
			if (!Rain)
			{
				const APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
				Rain = World->SpawnActor<AOnyxRain>(Player ? Player->GetActorLocation() * FVector(1.f, 0.f, 1.f) : FVector::ZeroVector, FRotator::ZeroRotator);
			}
			if (Rain)
			{
				Rain->SetIntensity(Intensity, 0.5f);
				UE_LOG(LogTemp, Display, TEXT("Onyx2D.Rain: %s at %.2f"), *Rain->GetActorNameOrLabel(), Intensity);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GoToCommand(TEXT("Onyx2D.GoTo"),
		TEXT("Puts the player at an actor (by label or name), to test that part of the level: Onyx2D.GoTo <Actor> [OffsetX in cm] [OffsetZ in cm, up]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APawn* Player = World && !Args.IsEmpty() ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
			AActor* Target = Player ? FindActorNamed(World, Args[0]) : nullptr;
			if (!Target)
			{
				UE_LOG(LogTemp, Warning, TEXT("Onyx2D.GoTo: no player, or no actor called %s."), Args.IsEmpty() ? TEXT("(nothing)") : *Args[0]);
				return;
			}
			const float OffsetX = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;
			const float OffsetZ = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.f;
			const FVector Spot = Target->GetActorLocation() + FVector(OffsetX, 0.f, OffsetZ);
			FVector At(Spot.X, Player->GetActorLocation().Y, Spot.Z);
			// Standing on the ground under the actor: its feet at the actor's height, then down onto the first thing there.
			// (Put its middle there and a waystone or a prop half a tile off the floor leaves it half in the ground.)
			const UOnyxMovementComponent* OnyxMovement = Player->FindComponentByClass<UOnyxMovementComponent>();
			if (const ACharacter* Character = Cast<ACharacter>(Player); Character && OnyxMovement && OnyxMovement->IsTopDown())
			{
				// From above there is no ground under it to drop onto: its feet right on the actor's spot.
				At.Z += Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			}
			else if (Character)
			{
				const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
				const FVector Start = At + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 2.f);
				FCollisionQueryParams Params(TEXT("OnyxGoTo"), false, Player);
				FHitResult Ground;
				At = World->SweepSingleByChannel(Ground, Start, Start - FVector(0.f, 0.f, 4000.f), FQuat::Identity, Capsule->GetCollisionObjectType(), Capsule->GetCollisionShape(), Params)
					&& !Ground.bStartPenetrating ? Ground.Location : Start;
			}
			Player->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
			if (UMovementComponent* Movement = Player->GetMovementComponent())
			{
				Movement->StopMovementImmediately();
			}
			if (UOnyxCameraComponent* Camera = Player->FindComponentByClass<UOnyxCameraComponent>())
			{
				Camera->SnapToTarget();
			}
		}));

	/** One action of the player's, as if its button were pressed. */
	void PlayerAct(UWorld* World, const FString& Action)
	{
		AOnyxCharacter* Player = Cast<AOnyxCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		UOnyxMovementComponent* Movement = Player ? Player->GetOnyxMovement() : nullptr;
		if (!Player || !Movement)
		{
			return;
		}
		UOnyxInventoryComponent* Inventory = Player->FindComponentByClass<UOnyxInventoryComponent>();
		if (Action.Equals(TEXT("Attack"), ESearchCase::IgnoreCase))
		{
			// As the button does: a tool or seeds held are used, anything else swings.
			if (!Inventory || !Inventory->UseSelectedItem())
			{
				Player->Combat->RequestAttack();
			}
		}
		else if (Action.Equals(TEXT("Shoot"), ESearchCase::IgnoreCase) || Action.Equals(TEXT("Throw"), ESearchCase::IgnoreCase))
		{
			// A tap: drawn in full, then loosed (a throw plays through).
			if (UOnyxRangedComponent* Ranged = Player->FindComponentByClass<UOnyxRangedComponent>())
			{
				const EOnyxRangedSlot Slot = Action.Equals(TEXT("Shoot"), ESearchCase::IgnoreCase) ? EOnyxRangedSlot::Shoot : EOnyxRangedSlot::Throw;
				UE_LOG(LogTemp, Display, TEXT("Onyx2D.Act %s: %s"), *Action, Ranged->Press(Slot) ? TEXT("started") : TEXT("can't now"));
				Ranged->Release(Slot);
			}
		}
		else if (Action.StartsWith(TEXT("Slot"), ESearchCase::IgnoreCase) && Inventory)
		{
			Inventory->SelectSlot(FCString::Atoi(*Action.Mid(4)) - 1);
		}
		else if (Action.Equals(TEXT("Jump"), ESearchCase::IgnoreCase))
		{
			Movement->JumpPressed();
			TWeakObjectPtr<UOnyxMovementComponent> WeakMovement(Movement);
			FTimerHandle Release;
			World->GetTimerManager().SetTimer(Release, FTimerDelegate::CreateLambda([WeakMovement]()
			{
				if (UOnyxMovementComponent* Held = WeakMovement.Get())
				{
					Held->JumpReleased();
				}
			}), 0.4f, false); // held to the top of a full jump
		}
		else if (Action.Equals(TEXT("Dash"), ESearchCase::IgnoreCase))
		{
			Movement->TryDash(Onyx2D::FacingSign(Player->GetFacing()));
		}
		else if (Action.Equals(TEXT("Left"), ESearchCase::IgnoreCase) || Action.Equals(TEXT("Right"), ESearchCase::IgnoreCase))
		{
			Player->SetFacing(Action.Equals(TEXT("Left"), ESearchCase::IgnoreCase) ? EOnyxFacing::Left : EOnyxFacing::Right);
		}
		else if (Action.Equals(TEXT("Interact"), ESearchCase::IgnoreCase))
		{
			if (UOnyxPlayerControlComponent* Control = Player->FindComponentByClass<UOnyxPlayerControlComponent>())
			{
				Control->Interact();
			}
		}
	}

	FAutoConsoleCommandWithWorldAndArgs NewDayCommand(TEXT("Onyx2D.NewDay"),
		TEXT("Starts the next day on the Onyx Calendar at once (crops grow, the clock goes back to the morning), to test growing: Onyx2D.NewDay [Days, default 1]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UOnyxCalendarSubsystem* Calendar = UOnyxCalendarSubsystem::Get(World);
			if (!Calendar)
			{
				return;
			}
			const int32 Days = Args.IsEmpty() ? 1 : FMath::Clamp(FCString::Atoi(*Args[0]), 1, 100);
			for (int32 Index = 0; Index < Days; ++Index)
			{
				Calendar->StartNewDay();
			}
			UE_LOG(LogTemp, Display, TEXT("Onyx2D.NewDay: %s, %s"), *Calendar->GetDateText().ToString(), *Calendar->GetTimeText().ToString());
		}));

	FAutoConsoleCommandWithWorldAndArgs HeldLookCommand(TEXT("Onyx2D.HeldLook"),
		TEXT("Shows another look (a skin) for an item in the player's hands, to try skins: Onyx2D.HeldLook <Item> <Look | None>, by asset name (Hoe or I_Hoe; Hoe_Copper or HL_Hoe_Copper)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
			UOnyxInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UOnyxInventoryComponent>() : nullptr;
			if (!Inventory || Args.Num() < 2)
			{
				UE_LOG(LogTemp, Warning, TEXT("Onyx2D.HeldLook <Item> <Look | None> (and a player with an Onyx Inventory)"));
				return;
			}
			auto Named = [](const FString& Name, const FString& Wanted, const TCHAR* Prefix)
			{
				return Name.Equals(Wanted, ESearchCase::IgnoreCase) || Name.Equals(Prefix + Wanted, ESearchCase::IgnoreCase);
			};
			UOnyxItem* Item = nullptr;
			for (const FOnyxItemStack& Stack : Inventory->GetSlots())
			{
				if (Stack.Item && Named(Stack.Item->GetName(), Args[0], TEXT("I_")))
				{
					Item = Stack.Item;
					break;
				}
			}
			UOnyxHeldLook* Look = nullptr;
			const bool bNone = Args[1].Equals(TEXT("None"), ESearchCase::IgnoreCase);
			if (!bNone)
			{
				TArray<FAssetData> Looks;
				IAssetRegistry::GetChecked().GetAssetsByClass(UOnyxHeldLook::StaticClass()->GetClassPathName(), Looks);
				for (const FAssetData& Asset : Looks)
				{
					if (Named(Asset.AssetName.ToString(), Args[1], TEXT("HL_")))
					{
						Look = Cast<UOnyxHeldLook>(Asset.GetAsset());
						break;
					}
				}
			}
			if (!Item || (!Look && !bNone))
			{
				UE_LOG(LogTemp, Warning, TEXT("Onyx2D.HeldLook: %s"), !Item ? TEXT("the player carries no such item.") : TEXT("no Onyx Held Look by that name."));
				return;
			}
			Inventory->SetHeldLook(Item, Look);
			UE_LOG(LogTemp, Display, TEXT("Onyx2D.HeldLook: %s now shows %s"), *Item->GetName(), Look ? *Look->GetName() : TEXT("its own look"));
		}));

	FAutoConsoleCommandWithWorldAndArgs ClipCommand(TEXT("Onyx2D.Clip"),
		TEXT("Plays one of the player's clips once, with a held look in its hands if named, to see animations in the game: Onyx2D.Clip <Role> [Look] (Wave; BowDraw Bow; Throw HL_Bomb)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AOnyxCharacter* Player = Cast<AOnyxCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
			if (!Player || !Player->MovesetComponent || Args.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("Onyx2D.Clip <Role> [Look] (and an Onyx character playing)"));
				return;
			}
			const FName Role(*Args[0]);
			UOnyxHeldLook* Look = nullptr;
			if (Args.Num() > 1)
			{
				TArray<FAssetData> Looks;
				IAssetRegistry::GetChecked().GetAssetsByClass(UOnyxHeldLook::StaticClass()->GetClassPathName(), Looks);
				for (const FAssetData& Asset : Looks)
				{
					const FString Name = Asset.AssetName.ToString();
					if (Name.Equals(Args[1], ESearchCase::IgnoreCase) || Name.Equals(TEXT("HL_") + Args[1], ESearchCase::IgnoreCase))
					{
						Look = Cast<UOnyxHeldLook>(Asset.GetAsset());
						break;
					}
				}
			}
			UOnyxInventoryComponent* Inventory = Player->FindComponentByClass<UOnyxInventoryComponent>();
			const float Length = Inventory ? Inventory->PlayClipWithLook(Role, Look) : Player->MovesetComponent->PlayOneShot(Role);
			UE_LOG(LogTemp, Display, TEXT("Onyx2D.Clip: %s%s, %.2f s"), *Role.ToString(), Look ? *(TEXT(" with ") + Look->GetName()) : TEXT(""), Length);
		}));

	FAutoConsoleCommandWithWorldAndArgs AimCommand(TEXT("Onyx2D.Aim"),
		TEXT("Aims the player's shots this many degrees up from level, ahead (instead of the mouse), to test aiming: Onyx2D.Aim <Degrees | Clear>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APawn* Player = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
			UOnyxRangedComponent* Ranged = Player ? Player->FindComponentByClass<UOnyxRangedComponent>() : nullptr;
			if (!Ranged || Args.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("Onyx2D.Aim <Degrees | Clear> (and a player with Onyx Ranged)"));
				return;
			}
			if (Args[0].Equals(TEXT("Clear"), ESearchCase::IgnoreCase))
			{
				Ranged->ClearAimOverride();
			}
			else
			{
				Ranged->SetAimOverride(FCString::Atof(*Args[0]));
			}
			UE_LOG(LogTemp, Display, TEXT("Onyx2D.Aim: %s"), *Args[0]);
		}));

	FAutoConsoleCommandWithWorldAndArgs ActCommand(TEXT("Onyx2D.Act"),
		TEXT("Makes the player act as if a button were pressed, to test and record fights: Onyx2D.Act <Attack | Jump | Dash | Left | Right | Interact | Slot<N>> [Delay s] [Every s] [Times]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("Onyx2D.Act <Attack | Jump | Dash | Left | Right | Interact> [Delay s] [Every s] [Times]"));
				return;
			}
			const FString Action = Args[0];
			const float Delay = Args.Num() > 1 ? FMath::Max(FCString::Atof(*Args[1]), 0.01f) : 0.01f;
			const float Every = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.f;
			const int32 Times = Args.Num() > 3 ? FMath::Max(FCString::Atoi(*Args[3]), 1) : (Every > 0.f ? 1000 : 1);
			TWeakObjectPtr<UWorld> WeakWorld(World);
			for (int32 Index = 0; Index < Times; ++Index)
			{
				FTimerHandle Handle;
				World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld, Action]()
				{
					if (UWorld* Live = WeakWorld.Get())
					{
						PlayerAct(Live, Action);
					}
				}), Delay + Index * Every, false);
			}
		}));

	/** Holds one of the player's directions for a while (moving, crouching, looking up), as if the stick were held. */
	void PlayerHold(UWorld* World, const FString& Direction, float Duration)
	{
		AOnyxCharacter* Player = Cast<AOnyxCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		UOnyxMovementComponent* Movement = Player ? Player->GetOnyxMovement() : nullptr;
		if (!Player || !Movement)
		{
			return;
		}
		const bool bDown = Direction.Equals(TEXT("Down"), ESearchCase::IgnoreCase);
		const bool bUp = Direction.Equals(TEXT("Up"), ESearchCase::IgnoreCase);
		const bool bMend = Direction.Equals(TEXT("Mend"), ESearchCase::IgnoreCase);
		if (Direction.Equals(TEXT("Shoot"), ESearchCase::IgnoreCase) || Direction.Equals(TEXT("Throw"), ESearchCase::IgnoreCase))
		{
			// Held for Duration (a bow drawn and aimed), then let go.
			UOnyxRangedComponent* Ranged = Player->FindComponentByClass<UOnyxRangedComponent>();
			if (!Ranged)
			{
				return;
			}
			const EOnyxRangedSlot Slot = Direction.Equals(TEXT("Shoot"), ESearchCase::IgnoreCase) ? EOnyxRangedSlot::Shoot : EOnyxRangedSlot::Throw;
			UE_LOG(LogTemp, Display, TEXT("Onyx2D.Hold %s: %s"), *Direction, Ranged->Press(Slot) ? TEXT("drawing") : TEXT("can't now"));
			TWeakObjectPtr<UOnyxRangedComponent> WeakRanged(Ranged);
			FTimerHandle LetGo;
			World->GetTimerManager().SetTimer(LetGo, FTimerDelegate::CreateLambda([WeakRanged, Slot]()
			{
				if (UOnyxRangedComponent* Held = WeakRanged.Get())
				{
					Held->Release(Slot);
				}
			}), Duration, false);
			return;
		}
		if (bMend)
		{
			// Mend held for Duration, then let go.
			UOnyxEnergyComponent* Energy = Player->FindComponentByClass<UOnyxEnergyComponent>();
			if (!Energy)
			{
				return;
			}
			UE_LOG(LogTemp, Display, TEXT("Onyx2D.Hold Mend: %s (energy %.0f)"), Energy->StartMend() ? TEXT("mending") : TEXT("can't mend now"), Energy->GetEnergy());
			TWeakObjectPtr<UOnyxEnergyComponent> WeakEnergy(Energy);
			FTimerHandle Release;
			World->GetTimerManager().SetTimer(Release, FTimerDelegate::CreateLambda([WeakEnergy]()
			{
				if (UOnyxEnergyComponent* Held = WeakEnergy.Get())
				{
					Held->StopMend();
				}
			}), Duration, false);
			return;
		}
		const float Side = Direction.Equals(TEXT("Left"), ESearchCase::IgnoreCase) ? -1.f : Direction.Equals(TEXT("Right"), ESearchCase::IgnoreCase) ? 1.f : 0.f;
		if (bDown)
		{
			Movement->SetCrouchHeld(true);
			Movement->SetVerticalInput(-1.f);
		}
		else if (bUp)
		{
			Movement->SetVerticalInput(1.f);
		}
		// Steering has to be given every frame, like a held stick.
		TWeakObjectPtr<AOnyxCharacter> WeakPlayer(Player);
		TSharedRef<float> Left = MakeShared<float>(Duration);
		TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
		// A looping timer runs once per Step that went by (several times in a long frame), so it counts down by Step.
		constexpr float Step = 0.005f;
		// The world held weakly too: a level change (an exit, a death) can end it while the hold is still running.
		TWeakObjectPtr<UWorld> WeakWorld(World);
		World->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda([WeakPlayer, Left, Handle, Side, bDown, bUp, WeakWorld, Step]()
		{
			UWorld* LiveWorld = WeakWorld.Get();
			if (!LiveWorld || LiveWorld->bIsTearingDown)
			{
				return; // its timers go with it
			}
			AOnyxCharacter* Held = WeakPlayer.Get();
			*Left -= Step;
			if (!Held || *Left <= 0.f)
			{
				if (UOnyxMovementComponent* Moving = Held ? Held->GetOnyxMovement() : nullptr)
				{
					if (bDown)
					{
						Moving->SetCrouchHeld(false);
					}
					if (bDown || bUp)
					{
						Moving->SetVerticalInput(0.f);
					}
				}
				LiveWorld->GetTimerManager().ClearTimer(*Handle);
				return;
			}
			if (Side != 0.f)
			{
				Held->AddMovementInput(FVector::ForwardVector, Side);
			}
		}), Step, true);
	}

	FAutoConsoleCommandWithWorldAndArgs HoldCommand(TEXT("Onyx2D.Hold"),
		TEXT("Holds one of the player's directions (or Mend) for a while, to test and record movement: Onyx2D.Hold <Left | Right | Down | Up | Mend> [Delay s] [Duration s]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("Onyx2D.Hold <Left | Right | Down | Up | Mend> [Delay s] [Duration s]"));
				return;
			}
			const FString Direction = Args[0];
			const float Delay = Args.Num() > 1 ? FMath::Max(FCString::Atof(*Args[1]), 0.01f) : 0.01f;
			const float Duration = Args.Num() > 2 ? FMath::Max(FCString::Atof(*Args[2]), 0.05f) : 0.5f;
			TWeakObjectPtr<UWorld> WeakWorld(World);
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld, Direction, Duration]()
			{
				if (UWorld* Live = WeakWorld.Get())
				{
					PlayerHold(Live, Direction, Duration);
				}
			}), Delay, false);
		}));

	FAutoConsoleCommandWithWorldAndArgs HurtCommand(TEXT("Onyx2D.Hurt"),
		TEXT("Takes health from the player (no knockback), to test healing: Onyx2D.Hurt [Amount]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APawn* Player = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
			if (UOnyxHealthComponent* Health = Player ? Player->FindComponentByClass<UOnyxHealthComponent>() : nullptr)
			{
				FOnyxDamageInfo Damage;
				Damage.Amount = Args.IsEmpty() ? 1.f : FMath::Max(FCString::Atof(*Args[0]), 0.f);
				Damage.bIgnoreInvulnerability = true;
				Health->ApplyDamage(Damage);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs EnergyCommand(TEXT("Onyx2D.Energy"),
		TEXT("Gives the player energy (Onyx Energy), to test mending: Onyx2D.Energy [Amount, default full]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APawn* Player = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
			if (UOnyxEnergyComponent* Energy = Player ? Player->FindComponentByClass<UOnyxEnergyComponent>() : nullptr)
			{
				Energy->AddEnergy(Args.IsEmpty() ? Energy->MaxEnergy : FCString::Atof(*Args[0]));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs BossCommand(TEXT("Onyx2D.Boss"),
		TEXT("Tests a boss fight: Onyx2D.Boss <Start | Health <share 0-1> | Stagger | Move <name> | Kill>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("Onyx2D.Boss <Start | Health <share 0-1> | Stagger | Kill>"));
				return;
			}
			const FString What = Args[0];
			if (What.Equals(TEXT("Start"), ESearchCase::IgnoreCase))
			{
				for (TActorIterator<AOnyxBossArena> It(World); It; ++It)
				{
					It->StartFight();
				}
				return;
			}
			for (TActorIterator<AOnyxCharacter> It(World); It; ++It)
			{
				UOnyxBossComponent* Boss = It->FindComponentByClass<UOnyxBossComponent>();
				if (!Boss || It->Health->IsDead())
				{
					continue;
				}
				if (What.Equals(TEXT("Stagger"), ESearchCase::IgnoreCase))
				{
					Boss->ForceStagger();
				}
				else if (What.Equals(TEXT("Move"), ESearchCase::IgnoreCase) && Args.Num() > 1)
				{
					Boss->ForceMove(FName(*Args[1]));
				}
				else if (What.Equals(TEXT("Kill"), ESearchCase::IgnoreCase))
				{
					It->Health->Kill(nullptr);
				}
				else if (What.Equals(TEXT("Health"), ESearchCase::IgnoreCase) && Args.Num() > 1)
				{
					// Down to that share of its health, as if it had been hit (its phases follow).
					const float Goal = FMath::Clamp(FCString::Atof(*Args[1]), 0.f, 1.f) * It->Health->MaxHealth;
					FOnyxDamageInfo Damage;
					Damage.Amount = It->Health->GetHealth() - Goal;
					Damage.bIgnoreInvulnerability = true;
					if (Damage.Amount > 0.f)
					{
						It->Health->ApplyDamage(Damage);
					}
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs SlowCommand(TEXT("Onyx2D.Slow"),
		TEXT("Slows the game down (or speeds it up) to watch or record a move frame by frame: Onyx2D.Slow [factor, 1 = normal]. ")
		TEXT("Timed Onyx2D.Act and Onyx2D.Hold run on the game's clock, so they slow down with it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World)
			{
				const float Factor = Args.IsEmpty() ? 1.f : FMath::Clamp(FCString::Atof(*Args[0]), 0.01f, 20.f);
				UGameplayStatics::SetGlobalTimeDilation(World, Factor);
				UE_LOG(LogTemp, Display, TEXT("Onyx2D.Slow: the game runs at %.2fx."), Factor);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GodCommand(TEXT("Onyx2D.God"),
		TEXT("The player can't be hurt (1) or can again (0), to watch a fight through: Onyx2D.God [1 | 0]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APawn* Player = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
			if (UOnyxHealthComponent* Health = Player ? Player->FindComponentByClass<UOnyxHealthComponent>() : nullptr)
			{
				Health->SetInvulnerable(Args.IsEmpty() || FCString::Atoi(*Args[0]) != 0);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs LightsCommand(TEXT("Onyx2D.Lights"),
		TEXT("Lists every 2D light (or those whose owner's name has the text): shape, place, reach, brightness now, pattern: Onyx2D.Lights [text]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UOnyxLightingSubsystem* Lighting = UOnyxLightingSubsystem::Get(World);
			if (!Lighting)
			{
				return;
			}
			static const TCHAR* Shapes[] = { TEXT("Point"), TEXT("Spot"), TEXT("Line"), TEXT("Area") };
			for (const UOnyxLight2DComponent* Light : Lighting->GetLights())
			{
				const FString Owner = Light->GetOwner() ? Light->GetOwner()->GetActorNameOrLabel() : TEXT("?");
				if (!Args.IsEmpty() && !Owner.Contains(Args[0]))
				{
					continue;
				}
				const FVector2D At = Light->GetPlanePosition();
				UE_LOG(LogTemp, Display, TEXT("Light %s: %s at (%.0f, %.0f), radius %.0f, brightness %.2f (x%.2f, on %.2f), visible %d, colour %s%s"), *Owner,
					Shapes[FMath::Clamp(int32(Light->Shape), 0, 3)], At.X, At.Y, Light->Radius, Light->GetEffectiveIntensity(), Light->IntensityScale,
					Light->GetEnabledAlpha(), Light->IsVisible() ? 1 : 0, *Light->Color.ToString(), Light->Pattern ? *(TEXT(", pattern ") + Light->Pattern->GetName()) : TEXT(""));
			}
			for (const UOnyxPixelLightComponent* Light : Lighting->GetPixelLights())
			{
				const FString Owner = Light->GetOwner() ? Light->GetOwner()->GetActorNameOrLabel() : TEXT("?");
				FOnyxPixelLightQuad Quad;
				const bool bLit = Light->GetLightQuad(Quad);
				UE_LOG(LogTemp, Display, TEXT("Pixel light %s: set %s (%d frames), intensity %.2f, lit this frame %d%s"), *Owner, *GetNameSafe(Light->LightSet),
					Light->LightSet ? Light->LightSet->Frames.Num() : 0, Light->Intensity, bLit ? 1 : 0,
					bLit ? *FString::Printf(TEXT(" at (%.0f, %.0f)-(%.0f, %.0f)"), Quad.Corners[3].X, Quad.Corners[3].Y, Quad.Corners[1].X, Quad.Corners[1].Y) : TEXT(""));
			}
			const FOnyxLightBufferStats& Stats = Lighting->GetLastStats();
			UE_LOG(LogTemp, Display, TEXT("Lights: %d drawn last frame, %d shadows kept, %d worked out, %d pixel lights"), Stats.Drawn, Stats.ShadowsReused,
				Stats.ShadowsComputed, Stats.PixelLightsDrawn);
		}));

	FAutoConsoleCommandWithWorldAndArgs SpriteCommand(TEXT("Onyx2D.Sprite"),
		TEXT("Logs what each character (or those whose name has the text) shows now: clip, frame, whether it is drawn, where, its flash and dissolve: Onyx2D.Sprite [text]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			for (TActorIterator<AOnyxCharacter> It(World); It; ++It)
			{
				const FString Name = It->GetActorNameOrLabel();
				UPaperFlipbookComponent* Sprite = It->GetSprite();
				if (!Sprite || (!Args.IsEmpty() && !Name.Contains(Args[0])))
				{
					continue;
				}
				const UPaperFlipbook* Flipbook = Sprite->GetFlipbook();
				const UPaperSprite* Frame = Flipbook ? Flipbook->GetSpriteAtTime(Sprite->GetPlaybackPosition(), true) : nullptr;
				const FVector Location = Sprite->GetComponentLocation();
				const FVector Scale = Sprite->GetComponentScale();
				const TArray<float>& Data = Sprite->GetCustomPrimitiveData().Data;
				auto DataAt = [&Data](int32 Index) { return Data.IsValidIndex(Index) ? Data[Index] : 0.f; };
				UE_LOG(LogTemp, Display, TEXT("Sprite %s: %s frame %d (%s) at %.2fs of %.2fs, visible %d, hidden %d, at (%.0f, %.0f, %.0f), scale (%.2f, %.2f, %.2f), colour %s, flash %.2f, dissolve %.2f, material %s"),
					*Name, *GetNameSafe(Flipbook), Sprite->GetPlaybackPositionInFrames(), *GetNameSafe(Frame), Sprite->GetPlaybackPosition(),
					Flipbook ? Flipbook->GetTotalDuration() : 0.f, Sprite->IsVisible() ? 1 : 0, Sprite->bHiddenInGame ? 1 : 0, Location.X, Location.Y, Location.Z,
					Scale.X, Scale.Y, Scale.Z, *Sprite->GetSpriteColor().ToString(), DataAt(Onyx2D::FlashDataIndex), DataAt(Onyx2D::DissolveDataIndex),
					*GetNameSafe(Sprite->GetMaterial(0)));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs SaveLightBufferCommand(TEXT("Onyx2D.SaveLightBuffer"),
		TEXT("Saves what the 2D lights drew this frame as PNGs (light, own glow), half brightness so strong light still shows: Onyx2D.SaveLightBuffer [name]. In Saved/OnyxLightBuffers."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UOnyxLightingSubsystem* Lighting = UOnyxLightingSubsystem::Get(World);
			if (!Lighting)
			{
				return;
			}
			const FString Name = Args.IsEmpty() ? TEXT("LightBuffer") : Args[0];
			auto Save = [&Name](UTextureRenderTarget2D* Target, const TCHAR* Suffix)
			{
				FTextureRenderTargetResource* Resource = Target ? Target->GameThread_GetRenderTargetResource() : nullptr;
				TArray<FLinearColor> Linear;
				if (!Resource || !Resource->ReadLinearColorPixels(Linear) || Linear.IsEmpty())
				{
					return;
				}
				TArray<FColor> Pixels;
				Pixels.Reserve(Linear.Num());
				for (const FLinearColor& Value : Linear)
				{
					Pixels.Add((Value * 0.5f).ToFColor(true).WithAlpha(255));
				}
				TArray64<uint8> Png;
				FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, Pixels, Png);
				const FString File = FPaths::ProjectSavedDir() / TEXT("OnyxLightBuffers") / Name + Suffix + TEXT(".png");
				FFileHelper::SaveArrayToFile(Png, *File);
				UE_LOG(LogTemp, Display, TEXT("Onyx2D.SaveLightBuffer: %s (%dx%d)"), *FPaths::ConvertRelativePathToFull(File), Target->SizeX, Target->SizeY);
			};
			Save(Lighting->GetLightBuffer(), TEXT(""));
			Save(Lighting->GetOwnGlowBuffer(), TEXT("_OwnGlow"));
			Save(Lighting->GetHaloBuffer(), TEXT("_Halo"));
		}));

	FAutoConsoleCommandWithWorldAndArgs BackdropCommand(TEXT("Onyx2D.Backdrop"),
		TEXT("Shows (1) or hides (0) a parallax background layer by name (or all of them), to see what each one draws: Onyx2D.Backdrop <Layer name | all> <0 | 1>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() < 2)
			{
				UE_LOG(LogTemp, Warning, TEXT("Onyx2D.Backdrop <Layer name | all> <0 | 1>"));
				return;
			}
			const bool bAll = Args[0].Equals(TEXT("all"), ESearchCase::IgnoreCase);
			const bool bShow = FCString::Atoi(*Args.Last()) != 0; // the last word: a layer's name may have spaces
			for (TActorIterator<AOnyxParallaxBackground> It(World); It; ++It)
			{
				TArray<UStaticMeshComponent*> Quads;
				It->GetComponents(Quads);
				for (int32 Index = 0; Index < It->BackgroundLayers.Num(); ++Index)
				{
					// A layer's name may have spaces: the arguments after the first are its name's words, the last one 0 or 1.
					const FString Name = FString::Join(TArrayView<const FString>(Args).Slice(0, Args.Num() - 1), TEXT(" "));
					if (!bAll && !It->BackgroundLayers[Index].Name.ToString().Equals(Name, ESearchCase::IgnoreCase))
					{
						continue;
					}
					for (UStaticMeshComponent* Quad : Quads)
					{
						if (Quad->TranslucencySortPriority == Index) // each layer's quad carries its index as its sort priority
						{
							Quad->SetVisibility(bShow);
						}
					}
					UE_LOG(LogTemp, Display, TEXT("Onyx2D.Backdrop: %s %s"), *It->BackgroundLayers[Index].Name.ToString(), bShow ? TEXT("shown") : TEXT("hidden"));
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs FogCommand(TEXT("Onyx2D.Fog"),
		TEXT("Lists every Onyx Fog (where it is drawn, its size and look), shows or hides those whose name has the text, or fades all of them: Onyx2D.Fog [list | <text> <0 | 1> | amount <n> [seconds]]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World)
			{
				return;
			}
			if (Args.Num() >= 2 && Args[0].Equals(TEXT("amount"), ESearchCase::IgnoreCase))
			{
				if (UOnyxFogSubsystem* Fogs = UOnyxFogSubsystem::Get(World))
				{
					Fogs->SetFogAmount(FCString::Atof(*Args[1]), Args.Num() >= 3 ? FCString::Atof(*Args[2]) : 0.f);
					UE_LOG(LogTemp, Display, TEXT("Onyx2D.Fog: amount %s"), *Args[1]);
				}
				return;
			}
			if (Args.Num() >= 2)
			{
				const bool bShow = FCString::Atoi(*Args.Last()) != 0;
				for (TActorIterator<AOnyxFog> It(World); It; ++It)
				{
					if (Args[0].Equals(TEXT("all"), ESearchCase::IgnoreCase) || It->GetActorNameOrLabel().Contains(Args[0]))
					{
						It->Mesh->SetVisibility(bShow);
						UE_LOG(LogTemp, Display, TEXT("Onyx2D.Fog: %s %s"), *It->GetActorNameOrLabel(), bShow ? TEXT("shown") : TEXT("hidden"));
					}
				}
				return;
			}
			if (const APlayerController* Controller = World->GetFirstPlayerController(); Controller && Controller->PlayerCameraManager)
			{
				const FMinimalViewInfo& View = Controller->PlayerCameraManager->GetCameraCacheView();
				UE_LOG(LogTemp, Display, TEXT("Onyx2D.Fog: camera at %s, %s, FOV %.1f, ortho width %.0f"), *View.Location.ToString(),
					View.ProjectionMode == ECameraProjectionMode::Orthographic ? TEXT("orthographic") : TEXT("perspective"), View.FOV, View.OrthoWidth);
			}
			if (const UOnyxFogSubsystem* Fogs = UOnyxFogSubsystem::Get(World))
			{
				UE_LOG(LogTemp, Display, TEXT("Onyx2D.Fog: amount %.2f"), Fogs->GetFogAmount());
			}
			for (TActorIterator<AOnyxFog> It(World); It; ++It)
			{
				const UProceduralMeshComponent* Mesh = It->Mesh;
				UE_LOG(LogTemp, Display, TEXT("Onyx2D.Fog: %s at %s (drawn at Y %.1f, sort %d, %s), size %.0f x %.0f, %s, colour %s, density %.2f, edge %.0f, %s"),
					*It->GetActorNameOrLabel(), *It->GetActorLocation().ToCompactString(), Mesh ? Mesh->Bounds.Origin.Y : 0.f,
					Mesh ? Mesh->TranslucencySortPriority : 0, Mesh && Mesh->IsVisible() ? TEXT("shown") : TEXT("hidden"), It->Size.X, It->Size.Y,
					It->bInFront ? TEXT("in front") : TEXT("behind"), *It->Look.Color.ToString(), It->Look.Density, It->Look.EdgeSoftness,
					Mesh && Mesh->GetMaterial(0) ? *Mesh->GetMaterial(0)->GetName() : TEXT("no material"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs WaterCommand(TEXT("Onyx2D.Water"),
		TEXT("Lists every Onyx Water: its surface, depth, width and current: Onyx2D.Water"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			for (TActorIterator<AOnyxWater> It(World); It; ++It)
			{
				UE_LOG(LogTemp, Display, TEXT("Onyx2D.Water: %s surface Z %.1f, bottom Z %.1f, width %.0f, current %s%s"),
					*It->GetActorNameOrLabel(), It->GetSurfaceZ(), It->GetBottomZ(), It->Size.X, *It->Current.ToString(), It->IsChangingLevel() ? TEXT(", changing level") : TEXT(""));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs AfterCommand(TEXT("Onyx2D.After"),
		TEXT("Runs a console command after a while of game time, to script a test in play: Onyx2D.After <seconds> <command...>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() < 2)
			{
				UE_LOG(LogTemp, Warning, TEXT("Onyx2D.After <seconds> <command...>"));
				return;
			}
			const float Delay = FMath::Max(FCString::Atof(*Args[0]), 0.01f);
			const FString Command = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
			TWeakObjectPtr<UWorld> WeakWorld(World);
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld, Command]()
			{
				if (UWorld* Live = WeakWorld.Get(); Live && GEngine)
				{
					UE_LOG(LogTemp, Display, TEXT("Onyx2D.After: %s"), *Command);
					GEngine->Exec(Live, *Command);
				}
			}), Delay, false);
		}));
}

