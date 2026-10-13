// Copyright Onyx2D. All Rights Reserved.

#include "Level/OnyxLevelValidator.h"
#include "Core/Onyx2DTypes.h"
#include "Level/OnyxLevelTools.h"
#include "Builders/OnyxTileBuilder.h"
#include "PaperTileMapActor.h"
#include "World/OnyxTileCollisionComponent.h"
#include "Character/OnyxCharacter.h"
#include "Core/Onyx2DLibrary.h"
#include "Core/Onyx2DSettings.h"
#include "Core/OnyxGameMode.h"
#include "Editor.h"
#include "Builders/OnyxNormalMaps.h"
#include "Level/OnyxMapPainter.h"
#include "Builders/OnyxTilePadding.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Light.h"
#include "Engine/SkyLight.h"
#include "Lighting/OnyxLightingEnvironment.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PaperTileMap.h"
#include "PaperTileLayer.h"
#include "PaperTileSet.h"
#include "Engine/RendererSettings.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "GameMapsSettings.h"
#include "Materials/MaterialInterface.h"
#include "PaperFlipbookComponent.h"
#include "PaperSpriteComponent.h"
#include "PaperTileMapComponent.h"
#include "ScopedTransaction.h"
#include "World/OnyxCheckpoint.h"
#include "World/OnyxHazardVolume.h"
#include "World/OnyxLevelBuilder.h"
#include "World/OnyxLevelExit.h"
#include "World/OnyxMovingPlatform.h"
#include "World/OnyxOneWayPlatform.h"
#include "World/OnyxPickup.h"
#include "World/OnyxSign.h"
#include "Character/OnyxTalkComponent.h"
#include "Logic/OnyxAction.h"
#include "Logic/OnyxTriggerZone.h"

namespace
{
	constexpr float PlaneTolerance = 1.f;
	const TCHAR* ZonesDocs = TEXT("world/trigger-zones");
	const TCHAR* DialogueDocs = TEXT("world/signs-and-dialogue");

	TSharedPtr<FOnyxValidationIssue> AddIssue(TArray<TSharedPtr<FOnyxValidationIssue>>& Issues, EOnyxIssueSeverity Severity, const FText& Message,
		const FText& FixLabel = FText::GetEmpty(), TFunction<void()> Fix = nullptr, AActor* Actor = nullptr)
	{
		TSharedPtr<FOnyxValidationIssue> Issue = MakeShared<FOnyxValidationIssue>();
		Issue->Severity = Severity;
		Issue->Message = Message;
		Issue->FixLabel = FixLabel;
		Issue->Fix = MoveTemp(Fix);
		Issue->Actor = Actor;
		Issues.Add(Issue);
		return Issue;
	}

	bool IsGameplayActor(const AActor* Actor)
	{
		return Actor->IsA<AOnyxCharacter>() || Actor->IsA<APlayerStart>() || Actor->IsA<AOnyxPickup>() || Actor->IsA<AOnyxMovingPlatform>()
			|| Actor->IsA<AOnyxOneWayPlatform>() || Actor->IsA<AOnyxHazardVolume>() || Actor->IsA<AOnyxCheckpoint>() || Actor->IsA<AOnyxLevelExit>();
	}

	bool UsesUnlitMaterial(const UMeshComponent* Component)
	{
		const UMaterialInterface* Material = Component->GetMaterial(0);
		return Material && Material->GetName().Contains(TEXT("Unlit"));
	}

	/** Paper2D's stock lit materials light sprites but ignore their normal maps. */
	bool UsesStockLitMaterial(const UMeshComponent* Component)
	{
		const UMaterialInterface* Material = Component->GetMaterial(0);
		return Material && Material->GetPathName().StartsWith(TEXT("/Paper2D/")) && !UsesUnlitMaterial(Component);
	}

	/** Sprites / tile sets drawn by a Paper2D component (the assets that carry normal maps). */
	void GetArtAssets(UMeshComponent* Component, TArray<UObject*>& OutAssets)
	{
		if (UPaperSpriteComponent* Sprite = Cast<UPaperSpriteComponent>(Component))
		{
			OutAssets.Add(Sprite->GetSprite());
		}
		else if (UPaperFlipbookComponent* Flipbook = Cast<UPaperFlipbookComponent>(Component))
		{
			if (Flipbook->GetFlipbook() && Flipbook->GetFlipbook()->GetNumKeyFrames() > 0)
			{
				OutAssets.Add(Flipbook->GetFlipbook()->GetSpriteAtFrame(0));
			}
		}
		else if (const UPaperTileMapComponent* TileMapComponent = Cast<UPaperTileMapComponent>(Component))
		{
			const UPaperTileMap* Map = TileMapComponent->TileMap;
			for (const UPaperTileLayer* Layer : Map ? Map->TileLayers : TArray<TObjectPtr<UPaperTileLayer>>())
			{
				for (int32 Y = 0; Y < Map->MapHeight; ++Y)
				{
					for (int32 X = 0; X < Map->MapWidth; ++X)
					{
						if (UPaperTileSet* TileSet = Layer->GetCell(X, Y).TileSet)
						{
							OutAssets.AddUnique(TileSet);
						}
					}
				}
			}
		}
		OutAssets.Remove(nullptr);
	}

	UTexture2D* SheetOf(const UObject* Asset)
	{
		if (const UPaperSprite* Sprite = Cast<UPaperSprite>(Asset))
		{
			return Sprite->GetSourceTexture();
		}
		const UPaperTileSet* TileSet = Cast<UPaperTileSet>(Asset);
		return TileSet ? TileSet->GetTileSheetTexture() : nullptr;
	}

	bool IsOnyxGameMode(const UClass* Class)
	{
		return Class && Class->IsChildOf(AOnyxGameMode::StaticClass());
	}

	void NotifyRendererChange(URendererSettings* Settings, FName PropertyName)
	{
		if (FProperty* Property = FindFProperty<FProperty>(URendererSettings::StaticClass(), PropertyName))
		{
			FPropertyChangedEvent Event(Property);
			Settings->PostEditChangeProperty(Event);
		}
	}
}

TArray<FString> FOnyxLevelValidator::GetProjectDifferences()
{
	TArray<FString> Differences;
	const URendererSettings* Renderer = GetDefault<URendererSettings>();
	if (Renderer->DynamicGlobalIllumination != EDynamicGlobalIlluminationMethod::None)
	{
		Differences.Add(TEXT("Dynamic GI (Lumen) is on"));
	}
	if (Renderer->Reflections != EReflectionMethod::None)
	{
		Differences.Add(TEXT("Lumen/SSR reflections are on"));
	}
	if (Renderer->ShadowMapMethod != EShadowMapMethod::ShadowMaps)
	{
		Differences.Add(TEXT("Virtual shadow maps are on"));
	}
	if (Renderer->bGenerateMeshDistanceFields)
	{
		Differences.Add(TEXT("Mesh distance fields are on"));
	}
	if (Renderer->bDefaultFeatureMotionBlur)
	{
		Differences.Add(TEXT("Motion blur is on"));
	}
	if (Renderer->bDefaultFeatureAutoExposure)
	{
		Differences.Add(TEXT("Auto exposure is on"));
	}
	if (Renderer->DefaultFeatureAntiAliasing != AAM_None && Renderer->DefaultFeatureAntiAliasing != AAM_FXAA)
	{
		Differences.Add(TEXT("Temporal anti-aliasing blurs pixel art"));
	}
	if (!IsOnyxGameMode(FSoftClassPath(UGameMapsSettings::GetGlobalDefaultGameMode()).TryLoadClass<AGameModeBase>()))
	{
		Differences.Add(TEXT("Default game mode is not Onyx Game Mode"));
	}
	return Differences;
}

void FOnyxLevelValidator::ConfigureProjectFor2D()
{
	URendererSettings* Renderer = GetMutableDefault<URendererSettings>();
	Renderer->DynamicGlobalIllumination = EDynamicGlobalIlluminationMethod::None;
	Renderer->Reflections = EReflectionMethod::None;
	Renderer->ShadowMapMethod = EShadowMapMethod::ShadowMaps;
	Renderer->bGenerateMeshDistanceFields = false;
	Renderer->bDefaultFeatureMotionBlur = false;
	Renderer->bDefaultFeatureAutoExposure = false;
	Renderer->DefaultFeatureAntiAliasing = AAM_None;
	for (const FName Name : { GET_MEMBER_NAME_CHECKED(URendererSettings, DynamicGlobalIllumination), GET_MEMBER_NAME_CHECKED(URendererSettings, Reflections),
		GET_MEMBER_NAME_CHECKED(URendererSettings, ShadowMapMethod), FName(TEXT("bGenerateMeshDistanceFields")), FName(TEXT("bDefaultFeatureMotionBlur")),
		FName(TEXT("bDefaultFeatureAutoExposure")), GET_MEMBER_NAME_CHECKED(URendererSettings, DefaultFeatureAntiAliasing) })
	{
		NotifyRendererChange(Renderer, Name);
	}
	Renderer->TryUpdateDefaultConfigFile();

	UGameMapsSettings::SetGlobalDefaultGameMode(AOnyxGameMode::StaticClass()->GetPathName());
	GetMutableDefault<UGameMapsSettings>()->TryUpdateDefaultConfigFile();
}

TArray<TSharedPtr<FOnyxValidationIssue>> FOnyxLevelValidator::Validate()
{
	TArray<TSharedPtr<FOnyxValidationIssue>> Issues;
	UWorld* World = FOnyxLevelTools::GetEditorWorld();
	if (!World)
	{
		return Issues;
	}

	const auto SetupLevel = []() { FOnyxLevelTools::SetupCurrentLevel(); };

	// --- Project ---------------------------------------------------------------
	const TArray<FString> ProjectDifferences = GetProjectDifferences();
	if (!ProjectDifferences.IsEmpty())
	{
		AddIssue(Issues, EOnyxIssueSeverity::Warning,
			FText::Format(INVTEXT("Project is not tuned for 2D: {0}."), FText::FromString(FString::Join(ProjectDifferences, TEXT(", ")))),
			INVTEXT("Configure"), []() { ConfigureProjectFor2D(); });
	}
	if (UOnyx2DSettings::Get()->DefaultPlayerClass.IsNull())
	{
		AddIssue(Issues, EOnyxIssueSeverity::Info, INVTEXT("No Default Player Class in Project Settings > Onyx2D: the grey placeholder hero will spawn."));
	}

	// --- Level basics ------------------------------------------------------------
	const AWorldSettings* WorldSettings = World->GetWorldSettings();
	const UClass* LevelGameMode = WorldSettings ? WorldSettings->DefaultGameMode.Get() : nullptr;
	const bool bProjectUsesOnyx = IsOnyxGameMode(FSoftClassPath(UGameMapsSettings::GetGlobalDefaultGameMode()).TryLoadClass<AGameModeBase>());
	if (LevelGameMode ? !IsOnyxGameMode(LevelGameMode) : !bProjectUsesOnyx)
	{
		AddIssue(Issues, EOnyxIssueSeverity::Error, INVTEXT("The level does not use Onyx Game Mode, so the Onyx player and HUD will not spawn."),
			INVTEXT("Fix"), SetupLevel);
	}

	bool bHasStart = TActorIterator<APlayerStart>(World) ? true : false;
	for (TActorIterator<AOnyxLevelBuilder> It(World); It && !bHasStart; ++It)
	{
		bHasStart = It->Layout.Contains(TEXT("P"));
	}
	if (!bHasStart)
	{
		AddIssue(Issues, EOnyxIssueSeverity::Error, INVTEXT("No Player Start: the player spawns at the world origin."), INVTEXT("Add"), SetupLevel);
	}
	for (TActorIterator<APaperTileMapActor> It(World); It; ++It)
	{
		if (!It->FindComponentByClass<UOnyxTileCollisionComponent>())
		{
			TWeakObjectPtr<APaperTileMapActor> Weak = *It;
			AddIssue(Issues, EOnyxIssueSeverity::Error,
				FText::Format(INVTEXT("{0} has no Onyx Tile Collision: characters fall through its tiles and the camera can't confine to it."), FText::FromString(It->GetActorLabel())),
				INVTEXT("Add"), [Weak]()
				{
					const FScopedTransaction Transaction(INVTEXT("Add Onyx Tile Collision"));
					FOnyxTileBuilder::EnsureTileCollision(Weak.Get());
				}, *It);
		}
	}
	AOnyxLightingEnvironment* Environment = TActorIterator<AOnyxLightingEnvironment>(World) ? *TActorIterator<AOnyxLightingEnvironment>(World) : nullptr;
	if (!Environment && !TActorIterator<ALight>(World))
	{
		AddIssue(Issues, EOnyxIssueSeverity::Warning, INVTEXT("No lights: lit sprites will render black."), INVTEXT("Add Onyx Lighting"), SetupLevel);
	}
	TArray<TWeakObjectPtr<AActor>> ExtraLights;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->IsA<ADirectionalLight>() || It->IsA<ASkyLight>())
		{
			ExtraLights.Add(*It);
		}
	}
	if (Environment && !ExtraLights.IsEmpty())
	{
		AddIssue(Issues, EOnyxIssueSeverity::Warning,
			FText::Format(INVTEXT("Onyx Lighting already has a key light and ambient light, but {0} other sun / sky light(s) light the level too: everything is lit twice and moods can't darken it."), ExtraLights.Num()),
			INVTEXT("Remove extras"), [ExtraLights]()
			{
				const FScopedTransaction Transaction(INVTEXT("Remove Extra Lights"));
				for (const TWeakObjectPtr<AActor>& Light : ExtraLights)
				{
					if (AActor* Target = Light.Get())
					{
						Target->GetWorld()->EditorDestroyActor(Target, true);
					}
				}
			});
	}
	else if (!Environment && !ExtraLights.IsEmpty())
	{
		AddIssue(Issues, EOnyxIssueSeverity::Info,
			INVTEXT("Lit by plain sun / sky lights: use Onyx Lighting for moods (Day, Dusk, Night, Cave...), depth fog, fixed exposure and zone blending."),
			INVTEXT("Replace"), [ExtraLights]()
			{
				const FScopedTransaction Transaction(INVTEXT("Use Onyx Lighting"));
				for (const TWeakObjectPtr<AActor>& Light : ExtraLights)
				{
					if (AActor* Target = Light.Get())
					{
						Target->GetWorld()->EditorDestroyActor(Target, true);
					}
				}
				FOnyxLevelTools::SetupCurrentLevel();
			});
	}

	// --- Per actor ---------------------------------------------------------------
	int32 Coins = 0;
	TSet<FName> GivenItems;
	TArray<const AOnyxTriggerZone*> ZonesNeedingItems;
	// Actions that aren't set up yet ("Teleport has no destination"), and the items Give actions hand out.
	auto CheckActions = [&Issues, &GivenItems](const TArray<TObjectPtr<UOnyxAction>>& Actions, AActor* Owner, const FText& Label, const TCHAR* Docs)
	{
		for (const UOnyxAction* Action : Actions)
		{
			if (!Action)
			{
				continue;
			}
			const FText Problem = Action->CheckSetup();
			if (!Problem.IsEmpty())
			{
				AddIssue(Issues, EOnyxIssueSeverity::Warning, FText::Format(INVTEXT("{0}: {1}"), Label, Problem), FText::GetEmpty(), nullptr, Owner)->DocsPage = Docs;
			}
			if (const UOnyxAction_Give* Give = Cast<UOnyxAction_Give>(Action); Give && Give->What == EOnyxGiveWhat::Item)
			{
				GivenItems.Add(Give->Item);
			}
		}
	};
	TMap<UTexture2D*, FText> SheetsWithoutNormals;
	TSet<UPaperTileSet*> UnpaddedTileSets;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		const FText Label = FText::FromString(Actor->GetActorLabel());

		if (IsGameplayActor(Actor) && FMath::Abs(Actor->GetActorLocation().Y) > PlaneTolerance)
		{
			TWeakObjectPtr<AActor> Weak = Actor;
			AddIssue(Issues, EOnyxIssueSeverity::Warning,
				FText::Format(INVTEXT("{0} is off the gameplay plane (Y = {1}): characters cannot reach it."), Label, FText::AsNumber(FMath::RoundToInt(Actor->GetActorLocation().Y))),
				INVTEXT("Snap to Y = 0"), [Weak]()
				{
					if (AActor* Target = Weak.Get())
					{
						const FScopedTransaction Transaction(INVTEXT("Snap to Gameplay Plane"));
						Target->Modify();
						FVector Location = Target->GetActorLocation();
						Location.Y = 0.f;
						Target->SetActorLocation(Location);
					}
				}, Actor);
		}

		TInlineComponentArray<UMeshComponent*> Meshes(Actor);
		for (UMeshComponent* Mesh : Meshes)
		{
			const bool bPaper = Mesh->IsA<UPaperSpriteComponent>() || Mesh->IsA<UPaperFlipbookComponent>() || Mesh->IsA<UPaperTileMapComponent>();
			if (bPaper && !UsesUnlitMaterial(Mesh))
			{
				TArray<UObject*> ArtAssets;
				GetArtAssets(Mesh, ArtAssets);
				for (const UObject* Art : ArtAssets)
				{
					UTexture2D* Sheet = SheetOf(Art);
					if (Sheet && !FOnyxNormalMaps::GetAssigned(Art) && !SheetsWithoutNormals.Contains(Sheet))
					{
						SheetsWithoutNormals.Add(Sheet, Label);
					}
					if (const UPaperTileSet* TileSet = Cast<UPaperTileSet>(Art); TileSet && !FOnyxTilePadding::IsPadded(TileSet))
					{
						UnpaddedTileSets.Add(const_cast<UPaperTileSet*>(TileSet));
					}
				}
			}
			if (bPaper && UsesStockLitMaterial(Mesh))
			{
				TWeakObjectPtr<UMeshComponent> Weak = Mesh;
				AddIssue(Issues, EOnyxIssueSeverity::Info,
					FText::Format(INVTEXT("{0} uses Paper2D's lit material: it ignores normal maps, so lights shade it flat."), Label),
					INVTEXT("Use Onyx lit"), [Weak]()
					{
						if (UMeshComponent* Target = Weak.Get())
						{
							const FScopedTransaction Transaction(INVTEXT("Configure Sprite Lighting"));
							Target->Modify();
							UOnyx2DLibrary::ConfigureSpriteLighting(Target, Target->CastShadow);
						}
					}, Actor);
				break;
			}
			if (bPaper && UsesUnlitMaterial(Mesh))
			{
				TWeakObjectPtr<UMeshComponent> Weak = Mesh;
				AddIssue(Issues, EOnyxIssueSeverity::Info,
					FText::Format(INVTEXT("{0} uses an unlit sprite material: it ignores lights and casts no shadow."), Label),
					INVTEXT("Make lit"), [Weak]()
					{
						if (UMeshComponent* Target = Weak.Get())
						{
							const FScopedTransaction Transaction(INVTEXT("Configure Sprite Lighting"));
							Target->Modify();
							UOnyx2DLibrary::ConfigureSpriteLighting(Target);
						}
					}, Actor);
				break;
			}
		}

		if (const AOnyxLevelExit* Exit = Cast<AOnyxLevelExit>(Actor); Exit && Exit->NextLevel.IsNull())
		{
			AddIssue(Issues, EOnyxIssueSeverity::Info, FText::Format(INVTEXT("{0} has no Next Level: it restarts the current level."), Label), FText::GetEmpty(), nullptr, Actor);
		}
		if (const AOnyxPickup* Pickup = Cast<AOnyxPickup>(Actor); Pickup && Pickup->Type == EOnyxPickupType::Coin)
		{
			Coins += Pickup->Amount;
		}
		if (const AOnyxPickup* Pickup = Cast<AOnyxPickup>(Actor); Pickup && Pickup->Type == EOnyxPickupType::Item)
		{
			GivenItems.Add(Pickup->ItemName);
		}

		if (const AOnyxTriggerZone* Zone = Cast<AOnyxTriggerZone>(Actor))
		{
			const bool bHasActions = Zone->Actions.ContainsByPredicate([](const UOnyxAction* Action) { return Action != nullptr; });
			if (!bHasActions)
			{
				AddIssue(Issues, EOnyxIssueSeverity::Warning,
					FText::Format(INVTEXT("{0} has no Actions, so it does nothing yet. Select it and add some in Details > Zone > Actions."), Label),
					FText::GetEmpty(), nullptr, Actor)->DocsPage = ZonesDocs;
			}
			CheckActions(Zone->Actions, Actor, Label, ZonesDocs);
			CheckActions(Zone->OtherwiseActions, Actor, Label, ZonesDocs);
			if (Zone->Needs == EOnyxTriggerNeeds::Item)
			{
				ZonesNeedingItems.Add(Zone);
			}
		}
		if (const AOnyxSign* Sign = Cast<AOnyxSign>(Actor))
		{
			const bool bHasText = Sign->Pages.ContainsByPredicate([](const FText& Page) { return !Page.IsEmpty(); });
			if (!bHasText)
			{
				AddIssue(Issues, EOnyxIssueSeverity::Warning, FText::Format(INVTEXT("{0} has nothing written on it: add Pages in Details > Sign."), Label),
					FText::GetEmpty(), nullptr, Actor)->DocsPage = DialogueDocs;
			}
			CheckActions(Sign->AfterReading, Actor, Label, DialogueDocs);
		}
		TArray<UOnyxTalkComponent*> Talkers;
		Actor->GetComponents(Talkers);
		for (const UOnyxTalkComponent* Talk : Talkers)
		{
			const bool bHasText = Talk->Lines.ContainsByPredicate([](const FText& Line) { return !Line.IsEmpty(); });
			if (!bHasText)
			{
				AddIssue(Issues, EOnyxIssueSeverity::Warning, FText::Format(INVTEXT("{0} can be talked to but has no Lines: add them in its Talk component."), Label),
					FText::GetEmpty(), nullptr, Actor)->DocsPage = DialogueDocs;
			}
			CheckActions(Talk->AfterFirstTalk, Actor, Label, DialogueDocs);
		}
	}
	for (const AOnyxTriggerZone* Zone : ZonesNeedingItems)
	{
		if (!GivenItems.Contains(Zone->NeededItem))
		{
			AddIssue(Issues, EOnyxIssueSeverity::Info,
				FText::Format(INVTEXT("{0} needs {1} {2}, but nothing in this level gives one (an Item pickup named {2}, or a Give action). Fine if the player brings it from an earlier level."),
					FText::FromString(Zone->GetActorLabel()), Zone->NeededAmount, FText::FromName(Zone->NeededItem)),
				FText::GetEmpty(), nullptr, const_cast<AOnyxTriggerZone*>(Zone))->DocsPage = ZonesDocs;
		}
	}
	for (UPaperTileSet* TileSet : UnpaddedTileSets)
	{
		TWeakObjectPtr<UPaperTileSet> Weak = TileSet;
		AddIssue(Issues, EOnyxIssueSeverity::Warning,
			FText::Format(INVTEXT("{0} isn't padded: thin lines can flash between tiles while the camera moves."), FText::FromString(TileSet->GetName())),
			INVTEXT("Pad tiles"), [Weak]()
			{
				FOnyxTilePadding::PadTileSet(Weak.Get());
				FOnyxMapPainter::RebuildLevelCollision();
			});
	}
	for (const TPair<UTexture2D*, FText>& Entry : SheetsWithoutNormals)
	{
		TWeakObjectPtr<UTexture2D> Sheet = Entry.Key;
		AddIssue(Issues, EOnyxIssueSeverity::Info,
			FText::Format(INVTEXT("{0} (used by {1}) has no normal map: lights shade it flat instead of in relief."), FText::FromString(Entry.Key->GetName()), Entry.Value),
			INVTEXT("Generate"), [Sheet]()
			{
				FOnyxNormalMaps::GenerateForSheet(Sheet.Get(), FOnyxNormalMapSettings());
			});
	}

	if (Issues.IsEmpty())
	{
		AddIssue(Issues, EOnyxIssueSeverity::Ok, INVTEXT("No problems found. The level is ready to play."));
	}
	if (Coins > 0)
	{
		AddIssue(Issues, EOnyxIssueSeverity::Info, FText::Format(INVTEXT("{0} coins can be collected in this level."), Coins));
	}
	return Issues;
}
