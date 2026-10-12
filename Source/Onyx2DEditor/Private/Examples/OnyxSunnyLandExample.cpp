// Copyright Onyx2D. All Rights Reserved.

#include "Examples/OnyxSunnyLandExample.h"
#include "AI/OnyxPatrolComponent.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxClipEvent.h"
#include "Animation/OnyxClipWindow.h"
#include "Animation/OnyxMoveset.h"
#include "Animation/OnyxMovesetComponent.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Builders/OnyxAssetUtils.h"
#include "Audio/OnyxLevelAudio.h"
#include "Sound/SoundWave.h"
#include "Builders/OnyxFramePacker.h"
#include "Level/OnyxBrushLibrary.h"
#include "Level/OnyxLevelTools.h"
#include "Level/OnyxMapPainter.h"
#include "Camera/OnyxCameraBounds.h"
#include "Camera/OnyxCameraComponent.h"
#include "Builders/OnyxAssetUtils.h"
#include "Character/OnyxBehaviourLibrary.h"
#include "Character/OnyxCharacter.h"
#include "Combat/OnyxCombatTypes.h"
#include "Character/OnyxPlayerControlComponent.h"
#include "Combat/OnyxHealthComponent.h"
#include "Combat/OnyxHitboxComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Core/OnyxGameMode.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Lighting/OnyxLight2D.h"
#include "Lighting/OnyxLightingEnvironment.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Movement/OnyxMovementComponent.h"
#include "ObjectTools.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PaperSpriteActor.h"
#include "PaperSpriteComponent.h"
#include "PaperTileMap.h"
#include "PaperTileMapActor.h"
#include "PaperTileMapComponent.h"
#include "PaperTileSet.h"
#include "OnyxEditorLibrary.h"
#include "World/OnyxParallaxBackground.h"
#include "World/OnyxPickup.h"

const TCHAR* FOnyxSunnyLandExample::LevelPath = TEXT("/Game/Maps/SunnyLand");
const TCHAR* FOnyxSunnyLandExample::ContentFolder = TEXT("/Game/SunnyLand");

namespace
{
	const TCHAR* Root = FOnyxSunnyLandExample::ContentFolder;
	constexpr float CmPerPixel = FOnyxSunnyLandExample::PixelSize;
	constexpr int32 TilePixels = 16;
	constexpr float TileWorld = TilePixels * CmPerPixel;
	constexpr int32 MapHeightTiles = 25;

	// Collision lists from the original game (Phaser tile ids are 1-based).
	constexpr int32 SolidIds[] = { 27, 29, 31, 33, 35, 37, 77, 81, 86, 87, 127, 129, 131, 133, 134, 135, 83, 84, 502, 504, 505, 529, 530,
		333, 335, 337, 339, 366, 368, 262, 191, 193, 195, 241, 245, 291, 293, 295 };
	constexpr int32 OneWayIds[] = { 35, 36, 84, 86, 134, 135, 366, 367, 368, 262 };

	/**
	 * Top-left corner of the placed map in the world. Paper2D puts the centre of tile (0, 0) on the actor, so this
	 * is read from the tile map component after placing it, never assumed.
	 */
	FVector MapCorner(0.f, 0.f, MapHeightTiles * TileWorld);

	/** Sunny Land pixel position (y down, from the map's top-left corner) -> world. */
	FVector WorldAt(float PixelX, float PixelY, float Depth = 0.f)
	{
		return FVector(MapCorner.X + PixelX * CmPerPixel, Depth, MapCorner.Z - PixelY * CmPerPixel);
	}

	FVector TileCenter(float TileX, float TileY)
	{
		return WorldAt(TileX * TilePixels, TileY * TilePixels);
	}

	/** Builds the example step by step; each step reports failures through Error. */
	class FBuilder
	{
	public:
		explicit FBuilder(const FString& InPack) : Pack(InPack), Png(InPack / TEXT("PNG")) {}

		bool Run(FText& OutError);

	private:
		UTexture2D* ImportPng(const FString& File, const FString& Folder, const FString& Name);
		TArray<UTexture2D*> ImportFrames(const FString& RelativeDir, const FString& Prefix);
		UTexture2D* Strip(const TArray<UTexture2D*>& Frames, const FString& Name, FIntPoint& OutFrameSize);
		UPaperFlipbook* Flipbook(const TArray<UTexture2D*>& Frames, const FString& Name, float FramesPerSecond);
		UBlueprint* MakeEnemy(const FString& Name, const TMap<FName, UTexture2D*>& Strips, const TMap<FName, float>& Speeds, FIntPoint FrameSize,
			TFunctionRef<void(AOnyxCharacter&, UOnyxPatrolComponent&)> Setup);
		template <class T>
		T* Spawn(UWorld* World, const FVector& Location, const FString& Label);
		bool Fail(const FText& Reason) { Error = Reason; return false; }

		FString Pack;
		FString Png;
		FText Error;
		UPaperFlipbook* EnemyDeath = nullptr;
		TArray<UPackage*> Dirty;
	};

	UTexture2D* FBuilder::ImportPng(const FString& File, const FString& Folder, const FString& Name)
	{
		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = File;
		Task->DestinationPath = FString(Root) / Folder;
		Task->DestinationName = Name;
		Task->bAutomated = true;
		Task->bReplaceExisting = true;
		Task->bSave = true;
		FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().ImportAssetTasks({ Task });
		const FString Path = FString(Root) / Folder / Name;
		return LoadObject<UTexture2D>(nullptr, *(Path + TEXT(".") + Name));
	}

	TArray<UTexture2D*> FBuilder::ImportFrames(const FString& RelativeDir, const FString& Prefix)
	{
		const FString Directory = Png / TEXT("sprites") / RelativeDir;
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Directory / TEXT("*.png")), true, false);
		// "player-run-10.png" after "player-run-2.png".
		Files.Sort([](const FString& A, const FString& B)
		{
			auto Number = [](const FString& File) { return FCString::Atoi(*FPaths::GetBaseFilename(File).RightChop(FPaths::GetBaseFilename(File).Find(TEXT("-"), ESearchCase::IgnoreCase, ESearchDir::FromEnd) + 1)); };
			return Number(A) < Number(B);
		});
		TArray<UTexture2D*> Frames;
		for (const FString& File : Files)
		{
			if (UTexture2D* Frame = ImportPng(Directory / File, TEXT("Frames") / Prefix, FPaths::GetBaseFilename(File).Replace(TEXT("-"), TEXT("_"))))
			{
				Frames.Add(Frame);
			}
		}
		return Frames;
	}

	UTexture2D* FBuilder::Strip(const TArray<UTexture2D*>& Frames, const FString& Name, FIntPoint& OutFrameSize)
	{
		FText StripError;
		UTexture2D* Texture = FOnyxFramePacker::PackStrip(Frames, FString(Root) / TEXT("Strips"), Name, OutFrameSize, StripError);
		if (!Texture)
		{
			Error = StripError;
		}
		return Texture;
	}

	UPaperFlipbook* FBuilder::Flipbook(const TArray<UTexture2D*>& Frames, const FString& Name, float FramesPerSecond)
	{
		FIntPoint FrameSize;
		UTexture2D* Texture = Strip(Frames, TEXT("T_") + Name + TEXT("_Strip"), FrameSize);
		if (!Texture)
		{
			return nullptr;
		}
		TArray<FIntPoint> Cells;
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			Cells.Add(FIntPoint(Index, 0));
		}
		return UOnyxEditorLibrary::CreateFlipbookFromCells(Texture, FrameSize, Cells, Name, FString(Root) / TEXT("Effects"), CmPerPixel, FramesPerSecond);
	}

	/** Shows the timeline at work on the fox: a Footstep message twice per run cycle, invulnerable while hurt. */
	void AddFoxTimelineEvents(UBlueprint* Fox, TArray<UPackage*>& Dirty)
	{
		const AOnyxCharacter* Defaults = Fox && Fox->GeneratedClass ? Fox->GeneratedClass->GetDefaultObject<AOnyxCharacter>() : nullptr;
		UOnyxMoveset* Moveset = Defaults && Defaults->MovesetComponent ? Defaults->MovesetComponent->Moveset.Get() : nullptr;
		if (!Moveset)
		{
			return;
		}
		Moveset->AddMessage(TEXT("Footstep"));
		Dirty.Add(Moveset->GetPackage());
		if (UOnyxClip* Run = Moveset->FindClip(TEXT("Run")); Run && Run->Events.IsEmpty() && Run->GetFrameUnits() >= 4)
		{
			Run->Lanes[0].Name = TEXT("Gameplay");
			const float Frame = 1.f / Run->GetFramesPerSecond();
			for (const int32 StepFrame : { 1, Run->GetFrameUnits() / 2 + 1 })
			{
				if (UOnyxClipEvent_Message* Step = Cast<UOnyxClipEvent_Message>(Run->AddEvent(UOnyxClipEvent_Message::StaticClass(), StepFrame * Frame, 0)))
				{
					Step->Message = TEXT("Footstep");
				}
			}
			Run->MarkPackageDirty();
			Dirty.Add(Run->GetPackage());
		}
		if (UOnyxClip* Hurt = Moveset->FindClip(TEXT("Hurt")); Hurt && Hurt->Events.IsEmpty())
		{
			if (UOnyxClipWindow* Window = Cast<UOnyxClipWindow>(Hurt->AddEvent(UOnyxClipWindow_Invulnerable::StaticClass(), 0.f, 0)))
			{
				Window->Length = FMath::Max(Hurt->GetLength(), UOnyxClipWindow::ShortestLength);
			}
			Hurt->MarkPackageDirty();
			Dirty.Add(Hurt->GetPackage());
		}
	}

	UBlueprint* FBuilder::MakeEnemy(const FString& Name, const TMap<FName, UTexture2D*>& Strips, const TMap<FName, float>& Speeds, FIntPoint FrameSize,
		TFunctionRef<void(AOnyxCharacter&, UOnyxPatrolComponent&)> Setup)
	{
		// A plain character with the patrolling-foe behaviours (Patrol, Contact Damage, Stompable).
		UBlueprint* Blueprint = UOnyxEditorLibrary::CreateCharacterFromStrips(Strips, Name, FString(Root) / TEXT("Characters"), FrameSize,
			int32(UOnyxBehaviourLibrary::PatrolFoePreset), OnyxTeams::Enemy, CmPerPixel, Speeds);
		AOnyxCharacter* Enemy = Blueprint ? Cast<AOnyxCharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
		UOnyxPatrolComponent* Patrol = FOnyxAssetUtils::FindComponentTemplate<UOnyxPatrolComponent>(Blueprint);
		if (!Enemy || !Patrol)
		{
			Fail(FText::Format(INVTEXT("Could not create the {0} enemy."), FText::FromString(Name)));
			return nullptr;
		}
		Enemy->Modify();
		Patrol->Modify();
		Enemy->bArtFacesLeft = true;
		Enemy->DeathEffect = EnemyDeath;
		Enemy->Health->MaxHealth = 1.f;
		Setup(*Enemy, *Patrol);
		Blueprint->MarkPackageDirty();
		Dirty.Add(Blueprint->GetPackage());
		return Blueprint;
	}

	template <class T>
	T* FBuilder::Spawn(UWorld* World, const FVector& Location, const FString& Label)
	{
		T* Actor = World->SpawnActor<T>(Location, FRotator::ZeroRotator);
		if (Actor)
		{
			Actor->SetActorLabel(Label);
		}
		return Actor;
	}

	bool FBuilder::Run(FText& OutError)
	{
		FScopedSlowTask Progress(8.f, INVTEXT("Building the Sunny Land example"));
		Progress.MakeDialog();
		ON_SCOPE_EXIT { OutError = Error; };

		// --- Fox ---------------------------------------------------------------------------------
		Progress.EnterProgressFrame(1.f, INVTEXT("Fox"));
		TMap<FName, UTexture2D*> FoxStrips;
		TMap<FName, float> FoxSpeeds;
		FIntPoint FrameSize;
		struct FAnim { const TCHAR* Name; const TCHAR* Dir; const TCHAR* Prefix; float Fps; };
		for (const FAnim& Anim : { FAnim{ TEXT("Idle"), TEXT("player/idle"), TEXT("player_idle"), 8.f }, FAnim{ TEXT("Run"), TEXT("player/run"), TEXT("player_run"), 14.f },
			FAnim{ TEXT("Crouch"), TEXT("player/crouch"), TEXT("player_crouch"), 8.f }, FAnim{ TEXT("Hurt"), TEXT("player/hurt"), TEXT("player_hurt"), 12.f } })
		{
			FoxStrips.Add(Anim.Name, Strip(ImportFrames(Anim.Dir, Anim.Prefix), FString(TEXT("T_Fox_")) + Anim.Name, FrameSize));
			FoxSpeeds.Add(Anim.Name, Anim.Fps);
		}
		const TArray<UTexture2D*> Jump = ImportFrames(TEXT("player/jump"), TEXT("player_jump"));
		if (Jump.Num() < 2)
		{
			return Fail(INVTEXT("The pack's player jump frames are missing."));
		}
		FoxStrips.Add(TEXT("Jump"), Strip({ Jump[0] }, TEXT("T_Fox_Jump"), FrameSize));
		FoxStrips.Add(TEXT("Fall"), Strip({ Jump[1] }, TEXT("T_Fox_Fall"), FrameSize));
		// The hero: a plain character with Player Control, Camera and Respawn.
		UBlueprint* Fox = UOnyxEditorLibrary::CreateCharacterFromStrips(FoxStrips, TEXT("Fox"), FString(Root) / TEXT("Characters"), FrameSize,
			int32(UOnyxBehaviourLibrary::PlayerPreset), OnyxTeams::Player, CmPerPixel, FoxSpeeds);
		UOnyxCameraComponent* FoxCamera = FOnyxAssetUtils::FindComponentTemplate<UOnyxCameraComponent>(Fox);
		if (!FoxCamera)
		{
			return Fail(INVTEXT("Could not create the fox."));
		}
		FoxCamera->Modify();
		FOnyxCameraSettings& Camera = FoxCamera->Settings;
		Camera.ViewHeight = 900.f;        // about 14 tiles: the original game's framing
		Camera.bPixelPerfect = true;       // whole screen pixels per art pixel: no shimmer, no seams
		Camera.ArtPixelSize = CmPerPixel;
		Fox->MarkPackageDirty();
		Dirty.Add(Fox->GetPackage());
		AddFoxTimelineEvents(Fox, Dirty);

		// --- Enemies, items, effects ---------------------------------------------------------------
		Progress.EnterProgressFrame(1.f, INVTEXT("Enemies and items"));
		EnemyDeath = Flipbook(ImportFrames(TEXT("enemy-death"), TEXT("enemy_death")), TEXT("FB_EnemyDeath"), 16.f);
		UPaperFlipbook* ItemFeedback = Flipbook(ImportFrames(TEXT("item-feedback"), TEXT("item_feedback")), TEXT("FB_ItemFeedback"), 16.f);
		UPaperFlipbook* Cherry = Flipbook(ImportFrames(TEXT("cherry"), TEXT("cherry")), TEXT("FB_Cherry"), 12.f);
		UPaperFlipbook* Gem = Flipbook(ImportFrames(TEXT("gem"), TEXT("gem")), TEXT("FB_Gem"), 12.f);

		FIntPoint OpossumSize;
		UTexture2D* OpossumStrip = Strip(ImportFrames(TEXT("opossum"), TEXT("opossum")), TEXT("T_Opossum_Run"), OpossumSize);
		UBlueprint* Opossum = MakeEnemy(TEXT("Opossum"), { { TEXT("Idle"), OpossumStrip }, { TEXT("Run"), OpossumStrip } }, { { TEXT("Idle"), 12.f }, { TEXT("Run"), 12.f } }, OpossumSize,
			[](AOnyxCharacter& Enemy, UOnyxPatrolComponent& Patrol)
			{
				Patrol.Style = EOnyxPatrolStyle::Walk;
				Patrol.bChasePlayer = false;
				Patrol.PauseAtTurn = 0.f;
				Enemy.GetOnyxMovement()->RunSpeed = 240.f;
			});
		FIntPoint EagleSize;
		UTexture2D* EagleStrip = Strip(ImportFrames(TEXT("eagle"), TEXT("eagle")), TEXT("T_Eagle_Fly"), EagleSize);
		UBlueprint* Eagle = MakeEnemy(TEXT("Eagle"), { { TEXT("Idle"), EagleStrip } }, { { TEXT("Idle"), 12.f } }, EagleSize,
			[](AOnyxCharacter&, UOnyxPatrolComponent& Patrol)
			{
				Patrol.Style = EOnyxPatrolStyle::Fly;
				Patrol.FlyRange = FVector2D(0.f, 400.f);
				Patrol.FlyPeriod = 2.f;
				Patrol.bFacePlayer = true;
			});
		FIntPoint FrogSize;
		UTexture2D* FrogIdle = Strip(ImportFrames(TEXT("frog/idle"), TEXT("frog_idle")), TEXT("T_Frog_Idle"), FrogSize);
		const TArray<UTexture2D*> FrogJump = ImportFrames(TEXT("frog/jump"), TEXT("frog_jump"));
		if (FrogJump.Num() < 2)
		{
			return Fail(INVTEXT("The pack's frog jump frames are missing."));
		}
		FIntPoint Unused;
		UBlueprint* Frog = MakeEnemy(TEXT("Frog"),
			{ { TEXT("Idle"), FrogIdle }, { TEXT("Jump"), Strip({ FrogJump[0] }, TEXT("T_Frog_Jump"), Unused) }, { TEXT("Fall"), Strip({ FrogJump[1] }, TEXT("T_Frog_Fall"), Unused) } },
			{ { TEXT("Idle"), 6.f } }, FrogSize,
			[](AOnyxCharacter&, UOnyxPatrolComponent& Patrol)
			{
				Patrol.Style = EOnyxPatrolStyle::Hop;
				Patrol.HopInterval = 2.f;
				Patrol.HopVelocity = FVector2D(320.f, 700.f);
				Patrol.bAlternateHops = true;
			});
		if (!Opossum || !Eagle || !Frog || !Cherry || !Gem)
		{
			return Fail(Error.IsEmpty() ? INVTEXT("Could not create the enemies or items.") : Error);
		}

		// --- Tiles ---------------------------------------------------------------------------------
		Progress.EnterProgressFrame(1.f, INVTEXT("Tile set"));
		UTexture2D* TileSheet = ImportPng(Png / TEXT("environment/layers/tileset.png"), TEXT("Textures"), TEXT("T_SunnyLand_Tiles"));
		UPaperTileSet* TileSet = UOnyxEditorLibrary::CreateTileSet(TileSheet, FIntPoint(TilePixels, TilePixels), TEXT("TS_SunnyLand"), Root, false);
		if (!TileSet)
		{
			return Fail(INVTEXT("Could not create the tile set."));
		}
		TArray<int32> Solid, OneWay;
		for (const int32 Id : SolidIds)
		{
			if (!TArrayView<const int32>(OneWayIds).Contains(Id))
			{
				Solid.Add(Id - 1);
			}
		}
		for (const int32 Id : OneWayIds)
		{
			OneWay.Add(Id - 1);
		}
		UOnyxEditorLibrary::SetTileCollision(TileSet, Solid, EOnyxTileCollision::Solid);
		UOnyxEditorLibrary::SetTileCollision(TileSet, OneWay, EOnyxTileCollision::OneWay);
		// The grass ground template (3 x 3, laid out with a gap between tiles) becomes a terrain for auto-tiling.
		if (!UOnyxEditorLibrary::GetTerrainNames(TileSet).Contains(TEXT("Ground")))
		{
			UOnyxEditorLibrary::CreateTerrain(TileSet, FIntPoint(1, 1), FIntPoint(5, 5), TEXT("Ground"));
		}

		// --- Props, backgrounds ----------------------------------------------------------------------
		Progress.EnterProgressFrame(1.f, INVTEXT("Props and backgrounds"));
		struct FProp { const TCHAR* File; int32 PixelX; int32 PixelY; UPaperSprite* Sprite = nullptr; FIntPoint Size = FIntPoint::ZeroValue; };
		// The original places props by their top-left pixel.
		TArray<FProp> Props = { { TEXT("tree"), 31 * 16, 4 * 16 + 3 }, { TEXT("house"), 48 * 16, 3 * 16 + 5 }, { TEXT("bush"), 10 * 16, 8 * 16 + 4 },
			{ TEXT("sign"), 11 * 16, 19 * 16 - 4 }, { TEXT("skulls"), 15 * 16, 19 * 16 + 6 }, { TEXT("face-block"), 23 * 16, 19 * 16 }, { TEXT("shrooms"), 28 * 16, 20 * 16 } };
		for (FProp& Prop : Props)
		{
			const FString AssetName = FString(Prop.File).Replace(TEXT("-"), TEXT("_"));
			UTexture2D* Texture = ImportPng(Png / TEXT("environment/props") / (FString(Prop.File) + TEXT(".png")), TEXT("Props"), TEXT("T_") + AssetName);
			if (!Texture)
			{
				return Fail(FText::Format(INVTEXT("Could not import the {0} prop."), FText::FromString(Prop.File)));
			}
			Prop.Size = FIntPoint(Texture->Source.GetSizeX(), Texture->Source.GetSizeY());
			Prop.Sprite = UOnyxEditorLibrary::CreateSpriteFromCell(Texture, Prop.Size, FIntPoint::ZeroValue, TEXT("S_") + AssetName, FString(Root) / TEXT("Props"), CmPerPixel);
			UOnyxEditorLibrary::GenerateNormalMap(Texture);
		}
		UTexture2D* Back = ImportPng(Png / TEXT("environment/layers/back.png"), TEXT("Textures"), TEXT("T_SunnyLand_Back"));
		UTexture2D* Middle = ImportPng(Png / TEXT("environment/layers/middle.png"), TEXT("Textures"), TEXT("T_SunnyLand_Middle"));
		// Animated items and effects get normal maps like the rest of the lit art.
		for (const UPaperFlipbook* Look : { Cherry, Gem, ItemFeedback, EnemyDeath })
		{
			if (Look && Look->GetNumKeyFrames() > 0)
			{
				UOnyxEditorLibrary::GenerateNormalMap(Look->GetSpriteAtFrame(0)->GetSourceTexture());
			}
		}

		UOnyxLightingProfile* CaveMood = FOnyxAssetUtils::CreateOrReuse<UOnyxLightingProfile>(Root, TEXT("LP_SunnyLandCave"));
		CaveMood->Modify();
		CaveMood->Preset = EOnyxMoodPreset::Cave;
		CaveMood->Mood = FOnyxLightingMood::FromPreset(EOnyxMoodPreset::Cave);
		CaveMood->MarkPackageDirty();
		Dirty.Add(CaveMood->GetPackage());

		// Game mode that spawns the fox in this level only.
		UBlueprint* GameMode = FOnyxAssetUtils::CreateBlueprint(Root, TEXT("BP_SunnyLandGameMode"), AOnyxGameMode::StaticClass());
		if (!GameMode)
		{
			return Fail(INVTEXT("Could not create the Sunny Land game mode."));
		}
		CastChecked<AOnyxGameMode>(GameMode->GeneratedClass->GetDefaultObject())->DefaultPawnClass = Fox->GeneratedClass;
		GameMode->MarkPackageDirty();
		Dirty.Add(GameMode->GetPackage());
		FOnyxAssetUtils::SavePackages(Dirty);

		// --- Level: rebuilt from scratch in a blank map, then saved over the old one -------------------
		Progress.EnterProgressFrame(1.f, INVTEXT("Level"));
		UEditorLoadingAndSavingUtils::NewBlankMap(false);
		TArray<FAssetData> OldLevel;
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssetsByPackageName(FName(FOnyxSunnyLandExample::LevelPath), OldLevel);
		if (!OldLevel.IsEmpty())
		{
			ObjectTools::DeleteAssets(OldLevel, false);
		}
		UWorld* World = GEditor->GetEditorWorldContext().World();
		const FString MapJson = Pack / TEXT("code/sunny-land/assets/maps/map.json");
		if (!UOnyxEditorLibrary::ImportTiledMap(MapJson, TileSet, TEXT("TM_SunnyLand"), Root, CmPerPixel, true))
		{
			return Fail(INVTEXT("Could not import the Sunny Land Tiled map."));
		}
		for (TActorIterator<APaperTileMapActor> It(World); It; ++It)
		{
			MapCorner = It->GetRenderComponent()->GetTileCornerPosition(0, 0, 0, true);
		}
		FOnyxLevelTools::SetupCurrentLevel(); // game mode, player start, Onyx Lighting, tile collision
		World->GetWorldSettings()->DefaultGameMode = GameMode->GeneratedClass;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			It->SetActorLocation(TileCenter(54, 9)); // where the original game starts the fox
		}

		Progress.EnterProgressFrame(1.f, INVTEXT("Placing props, enemies and items"));
		for (const FProp& Prop : Props)
		{
			// Scenery goes on the Background layer, behind the tiles: grass and tile edges overlap its base.
			// Prop sprites pivot on their bottom centre (they stand on the ground).
			APaperSpriteActor* Actor = Spawn<APaperSpriteActor>(World,
				WorldAt(Prop.PixelX + Prop.Size.X * 0.5f, Prop.PixelY + Prop.Size.Y, FOnyxMapPainter::GetObjectLayerDepth(FOnyxBrushLibrary::BackgroundLayer)),
				FString(TEXT("Prop_")) + Prop.File);
			UPaperSpriteComponent* Component = Actor->GetRenderComponent();
			Component->SetMobility(EComponentMobility::Movable);
			Component->SetSprite(Prop.Sprite);
			UOnyx2DLibrary::ConfigureSpriteLighting(Component, false);
			Actor->Tags = { FName(TEXT("Onyx.Brush.") + Prop.Sprite->GetPathName()), FName(TEXT("Onyx.Layer.Background")) };
		}
		struct FPlacement { UBlueprint* Blueprint; float X; float Y; };
		for (const FPlacement& Placement : { FPlacement{ Frog, 15, 9 }, FPlacement{ Frog, 30, 20 }, FPlacement{ Eagle, 33, 6 }, FPlacement{ Eagle, 6, 7 },
			FPlacement{ Opossum, 42, 9 }, FPlacement{ Opossum, 23, 20 } })
		{
			AActor* Enemy = World->SpawnActor<AActor>(Placement.Blueprint->GeneratedClass, TileCenter(Placement.X, Placement.Y), FRotator::ZeroRotator);
			Enemy->SetActorLabel(FString::Printf(TEXT("%s_%d_%d"), *Placement.Blueprint->GetName(), int32(Placement.X), int32(Placement.Y)));
		}
		struct FItems { UPaperFlipbook* Look; int32 Amount; TArray<FIntPoint> Tiles; };
		for (const FItems& Items : { FItems{ Cherry, 1, { { 30, 5 }, { 31, 5 }, { 32, 5 }, { 23, 17 }, { 24, 17 }, { 25, 17 } } },
			FItems{ Gem, 5, { { 3, 6 }, { 4, 6 }, { 5, 6 }, { 44, 12 }, { 42, 13 }, { 42, 16 } } } })
		{
			for (const FIntPoint& At : Items.Tiles)
			{
				AOnyxPickup* Pickup = Spawn<AOnyxPickup>(World, TileCenter(At.X, At.Y), FString::Printf(TEXT("%s_%d_%d"), *Items.Look->GetName(), At.X, At.Y));
				Pickup->Flipbook = Items.Look;
				Pickup->CollectEffect = ItemFeedback;
				Pickup->Amount = Items.Amount;
				Pickup->BobHeight = 6.f;
				Pickup->RerunConstructionScripts();
			}
		}

		Progress.EnterProgressFrame(1.f, INVTEXT("Backgrounds and lighting"));
		// Parallax: sea and sky far away, jungle closer.
		AOnyxParallaxBackground* Background = Spawn<AOnyxParallaxBackground>(World, FVector(58 * TileWorld * 0.5f, 0.f, 800.f), TEXT("Parallax"));
		Background->BackgroundLayers.Reset();
		FOnyxParallaxLayer Sky;
		Sky.Name = TEXT("Sky and sea");
		Sky.Texture = Back;
		Sky.Factor = FVector2D(0.9f, 0.9f);
		Sky.Depth = 2500.f;
		Sky.Height = 2200.f;
		Background->BackgroundLayers.Add(Sky);
		FOnyxParallaxLayer Jungle;
		Jungle.Name = TEXT("Jungle");
		Jungle.Texture = Middle;
		Jungle.Factor = FVector2D(0.6f, 0.7f);
		Jungle.Depth = 1200.f;
		Jungle.Height = 1500.f;
		Jungle.OffsetZ = -200.f;
		Background->BackgroundLayers.Add(Jungle);
		Background->RerunConstructionScripts();

		for (TActorIterator<AOnyxLightingEnvironment> It(World); It; ++It)
		{
			It->Preset = EOnyxMoodPreset::Day;
			It->Mood = FOnyxLightingMood::FromPreset(EOnyxMoodPreset::Day);
		}
		// 2D lights: the cave's wall torches, a lantern by the house door, magic in the upper cave.
		struct FLight { const TCHAR* Label; EOnyxLightPreset Preset; FVector Location; };
		for (const FLight& Light : { FLight{ TEXT("Torch_1"), EOnyxLightPreset::Torch, TileCenter(17.5f, 18.5f) }, FLight{ TEXT("Torch_2"), EOnyxLightPreset::Torch, TileCenter(23.5f, 18.5f) },
			FLight{ TEXT("Torch_3"), EOnyxLightPreset::Torch, TileCenter(29.5f, 18.5f) }, FLight{ TEXT("Torch_4"), EOnyxLightPreset::Torch, TileCenter(36.5f, 18.5f) },
			FLight{ TEXT("Lantern_House"), EOnyxLightPreset::Lantern, WorldAt(48 * 16 + 30, 3 * 16 + 80) },
			FLight{ TEXT("Magic_UpperCave"), EOnyxLightPreset::Magic, TileCenter(18.5f, 8.f) } })
		{
			AOnyxLight2D* Actor = Spawn<AOnyxLight2D>(World, Light.Location, Light.Label);
			Actor->ApplyPreset(Light.Preset);
			Actor->RerunConstructionScripts();
		}
		// Underground: it gets dark when the player goes in (lighting blends, camera unchanged).
		AOnyxCameraBounds* Cave = Spawn<AOnyxCameraBounds>(World, TileCenter(25.f, 17.f), TEXT("Zone_Cave"));
		Cave->Area->SetBoxExtent(FVector(15.5f * TileWorld, 400.f, 4.5f * TileWorld));
		Cave->bConfine = false;
		Cave->Lighting = CaveMood;
		Cave->Priority = 1;

		// Music: the pack's looping track (CC0), played by the level's Onyx Level Audio.
		const FString MusicFile = Pack / TEXT("Sound/platformer_level03_loop.ogg");
		if (FPaths::FileExists(MusicFile))
		{
			UAssetImportTask* Task = NewObject<UAssetImportTask>();
			Task->Filename = MusicFile;
			Task->DestinationPath = FString(Root) / TEXT("Audio");
			Task->DestinationName = TEXT("SW_SunnyLand_Music");
			Task->bAutomated = true;
			Task->bReplaceExisting = true;
			Task->bSave = false;
			FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().ImportAssetTasks({ Task });
			if (USoundWave* Music = LoadObject<USoundWave>(nullptr, *(FString(Root) / TEXT("Audio/SW_SunnyLand_Music.SW_SunnyLand_Music"))))
			{
				Music->Modify();
				Music->bLooping = true;
				Music->MarkPackageDirty();
				FOnyxAssetUtils::SavePackages({ Music->GetPackage() });
				AOnyxLevelAudio* Audio = Spawn<AOnyxLevelAudio>(World, TileCenter(2.f, 2.f), TEXT("LevelAudio"));
				Audio->Music = Music;
				Audio->MusicVolume = 0.7f;
			}
		}

		Progress.EnterProgressFrame(1.f, INVTEXT("Saving"));
		if (!UEditorLoadingAndSavingUtils::SaveMap(World, FOnyxSunnyLandExample::LevelPath))
		{
			return Fail(INVTEXT("Could not save /Game/Maps/SunnyLand."));
		}
		return true;
	}
}

FString FOnyxSunnyLandExample::GetPackFolder()
{
	// The art (CC0) ships with the plugin, so the example builds in any project.
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Onyx2D"));
	return Plugin.IsValid() ? FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("Extras/SunnyLand")) : FString();
}

bool FOnyxSunnyLandExample::IsPackPresent()
{
	return FPaths::FileExists(GetPackFolder() / TEXT("code/sunny-land/assets/maps/map.json"));
}

UPaperSprite* FOnyxSunnyLandExample::ImportProp(const FString& Name)
{
	const FString AssetName = Name.Replace(TEXT("-"), TEXT("_"));
	const FString Folder = FString(Root) / TEXT("Props");
	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = GetPackFolder() / TEXT("PNG/environment/props") / (Name + TEXT(".png"));
	Task->DestinationPath = Folder;
	Task->DestinationName = TEXT("T_") + AssetName;
	Task->bAutomated = true;
	Task->bReplaceExisting = true;
	Task->bSave = true;
	FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().ImportAssetTasks({ Task });
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *(Folder / (TEXT("T_") + AssetName) + TEXT(".T_") + AssetName));
	if (!Texture)
	{
		return nullptr;
	}
	UPaperSprite* Sprite = UOnyxEditorLibrary::CreateSpriteFromCell(Texture, FIntPoint(Texture->Source.GetSizeX(), Texture->Source.GetSizeY()), FIntPoint::ZeroValue,
		TEXT("S_") + AssetName, Folder, CmPerPixel);
	UOnyxEditorLibrary::GenerateNormalMap(Texture);
	return Sprite;
}

bool FOnyxSunnyLandExample::Build(FText& OutError)
{
	if (!IsPackPresent())
	{
		OutError = FText::Format(INVTEXT("The Sunny Land art is missing from the plugin: {0} should contain it (CC0, opengameart.org/node/74142)."), FText::FromString(GetPackFolder()));
		return false;
	}
	FBuilder Builder(GetPackFolder());
	return Builder.Run(OutError);
}
