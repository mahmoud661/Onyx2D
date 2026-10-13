// Copyright Onyx2D. All Rights Reserved.

#include "Examples/OnyxStarterGame.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Audio/OnyxLevelAudio.h"
#include "Builders/OnyxAssetUtils.h"
#include "Builders/OnyxTileBuilder.h"
#include "Character/OnyxBehaviourLibrary.h"
#include "Character/OnyxCharacter.h"
#include "Character/OnyxTalkComponent.h"
#include "Combat/OnyxCombatTypes.h"
#include "Components/BoxComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Examples/OnyxSunnyLandExample.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "GameMapsSettings.h"
#include "Level/OnyxBrushLibrary.h"
#include "Level/OnyxLevelTools.h"
#include "Level/OnyxMapPainter.h"
#include "Lighting/OnyxLightingEnvironment.h"
#include "Logic/OnyxAction.h"
#include "Logic/OnyxTriggerZone.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "ObjectTools.h"
#include "OnyxEditorLibrary.h"
#include "OnyxUISettings.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PaperSpriteActor.h"
#include "PaperSpriteComponent.h"
#include "PaperTileMap.h"
#include "PaperTileSet.h"
#include "Sound/SoundBase.h"
#include "World/OnyxParallaxBackground.h"
#include "World/OnyxPickup.h"
#include "World/OnyxSign.h"

const TCHAR* FOnyxStarterGame::ContentFolder = TEXT("/Game/StarterGame");
const TCHAR* FOnyxStarterGame::LevelPath = TEXT("/Game/StarterGame/SG_Meadow");
const TCHAR* FOnyxStarterGame::MenuPath = TEXT("/Game/StarterGame/SG_MainMenu");

/** Helpers of the starter game's builder (named, so unity builds don't mix them with other builders'). */
namespace OnyxStarterBuild
{
	/**
	 * The meadow, one character per tile ('#' = Ground terrain), 64 x 20 tiles of 16 px (64 cm). From the left: the
	 * start with a sign and the frog, a floating platform with gems, the mushroom pit under the key's ledge, and the
	 * cliff whose opening the gate closes (columns 50-51, rows 12-14). The end is past the gate.
	 */
	const TCHAR* const MeadowLayout[] = {
		TEXT("##................................................##..........##"),
		TEXT("##................................................##..........##"),
		TEXT("##................................................##..........##"),
		TEXT("##................................................##..........##"),
		TEXT("##................................................##..........##"),
		TEXT("##................................................##..........##"),
		TEXT("##................................................##..........##"),
		TEXT("##................................................##..........##"),
		TEXT("##...............................#####............##..........##"),
		TEXT("##...............................#####............##..........##"),
		TEXT("##................####............................##..........##"),
		TEXT("##................####............................##..........##"),
		TEXT("##............................................................##"),
		TEXT("##............................................................##"),
		TEXT("##............................................................##"),
		TEXT("#############################...################################"),
		TEXT("#############################...################################"),
		TEXT("#############################...################################"),
		TEXT("################################################################"),
		TEXT("################################################################"),
	};

	template <class T>
	T* LoadAsset(const FString& Path)
	{
		return LoadObject<T>(nullptr, *(Path + TEXT(".") + FPaths::GetBaseFilename(Path)), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	void DeleteAsset(const FString& PackageName)
	{
		TArray<FAssetData> Found;
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssetsByPackageName(FName(PackageName), Found);
		if (!Found.IsEmpty())
		{
			ObjectTools::DeleteAssets(Found, false);
		}
	}

	UPaperSprite* FirstFrame(const UPaperFlipbook* Flipbook)
	{
		return Flipbook && Flipbook->GetNumKeyFrames() > 0 ? Flipbook->GetSpriteAtFrame(0) : nullptr;
	}

	/** A new action of type T inside Owner (a zone, sign or component), as the Details panel's + would make it. */
	template <class T>
	T* NewAction(UObject* Owner, TArray<TObjectPtr<UOnyxAction>>& List, float Delay = 0.f)
	{
		// The owner's flags carry over, so an action on a Blueprint's component template is an archetype too.
		T* Action = NewObject<T>(Owner, NAME_None, Owner->GetMaskedFlags(RF_PropagateToSubObjects) | RF_Transactional);
		Action->Delay = Delay;
		List.Add(Action);
		return Action;
	}

	FOnyxDialogueLine Line(const TCHAR* Speaker, const TCHAR* Text, UObject* Portrait = nullptr)
	{
		FOnyxDialogueLine Result;
		Result.Speaker = FText::FromString(Speaker);
		Result.Text = FText::FromString(Text);
		Result.Portrait = Portrait;
		return Result;
	}

	/** A brush that places one of the project's actor classes, standing on the floor. */
	FOnyxPainterBrush ClassBrush(UClass* Class, const TCHAR* Id)
	{
		FOnyxPainterBrush Brush;
		Brush.Id = Id;
		Brush.Label = FText::FromString(Id);
		Brush.Class = Class;
		Brush.bStandOnFloor = true;
		return Brush;
	}

	/** A brush that places a sprite as decoration (in front, or behind the tiles), standing on the floor. */
	FOnyxPainterBrush SpriteBrush(UPaperSprite* Sprite, const TCHAR* Id)
	{
		FOnyxPainterBrush Brush = ClassBrush(APaperSpriteActor::StaticClass(), Id);
		Brush.Configure = [Sprite](AActor* Actor)
		{
			UPaperSpriteComponent* Component = CastChecked<APaperSpriteActor>(Actor)->GetRenderComponent();
			Component->SetMobility(EComponentMobility::Movable);
			Component->SetSprite(Sprite);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		};
		return Brush;
	}

	/**
	 * Mossy, the friendly frog: made from Sunny Land's frog strips by the same character builder as the New Character
	 * dialog, with the Friendly NPC preset (the Talk behaviour, on the player's team). What it says is set on its Talk
	 * component, so any copy placed says the same.
	 */
	UBlueprint* MakeVillager(const FString& Folder, UPaperSprite* Portrait, FText& OutError)
	{
		const FString Strips = FString(FOnyxSunnyLandExample::ContentFolder) / TEXT("Strips");
		UTexture2D* Idle = LoadAsset<UTexture2D>(Strips / TEXT("T_Frog_Idle"));
		UTexture2D* Jump = LoadAsset<UTexture2D>(Strips / TEXT("T_Frog_Jump"));
		UTexture2D* Fall = LoadAsset<UTexture2D>(Strips / TEXT("T_Frog_Fall"));
		if (!Idle || !Jump || !Fall)
		{
			OutError = INVTEXT("The Sunny Land frog's animation strips are missing.");
			return nullptr;
		}
		// The pack's frog idle is four frames side by side.
		const FIntPoint FrameSize(Idle->Source.GetSizeX() / 4, Idle->Source.GetSizeY());
		UBlueprint* Villager = UOnyxEditorLibrary::CreateCharacterFromStrips(
			{ { TEXT("Idle"), Idle }, { TEXT("Jump"), Jump }, { TEXT("Fall"), Fall } }, TEXT("Mossy"), Folder / TEXT("Characters"), FrameSize,
			int32(EOnyxBehaviour::Talk), OnyxTeams::Player, FOnyxSunnyLandExample::PixelSize, { { TEXT("Idle"), 6.f } });
		AOnyxCharacter* Defaults = Villager ? Cast<AOnyxCharacter>(Villager->GeneratedClass->GetDefaultObject()) : nullptr;
		UOnyxTalkComponent* Talk = Villager ? FOnyxAssetUtils::FindComponentTemplate<UOnyxTalkComponent>(Villager) : nullptr;
		if (!Defaults || !Talk)
		{
			OutError = INVTEXT("Could not create Mossy the frog.");
			return nullptr;
		}
		Defaults->Modify();
		Defaults->bArtFacesLeft = true; // the pack draws its creatures facing left
		Talk->Modify();
		Talk->SpeakerName = INVTEXT("Mossy");
		Talk->Portrait = Portrait;
		Talk->Lines = {
			INVTEXT("Oh! A fox! Hello there, I'm Mossy."),
			INVTEXT("I dropped my blue key past the opossums, way up on a ledge. Bounce on the mushrooms in the pit to get up there!"),
			INVTEXT("Bring it to the gate in the east and it will open for you."),
		};
		Talk->LaterLines = { INVTEXT("The key is up on the ledge over the mushroom pit. The gate is to the east!") };
		Talk->AfterFirstTalk.Reset();
		UOnyxAction_ShowMessage* Goal = NewAction<UOnyxAction_ShowMessage>(Talk, Talk->AfterFirstTalk);
		Goal->Message = INVTEXT("New goal: find Mossy's key");
		Villager->MarkPackageDirty();
		FOnyxAssetUtils::SavePackages({ Villager->GetPackage() });
		return Villager;
	}

	/** A stone gate filling a 2 x 3 tile opening: solid, built from Sunny Land blocks. */
	FOnyxPainterBrush GateBrush(UPaperSprite* BigBlock, UPaperSprite* Block, float Tile)
	{
		FOnyxPainterBrush Brush = ClassBrush(AActor::StaticClass(), TEXT("Gate"));
		Brush.bStandOnFloor = false;
		Brush.Configure = [BigBlock, Block, Tile](AActor* Actor)
		{
			UBoxComponent* Solid = NewObject<UBoxComponent>(Actor, TEXT("Solid"), RF_Transactional);
			Solid->SetBoxExtent(FVector(Tile, 200.f, Tile * 1.5f));
			Solid->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Solid->SetMobility(EComponentMobility::Movable);
			// The empty-actor factory may already have given it a root.
			if (USceneComponent* Root = Actor->GetRootComponent())
			{
				Root->SetMobility(EComponentMobility::Movable);
				Solid->SetupAttachment(Root);
			}
			else
			{
				Actor->SetRootComponent(Solid);
			}
			Actor->AddInstanceComponent(Solid);
			Solid->RegisterComponent();
			// A big block at the bottom, two small ones on top. The block sprites stand on their bottom (bottom pivot): the
			// big one on the box's floor (1.5 tiles below its centre), the small ones on top of it.
			struct FPiece { UPaperSprite* Sprite; FVector Offset; const TCHAR* Name; };
			for (const FPiece& Piece : { FPiece{ BigBlock, FVector(0.f, 0.f, -Tile * 1.5f), TEXT("Bottom") },
				FPiece{ Block, FVector(-Tile * 0.5f, 0.f, Tile * 0.5f), TEXT("TopLeft") }, FPiece{ Block, FVector(Tile * 0.5f, 0.f, Tile * 0.5f), TEXT("TopRight") } })
			{
				UPaperSpriteComponent* Sprite = NewObject<UPaperSpriteComponent>(Actor, Piece.Name, RF_Transactional);
				Sprite->SetupAttachment(Solid);
				Sprite->SetRelativeLocation(Piece.Offset);
				Sprite->SetSprite(Piece.Sprite);
				Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Sprite->SetMobility(EComponentMobility::Movable);
				Actor->AddInstanceComponent(Sprite);
				Sprite->RegisterComponent();
			}
		};
		return Brush;
	}

	struct FAssets
	{
		UPaperTileSet* TileSet = nullptr;
		int32 Ground = INDEX_NONE;
		UClass* GameMode = nullptr;
		UClass* Opossum = nullptr;
		UPaperFlipbook* Gem = nullptr;
		UPaperFlipbook* Cherry = nullptr;
		UPaperFlipbook* Sparkle = nullptr;
		UPaperSprite* FoxFace = nullptr;
		UPaperSprite* FrogFace = nullptr;
		UTexture2D* Back = nullptr;
		UTexture2D* Middle = nullptr;
		USoundBase* Music = nullptr;

		bool Load(FText& OutError)
		{
			const FString SunnyLand = FOnyxSunnyLandExample::ContentFolder;
			TileSet = LoadAsset<UPaperTileSet>(SunnyLand / TEXT("TS_SunnyLand"));
			Ground = TileSet ? UOnyxEditorLibrary::GetTerrainNames(TileSet).IndexOfByKey(TEXT("Ground")) : INDEX_NONE;
			const UBlueprint* Mode = LoadAsset<UBlueprint>(SunnyLand / TEXT("BP_SunnyLandGameMode"));
			const UBlueprint* OpossumBlueprint = LoadAsset<UBlueprint>(SunnyLand / TEXT("Characters/Opossum/BP_Opossum"));
			GameMode = Mode ? Mode->GeneratedClass.Get() : nullptr;
			Opossum = OpossumBlueprint ? OpossumBlueprint->GeneratedClass.Get() : nullptr;
			Gem = LoadAsset<UPaperFlipbook>(SunnyLand / TEXT("Effects/FB_Gem"));
			Cherry = LoadAsset<UPaperFlipbook>(SunnyLand / TEXT("Effects/FB_Cherry"));
			Sparkle = LoadAsset<UPaperFlipbook>(SunnyLand / TEXT("Effects/FB_ItemFeedback"));
			FoxFace = FirstFrame(LoadAsset<UPaperFlipbook>(SunnyLand / TEXT("Characters/Fox/Clips/FB_Fox_Idle")));
			FrogFace = FirstFrame(LoadAsset<UPaperFlipbook>(SunnyLand / TEXT("Characters/Frog/Clips/FB_Frog_Idle")));
			Back = LoadAsset<UTexture2D>(SunnyLand / TEXT("Textures/T_SunnyLand_Back"));
			Middle = LoadAsset<UTexture2D>(SunnyLand / TEXT("Textures/T_SunnyLand_Middle"));
			Music = LoadAsset<USoundBase>(SunnyLand / TEXT("Audio/SW_SunnyLand_Music"));
			if (!TileSet || Ground == INDEX_NONE || !GameMode || !Opossum || !Gem)
			{
				OutError = INVTEXT("The Sunny Land example's assets are incomplete: rebuild it from Home > Build Example.");
				return false;
			}
			return true;
		}
	};

	bool BuildMeadow(const FAssets& Assets, UBlueprint* Villager, bool bMenuIsProjectStart, FScopedSlowTask& Progress, FText& OutError)
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		const FString Folder = FOnyxStarterGame::ContentFolder;
		FOnyxTileMapSpec Spec;
		Spec.TileSet = Assets.TileSet;
		Spec.Name = TEXT("TM_Meadow");
		Spec.Folder = Folder;
		Spec.Width = FCString::Strlen(MeadowLayout[0]);
		Spec.Height = UE_ARRAY_COUNT(MeadowLayout);
		Spec.CentimetersPerPixel = FOnyxSunnyLandExample::PixelSize;
		Spec.bFillGround = false;
		Spec.bPlaceInLevel = true;
		UPaperTileMap* TileMap = FOnyxTileBuilder::CreateTileMap(Spec, OutError);
		if (!TileMap)
		{
			return false;
		}
		FOnyxLevelTools::SetupCurrentLevel(); // game mode, player start, Onyx Lighting, tile collision
		World->GetWorldSettings()->DefaultGameMode = Assets.GameMode;

		// The ground, painted the way the Level Painter paints it.
		Progress.EnterProgressFrame(1.f, INVTEXT("Painting the meadow"));
		FOnyxMapPainter Painter;
		Painter.SetTileMap(TileMap);
		Painter.BeginStroke(INVTEXT("Paint the meadow"));
		for (int32 Row = 0; Row < Spec.Height; ++Row)
		{
			for (int32 Column = 0; Column < Spec.Width; ++Column)
			{
				if (MeadowLayout[Row][Column] == TEXT('#'))
				{
					Painter.PaintTerrain(FIntPoint(Column, Row), Assets.TileSet, Assets.Ground, false);
				}
			}
		}
		Painter.EndStroke();
		Painter.FlushChanges();
		FOnyxAssetUtils::SavePackages({ TileMap->GetPackage() });

		Progress.EnterProgressFrame(1.f, INVTEXT("Placing the gameplay"));
		const FOnyxBrushLibrary& Library = FOnyxBrushLibrary::Get();
		const FName Entities = FOnyxBrushLibrary::EntitiesLayer;
		const FName Behind = FOnyxBrushLibrary::BackgroundLayer;
		const float Tile = 16.f * FOnyxSunnyLandExample::PixelSize;
		auto Place = [&](const FOnyxPainterBrush& Brush, FIntPoint Cell, const TCHAR* Label, FName Layer = NAME_None) -> AActor*
		{
			AActor* Actor = Painter.PlaceObject(Cell, Brush, Layer.IsNone() ? Entities : Layer);
			if (Actor)
			{
				Actor->SetActorLabel(Label);
			}
			return Actor;
		};
		auto Zone = [&](const FIntRect& Cells, const TCHAR* Label) -> AOnyxTriggerZone*
		{
			const TSharedPtr<FOnyxPainterBrush> Brush = Library.FindById(TEXT("TriggerZone"));
			AOnyxTriggerZone* Result = Brush.IsValid() ? Cast<AOnyxTriggerZone>(Painter.PlaceArea(Cells, *Brush, Entities)) : nullptr;
			if (Result)
			{
				Result->SetActorLabel(Label);
			}
			return Result;
		};

		// The fox starts next to the welcome sign, so its "Read" prompt shows straight away.
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			It->SetActorLocation(Painter.CellToWorld(FIntPoint(5, 13)));
		}

		// Scenery behind the tiles.
		for (const TPair<const TCHAR*, FIntPoint>& Piece : { TPair<const TCHAR*, FIntPoint>(TEXT("tree"), FIntPoint(9, 14)), TPair<const TCHAR*, FIntPoint>(TEXT("bush"), FIntPoint(15, 14)),
			TPair<const TCHAR*, FIntPoint>(TEXT("tree"), FIntPoint(43, 14)), TPair<const TCHAR*, FIntPoint>(TEXT("bush"), FIntPoint(25, 14)),
			TPair<const TCHAR*, FIntPoint>(TEXT("house"), FIntPoint(58, 14)) })
		{
			if (UPaperSprite* Sprite = FOnyxSunnyLandExample::ImportProp(Piece.Key))
			{
				Place(SpriteBrush(Sprite, Piece.Key), Piece.Value, *FString::Printf(TEXT("Scenery_%s_%d"), Piece.Key, Piece.Value.X), Behind);
			}
		}

		// Signs.
		UPaperSprite* SignArt = FOnyxSunnyLandExample::ImportProp(TEXT("sign"));
		const TSharedPtr<FOnyxPainterBrush> SignBrush = Library.FindById(TEXT("Sign"));
		auto Sign = [&](FIntPoint Cell, const TCHAR* Label, TArray<FText> Pages, bool bWalkingPast) -> AOnyxSign*
		{
			if (!SignBrush.IsValid())
			{
				return nullptr;
			}
			// The art goes on before placing, so the sign stands on its own post (not on the stand-in board).
			FOnyxPainterBrush Brush = *SignBrush;
			Brush.Configure = [SignArt, Pages = MoveTemp(Pages), bWalkingPast](AActor* Actor)
			{
				AOnyxSign* Placed = CastChecked<AOnyxSign>(Actor);
				Placed->Look = SignArt;
				Placed->Pages = Pages;
				Placed->bReadByWalkingPast = bWalkingPast;
				Placed->RerunConstructionScripts();
			};
			return Cast<AOnyxSign>(Place(Brush, Cell, Label));
		};
		Sign(FIntPoint(6, 14), TEXT("Sign_Welcome"), {
			INVTEXT("Welcome to the Starter Game!"),
			INVTEXT("Move with A and D or the arrow keys. Jump with Space, and press it again in the air to double jump."),
			INVTEXT("Press E next to signs, doors and characters to use them. Jump on enemies to defeat them."),
		}, false);
		Sign(FIntPoint(54, 14), TEXT("Sign_Thanks"), { INVTEXT("Thanks for playing!") }, true);

		// Mossy the frog.
		if (Villager)
		{
			Place(ClassBrush(Villager->GeneratedClass, TEXT("Villager")), FIntPoint(11, 14), TEXT("Mossy"));
		}

		// Opossums on the way, gems to collect, a cherry to heal.
		for (const FIntPoint& Cell : { FIntPoint(23, 14), FIntPoint(40, 14) })
		{
			Place(ClassBrush(Assets.Opossum, TEXT("Opossum")), Cell, *FString::Printf(TEXT("Opossum_%d"), Cell.X));
		}
		const TSharedPtr<FOnyxPainterBrush> CoinBrush = Library.FindById(TEXT("Coin"));
		for (const FIntPoint& Cell : { FIntPoint(15, 13), FIntPoint(16, 13), FIntPoint(19, 9), FIntPoint(20, 9), FIntPoint(26, 13), FIntPoint(45, 13), FIntPoint(46, 13) })
		{
			AOnyxPickup* Gem = CoinBrush.IsValid() ? Cast<AOnyxPickup>(Place(*CoinBrush, Cell, *FString::Printf(TEXT("Gem_%d_%d"), Cell.X, Cell.Y))) : nullptr;
			if (Gem)
			{
				Gem->Flipbook = Assets.Gem;
				Gem->CollectEffect = Assets.Sparkle;
				Gem->BobHeight = 6.f;
				Gem->RerunConstructionScripts();
			}
		}
		if (const TSharedPtr<FOnyxPainterBrush> HealthBrush = Library.FindById(TEXT("Health")))
		{
			if (AOnyxPickup* Cherry = Cast<AOnyxPickup>(Place(*HealthBrush, FIntPoint(36, 7), TEXT("Cherry"))))
			{
				Cherry->Flipbook = Assets.Cherry;
				Cherry->CollectEffect = Assets.Sparkle;
				Cherry->BobHeight = 6.f;
				Cherry->RerunConstructionScripts();
			}
		}

		// The key, up on the ledge (a Key pickup: its item is called "Key").
		if (const TSharedPtr<FOnyxPainterBrush> KeyBrush = Library.FindById(TEXT("Key")))
		{
			Place(*KeyBrush, FIntPoint(34, 7), TEXT("Key"));
		}

		// Bouncy mushrooms: a zone in the pit that throws the player up to the ledge.
		if (UPaperSprite* Shrooms = FOnyxSunnyLandExample::ImportProp(TEXT("shrooms")))
		{
			for (const int32 Column : { 29, 30, 31 })
			{
				Place(SpriteBrush(Shrooms, TEXT("Shrooms")), FIntPoint(Column, 17), *FString::Printf(TEXT("Mushroom_%d"), Column), Behind);
			}
		}
		if (AOnyxTriggerZone* Spring = Zone(FIntRect(FIntPoint(29, 16), FIntPoint(31, 17)), TEXT("Zone_Mushrooms")))
		{
			Spring->When = EOnyxTriggerWhen::PlayerEnters;
			Spring->bOnlyOnce = false;
			Spring->Cooldown = 0.4f;
			Spring->EditorColor = FLinearColor(1.f, 0.55f, 0.85f);
			NewAction<UOnyxAction_Launch>(Spring, Spring->Actions)->Velocity = FVector2D(0.f, 2200.f);
			UOnyxAction_SpawnEffect* Puff = NewAction<UOnyxAction_SpawnEffect>(Spring, Spring->Actions);
			Puff->Effect = Assets.Sparkle;
			Puff->Where = EOnyxActionSpot::AtPlayer;
			NewAction<UOnyxAction_ShakeCamera>(Spring, Spring->Actions)->Strength = 0.12f;
			Spring->RerunConstructionScripts();
		}

		// A checkpoint before the gate.
		if (const TSharedPtr<FOnyxPainterBrush> CheckpointBrush = Library.FindById(TEXT("Checkpoint")))
		{
			Place(*CheckpointBrush, FIntPoint(42, 14), TEXT("Checkpoint"));
		}

		// The gate fills the cliff's opening; the zone in front of it needs the key and lifts it into the cliff.
		AActor* Gate = nullptr;
		UPaperSprite* BigBlock = FOnyxSunnyLandExample::ImportProp(TEXT("block-big"));
		UPaperSprite* Block = FOnyxSunnyLandExample::ImportProp(TEXT("block"));
		if (BigBlock && Block)
		{
			Gate = Place(GateBrush(BigBlock, Block, Tile), FIntPoint(50, 13), TEXT("Gate"), Behind);
			if (Gate)
			{
				// On the Background layer it draws behind the tiles, so it disappears into the cliff as it rises.
				const FVector Left = Painter.CellToWorld(FIntPoint(50, 13));
				const FVector Right = Painter.CellToWorld(FIntPoint(51, 13));
				Gate->SetActorLocation(FVector((Left.X + Right.X) * 0.5f, FOnyxMapPainter::GetObjectLayerDepth(Behind), Left.Z));
			}
		}
		if (AOnyxTriggerZone* GateZone = Zone(FIntRect(FIntPoint(47, 12), FIntPoint(49, 14)), TEXT("Zone_Gate")))
		{
			GateZone->When = EOnyxTriggerWhen::PlayerInteracts;
			GateZone->InteractPrompt = INVTEXT("Open");
			GateZone->Needs = EOnyxTriggerNeeds::Item;
			GateZone->NeededItem = TEXT("Key");
			GateZone->NeededAmount = 1;
			GateZone->bUseThemUp = true;
			GateZone->EditorColor = FLinearColor(0.45f, 0.8f, 1.f);
			NewAction<UOnyxAction_ShakeCamera>(GateZone, GateZone->Actions)->Strength = 0.3f;
			UOnyxAction_Move* Lift = NewAction<UOnyxAction_Move>(GateZone, GateZone->Actions);
			Lift->Actors = { Gate };
			Lift->Offset = FVector2D(0.f, Tile * 3.f);
			Lift->Time = 1.2f;
			Lift->bWaitUntilDone = true;
			NewAction<UOnyxAction_ShowMessage>(GateZone, GateZone->Actions)->Message = INVTEXT("The gate is open!");
			NewAction<UOnyxAction_ShowMessage>(GateZone, GateZone->OtherwiseActions)->Message = INVTEXT("Locked. Mossy the frog knows where the key is.");
			GateZone->RerunConstructionScripts();
		}

		// A goal as the level starts.
		if (AOnyxTriggerZone* Intro = Zone(FIntRect(FIntPoint(2, 2), FIntPoint(3, 3)), TEXT("Zone_Intro")))
		{
			Intro->When = EOnyxTriggerWhen::LevelStarts;
			Intro->EditorColor = FLinearColor(1.f, 0.85f, 0.35f);
			UOnyxAction_ShowMessage* Goal = NewAction<UOnyxAction_ShowMessage>(Intro, Intro->Actions, 0.8f);
			Goal->Message = INVTEXT("Talk to the frog: walk up to it and press E");
			Goal->Duration = 4.f;
			Intro->RerunConstructionScripts();
		}

		// The end: a short conversation, a reward, then back to the menu (or the start).
		if (AOnyxTriggerZone* End = Zone(FIntRect(FIntPoint(56, 10), FIntPoint(60, 14)), TEXT("Zone_End")))
		{
			End->When = EOnyxTriggerWhen::PlayerEnters;
			End->EditorColor = FLinearColor(0.7f, 0.55f, 1.f);
			UOnyxAction_Dialogue* Talk = NewAction<UOnyxAction_Dialogue>(End, End->Actions);
			Talk->Lines = {
				Line(TEXT("Fox"), TEXT("Made it! The gate is open and Mossy has a way home."), Assets.FoxFace),
				Line(TEXT(""), TEXT("That's the whole Starter Game. Every piece of it (the signs, the frog, the gate, the bouncy mushrooms) is a Trigger Zone, a Sign or the Talk behaviour, set up in the Details panel.")),
				Line(TEXT(""), TEXT("Open the Onyx2D window and try the Level Painter to make this level your own!")),
			};
			UOnyxAction_Give* Reward = NewAction<UOnyxAction_Give>(End, End->Actions);
			Reward->What = EOnyxGiveWhat::Coins;
			Reward->Amount = 10;
			UOnyxAction_OpenLevel* Next = NewAction<UOnyxAction_OpenLevel>(End, End->Actions, 0.5f);
			Next->FadeTime = 1.f;
			if (bMenuIsProjectStart)
			{
				Next->Level = TSoftObjectPtr<UWorld>(FSoftObjectPath(FString(FOnyxStarterGame::MenuPath) + TEXT(".") + FPaths::GetBaseFilename(FOnyxStarterGame::MenuPath)));
			}
			End->RerunConstructionScripts();
		}

		FOnyxSunnyLandExample::AddScenery(World, Assets.Back, Assets.Middle);
		if (Assets.Music)
		{
			AOnyxLevelAudio* Audio = World->SpawnActor<AOnyxLevelAudio>(Painter.CellToWorld(FIntPoint(3, 2)), FRotator::ZeroRotator);
			Audio->SetActorLabel(TEXT("LevelAudio"));
			Audio->Music = Assets.Music;
			Audio->MusicVolume = 0.7f;
		}
		if (!UEditorLoadingAndSavingUtils::SaveMap(World, FOnyxStarterGame::LevelPath))
		{
			OutError = FText::Format(INVTEXT("Could not save {0}."), FText::FromString(FOnyxStarterGame::LevelPath));
			return false;
		}
		return true;
	}

	/** The title screen's level: the meadow's sky behind the Onyx2D main menu. */
	bool BuildMenu(const FAssets& Assets, FText& OutError)
	{
		UEditorLoadingAndSavingUtils::NewBlankMap(false);
		UWorld* World = GEditor->GetEditorWorldContext().World();
		FOnyxTileMapSpec Spec;
		Spec.TileSet = Assets.TileSet;
		Spec.Name = TEXT("TM_MenuGround");
		Spec.Folder = FOnyxStarterGame::ContentFolder;
		Spec.Width = 30;
		Spec.Height = 12;
		Spec.CentimetersPerPixel = FOnyxSunnyLandExample::PixelSize;
		Spec.bFillGround = false;
		Spec.bPlaceInLevel = true;
		UPaperTileMap* TileMap = FOnyxTileBuilder::CreateTileMap(Spec, OutError);
		if (!TileMap)
		{
			return false;
		}
		FOnyxLevelTools::SetupCurrentLevel();
		World->GetWorldSettings()->DefaultGameMode = Assets.GameMode;
		FOnyxMapPainter Painter;
		Painter.SetTileMap(TileMap);
		Painter.BeginStroke(INVTEXT("Paint the menu ground"));
		for (int32 Row = 9; Row < Spec.Height; ++Row)
		{
			for (int32 Column = 0; Column < Spec.Width; ++Column)
			{
				Painter.PaintTerrain(FIntPoint(Column, Row), Assets.TileSet, Assets.Ground, false);
			}
		}
		Painter.EndStroke();
		Painter.FlushChanges();
		FOnyxAssetUtils::SavePackages({ TileMap->GetPackage() });
		FOnyxSunnyLandExample::AddScenery(World, Assets.Back, Assets.Middle);
		if (!UEditorLoadingAndSavingUtils::SaveMap(World, FOnyxStarterGame::MenuPath))
		{
			OutError = FText::Format(INVTEXT("Could not save {0}."), FText::FromString(FOnyxStarterGame::MenuPath));
			return false;
		}
		return true;
	}

	/** Makes the starter game what the project opens with. */
	void MakeProjectStart()
	{
		const FSoftObjectPath Menu(FString(FOnyxStarterGame::MenuPath) + TEXT(".") + FPaths::GetBaseFilename(FOnyxStarterGame::MenuPath));
		const FSoftObjectPath Meadow(FString(FOnyxStarterGame::LevelPath) + TEXT(".") + FPaths::GetBaseFilename(FOnyxStarterGame::LevelPath));
		UOnyxUISettings* UISettings = GetMutableDefault<UOnyxUISettings>();
		UISettings->MainMenuLevel = TSoftObjectPtr<UWorld>(Menu);
		UISettings->FirstGameLevel = TSoftObjectPtr<UWorld>(Meadow);
		UISettings->GameTitle = INVTEXT("SUNNY MEADOW");
		UISettings->GameSubtitle = INVTEXT("AN ONYX2D STARTER GAME");
		UISettings->TryUpdateDefaultConfigFile();
		UGameMapsSettings::SetGameDefaultMap(Menu.ToString());
		GetMutableDefault<UGameMapsSettings>()->TryUpdateDefaultConfigFile();
	}
}

bool FOnyxStarterGame::Build(bool bMakeProjectStart, FText& OutError)
{
	using namespace OnyxStarterBuild;
	FScopedSlowTask Progress(6.f, INVTEXT("Building the Starter Game"));
	Progress.MakeDialog();

	FAssets Assets;
	Progress.EnterProgressFrame(1.f, INVTEXT("Sunny Land art"));
	if (!Assets.Load(OutError))
	{
		// First time: import the Sunny Land art (characters, tiles, effects) through its own example.
		if (!FOnyxSunnyLandExample::Build(OutError) || !Assets.Load(OutError))
		{
			return false;
		}
	}

	// Fresh assets each time, so nothing stale is left over.
	UEditorLoadingAndSavingUtils::NewBlankMap(false);
	for (const FString& Package : { FString(LevelPath), FString(MenuPath), FString(ContentFolder) / TEXT("TM_Meadow"),
		FString(ContentFolder) / TEXT("TM_MenuGround"), FString(ContentFolder) / TEXT("BP_FrogVillager") }) // (the old duplicated frog)
	{
		DeleteAsset(Package);
	}

	Progress.EnterProgressFrame(1.f, INVTEXT("Mossy the frog"));
	UBlueprint* Villager = MakeVillager(ContentFolder, Assets.FrogFace, OutError);
	if (!Villager)
	{
		return false;
	}
	if (!BuildMeadow(Assets, Villager, bMakeProjectStart, Progress, OutError))
	{
		return false;
	}
	Progress.EnterProgressFrame(1.f, INVTEXT("Main menu"));
	if (!BuildMenu(Assets, OutError))
	{
		return false;
	}
	if (bMakeProjectStart)
	{
		MakeProjectStart();
	}
	Progress.EnterProgressFrame(1.f, INVTEXT("Opening the meadow"));
	FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(LevelPath, FPackageName::GetMapPackageExtension()));
	return true;
}
