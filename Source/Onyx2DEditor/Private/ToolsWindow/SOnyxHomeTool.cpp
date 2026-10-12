// Copyright Onyx2D. All Rights Reserved.

#include "ToolsWindow/SOnyxHomeTool.h"
#include "AddToProjectConfig.h"
#include "AI/OnyxPatrolComponent.h"
#include "Level/OnyxLevelTools.h"
#include "Level/OnyxLevelValidator.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Lighting/OnyxLightingEnvironment.h"
#include "PaperFlipbook.h"
#include "PaperTileMapActor.h"
#include "PaperTileSet.h"
#include "Animation/OnyxClipEvent.h"
#include "Animation/OnyxClipWindow.h"
#include "Character/OnyxCharacter.h"
#include "Character/OnyxPlayerControlComponent.h"
#include "Core/OnyxGameMode.h"
#include "Core/OnyxPlayerController.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Examples/OnyxSunnyLandExample.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "GameProjectGenerationModule.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Lighting/OnyxLight2D.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Style/OnyxEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "ToolsWindow/OnyxToolWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "ToolsWindow/SOnyxToolsWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "World/OnyxCheckpoint.h"
#include "World/OnyxHazardVolume.h"
#include "World/OnyxLevelExit.h"
#include "World/OnyxMovingPlatform.h"
#include "World/OnyxPickup.h"

using namespace OnyxTools;

namespace
{
	FString PluginDir()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Onyx2D"));
		return Plugin.IsValid() ? FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()) : FString();
	}
}

void SOnyxHomeTool::Construct(const FArguments& InArgs)
{
	OnOpenToolDelegate = InArgs._OnOpenTool;
	Done.Init(false, 7);
	UpdateProgress(0.0, 0.f);
	RegisterActiveTimer(2.f, FWidgetActiveTimerDelegate::CreateSP(this, &SOnyxHomeTool::UpdateProgress));

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
		.Padding(0.f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot().HAlign(HAlign_Center).Padding(16.f, 12.f)
			[
				SNew(SBox).MaxDesiredWidth(900.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
					[
						MakeHeader()
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
					[
						Section(INVTEXT("Get Started: From Art to a Playable Level"), MakeSteps())
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Section(INVTEXT("Extend Onyx2D With Your Own Code"), MakeExtend())
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SOnyxHomeTool::Button(const FText& Label, const FText& Tooltip, TFunction<void()> OnClicked, bool bPrimary)
{
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), bPrimary ? "PrimaryButton" : "Button")
		.ToolTipText(Tooltip)
		.ContentPadding(FMargin(10.f, 3.f))
		.OnClicked_Lambda([OnClicked]() { OnClicked(); return FReply::Handled(); })
		[
			SNew(STextBlock).Text(Label)
		];
}

TSharedRef<SWidget> SOnyxHomeTool::MakeHeader()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
		[
			SNew(SImage).Image(FOnyxEditorStyle::Get().GetBrush("Onyx2D.Icon")).DesiredSizeOverride(FVector2D(48.f, 48.f))
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(INVTEXT("Onyx2D")).Font(Font(20, true))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(INVTEXT("A 2D game framework for Unreal: drop in art, paint a level, light it, play. Everything here is C++ you can extend."))
				.ColorAndOpacity(Muted()).AutoWrapText(true)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
		[
			Button(INVTEXT("README"), INVTEXT("Open the plugin documentation."), []()
			{
				FPlatformProcess::LaunchFileInDefaultExternalApplication(*(PluginDir() / TEXT("README.md")));
			})
		];
}

TSharedRef<SWidget> SOnyxHomeTool::MakeStep(int32 Number, const FText& Title, const FText& Description, const TSharedRef<SWidget>& Actions)
{
	const FLinearColor DoneColor(0.35f, 0.85f, 0.4f);
	// The next step to do is outlined; done steps show a check and DONE.
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor_Lambda([this, Number]() { return Number > 0 && GetNextStep() == Number ? Accent().CopyWithNewOpacity(0.55f) : FLinearColor::Transparent; })
		.Padding(1.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
			.Padding(FMargin(10.f, 8.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
				[
					SNew(SBox).WidthOverride(26.f).HeightOverride(26.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this, Number]()
						{
							return Number == 0 ? FText::FromString(TEXT("★")) : (IsDone(Number) ? FText::FromString(TEXT("✓")) : FText::AsNumber(Number));
						})
						.Font(Font(14, true))
						.ColorAndOpacity_Lambda([this, Number, DoneColor]() { return FSlateColor(IsDone(Number) ? DoneColor : Accent()); })
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock).Text(Title).Font(Font(10, true))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Font(Font(8, true))
							.Text_Lambda([this, Number]()
							{
								return IsDone(Number) ? INVTEXT("DONE") : (Number > 0 && GetNextStep() == Number ? INVTEXT("NEXT STEP") : FText::GetEmpty());
							})
							.ColorAndOpacity_Lambda([this, Number, DoneColor]() { return FSlateColor(IsDone(Number) ? DoneColor : Accent()); })
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock).Text(Description).ColorAndOpacity(Muted()).AutoWrapText(true)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
				[
					Actions
				]
			]
		];
}

TSharedRef<SWidget> SOnyxHomeTool::MakeSteps()
{
	auto Actions = [](std::initializer_list<TSharedRef<SWidget>> Buttons)
	{
		TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
		for (const TSharedRef<SWidget>& ButtonWidget : Buttons)
		{
			Box->AddSlot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f)[ButtonWidget];
		}
		return Box;
	};

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			MakeStep(1, INVTEXT("Set up the project (once)"),
				INVTEXT("Renderer settings for 2D and the Onyx game mode as the default."),
				Actions({ Button(INVTEXT("Configure Project"), INVTEXT("Configure Project for 2D"), []()
				{
					FOnyxLevelValidator::ConfigureProjectFor2D();
					Notify(INVTEXT("Project configured for 2D. Restart the editor for distance-field / shadow changes."), true);
				}) }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			MakeStep(2, INVTEXT("Bring in your art"),
				INVTEXT("Drop PNG files or art folders on the Import tab: tile sheets become tile sets, animations become characters, the rest sprites and backgrounds."),
				Actions({ Button(INVTEXT("Import Art"), INVTEXT("Open the Import tab"), [this]() { OpenTool(SOnyxToolsWindow::ImportTabId); }, true) }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			MakeStep(3, INVTEXT("Set up a level"),
				INVTEXT("Open or create a level, then add what a 2D level needs: game mode, player start, camera bounds, lighting, tile collision."),
				Actions({ Button(INVTEXT("Set Up Level"), INVTEXT("Set Up Level for 2D (safe to press again)"), []()
				{
					const TArray<FString> Added = FOnyxLevelTools::SetupCurrentLevel();
					Notify(Added.IsEmpty() ? INVTEXT("The level was already set up.") : FText::FromString(TEXT("Added: ") + FString::Join(Added, TEXT(", "))), true);
				}) }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			MakeStep(4, INVTEXT("Paint the level"),
				INVTEXT("Pick a tile set, press New Map and paint. The Entities layer places the player, enemies, pickups, checkpoints and the exit."),
				Actions({ Button(INVTEXT("Level Painter"), INVTEXT("Open the Level Painter"), [this]() { OpenTool(SOnyxToolsWindow::PainterTabId); }) }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			MakeStep(5, INVTEXT("Fill it with life"),
				INVTEXT("Preview your characters and animations in the Library and place them; add 2D lights, platforms, hazards and a parallax background."),
				Actions({ Button(INVTEXT("Library"), INVTEXT("Open the Library"), [this]() { OpenTool(SOnyxToolsWindow::LibraryTabId); }),
					Button(INVTEXT("Level"), INVTEXT("Open the Level tab"), [this]() { OpenTool(SOnyxToolsWindow::LevelTabId); }),
					Button(INVTEXT("Parallax"), INVTEXT("Open the Parallax tab"), [this]() { OpenTool(SOnyxToolsWindow::ParallaxTabId); }) }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			MakeStep(6, INVTEXT("Check and play"),
				INVTEXT("Check Level lists setup problems with one-click fixes. Then play it right in the editor."),
				Actions({ Button(INVTEXT("Check Level"), INVTEXT("Open the Level tab's checks"), [this]() { OpenTool(SOnyxToolsWindow::LevelTabId); }),
					Button(INVTEXT("Play"), INVTEXT("Play in the editor"), []()
					{
						if (GEditor)
						{
							GEditor->RequestPlaySession(FRequestPlaySessionParams());
						}
					}, true) }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			MakeStep(0, INVTEXT("Or see a finished level first"),
				INVTEXT("Builds the Sunny Land example (art shipped with the plugin): animated fox, enemies, pickups, the original map, 2D lighting and a torch-lit cave."),
				Actions({ Button(INVTEXT("Build Example"), INVTEXT("Build and open the Sunny Land example level"), []()
				{
					FText Error;
					if (FOnyxSunnyLandExample::Build(Error))
					{
						FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(FOnyxSunnyLandExample::LevelPath, FPackageName::GetMapPackageExtension()));
						Notify(INVTEXT("Sunny Land example built: press Play"), true);
					}
					else
					{
						Notify(Error, false);
					}
				}) }))
		];
}

TSharedRef<SWidget> SOnyxHomeTool::MakeBaseClassRow(UClass* Class, const FText& Label, const FText& Description)
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
		.Padding(FMargin(10.f, 5.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SImage).Image(FSlateIconFinder::FindIconBrushForClass(Class)).DesiredSizeOverride(FVector2D(16.f, 16.f))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock).Text(Label).Font(Font(10, true))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f).VAlign(VAlign_Bottom)
					[
						SNew(STextBlock).Text(FText::FromString(Class->GetPrefixCPP() + Class->GetName())).Font(Font(8)).ColorAndOpacity(Muted())
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Description).ColorAndOpacity(Muted()).AutoWrapText(true)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
			[
				Button(INVTEXT("New Blueprint"), FText::Format(INVTEXT("Create a Blueprint child of {0} in your Content folder and open it."), Label),
					[this, Class]() { CreateBlueprint(Class); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f, 0.f, 0.f)
			[
				Button(INVTEXT("New C++ Class"), FText::Format(INVTEXT("Add a C++ class deriving from {0} to your game module (Source/<YourProject>)."), Label),
					[this, Class]() { CreateCppClass(Class); })
			]
		];
}

TSharedRef<SWidget> SOnyxHomeTool::MakeExtend()
{
	auto Note = [](const FText& Title, const FText& Text)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(Title).Font(Font(10, true))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 8.f)
			[
				SNew(STextBlock).Text(Text).ColorAndOpacity(Muted()).AutoWrapText(true)
			];
	};

	TSharedRef<SVerticalBox> Classes = SNew(SVerticalBox);
	struct FBase { UClass* Class; FText Label; FText Description; };
	const FBase Bases[] = {
		{ AOnyxCharacter::StaticClass(), INVTEXT("Character"), INVTEXT("Movement, health, moveset and combat. What it does comes from behaviours (components). Override OnHurt and OnDied; Resolve Locomotion Clip on its Moveset component.") },
		{ UOnyxPlayerControlComponent::StaticClass(), INVTEXT("Player Control"), INVTEXT("A behaviour: change or extend how the player drives a character.") },
		{ UOnyxPatrolComponent::StaticClass(), INVTEXT("Patrol Behaviour"), INVTEXT("A behaviour: write a new movement pattern and give it to any character.") },
		{ UOnyxClipEvent::StaticClass(), INVTEXT("Clip Event"), INVTEXT("Your own timeline event (beside Message, Sound, Effect, Camera Shake): appears in the timeline's Add Event menu.") },
		{ UOnyxClipWindow::StaticClass(), INVTEXT("Clip Window"), INVTEXT("Your own timeline window, open for a span (beside Hitbox, Invulnerable, Loop Sound): Open, Update, Close.") },
		{ AOnyxPickup::StaticClass(), INVTEXT("Pickup"), INVTEXT("Coins, health, power-ups: override what happens when collected.") },
		{ AOnyxLight2D::StaticClass(), INVTEXT("2D Light"), INVTEXT("A light with presets, flicker and pulse: make a reusable light type (street lamp, lava glow).") },
		{ AOnyxHazardVolume::StaticClass(), INVTEXT("Hazard"), INVTEXT("Spikes, lava, saw blades: damage and knockback on touch.") },
		{ AOnyxMovingPlatform::StaticClass(), INVTEXT("Moving Platform"), INVTEXT("Waypoint platforms: elevators, falling platforms, conveyor tricks.") },
		{ AOnyxCheckpoint::StaticClass(), INVTEXT("Checkpoint"), INVTEXT("Sets the respawn point and heals.") },
		{ AOnyxLevelExit::StaticClass(), INVTEXT("Level Exit"), INVTEXT("Fades out and opens the next level.") },
		{ AOnyxGameMode::StaticClass(), INVTEXT("Game Mode"), INVTEXT("Rules of your game: lives, score, what happens on death.") },
		{ AOnyxPlayerController::StaticClass(), INVTEXT("Player Controller"), INVTEXT("Input and pause: your own bindings or extra actions.") },
	};
	for (const FBase& Base : Bases)
	{
		Classes->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)[MakeBaseClassRow(Base.Class, Base.Label, Base.Description)];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 8.f, 10.f, 0.f)
		[
			Note(INVTEXT("Blueprints (no C++ needed)"),
				INVTEXT("Make a child of an Onyx class below, change its settings in the Details panel and add logic in its Event Graph. "
					"It is saved in your project's Content folder and shows up in the Library tab for previewing and placing."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 0.f)
		[
			Note(INVTEXT("C++"),
				INVTEXT("Your C++ classes go in your game's own module (Source/<YourProject>/), deriving from the Onyx classes below; "
					"the dialog creates the files and compiles them. A Blueprint-only project gets a C++ module the first time you add a class. "
					"Don't edit the plugin's own code: keep your changes in your module so you can update Onyx2D without losing them."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 0.f)
		[
			Note(INVTEXT("Previewing what you make"),
				INVTEXT("The Library tab lists every character, enemy, tile set and animation in the project, plays it, and places it in the level "
					"in front of the camera. Press Play to try it in the level."))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Classes
		];
}

int32 SOnyxHomeTool::GetNextStep() const
{
	for (int32 Step = 1; Step < Done.Num(); ++Step)
	{
		if (!Done[Step])
		{
			return Step;
		}
	}
	return 0;
}

EActiveTimerReturnType SOnyxHomeTool::UpdateProgress(double CurrentTime, float DeltaTime)
{
	// 1. Project configured for 2D.
	Done[1] = FOnyxLevelValidator::GetProjectDifferences().IsEmpty();

	// 2. Art in the project: a tile set or an animation.
	bool bHasArt = false;
	FARFilter Filter;
	Filter.PackagePaths.Add(TEXT("/Game"));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths = { UPaperTileSet::StaticClass()->GetClassPathName(), UPaperFlipbook::StaticClass()->GetClassPathName() };
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().EnumerateAssets(Filter, [&bHasArt](const FAssetData&)
	{
		bHasArt = true;
		return false;
	});
	Done[2] = bHasArt;

	// 3-5. The open level: set up, painted, filled.
	UWorld* World = FOnyxLevelTools::GetEditorWorld();
	auto Has = [World](UClass* Class)
	{
		if (World)
		{
			for (TActorIterator<AActor> It(World, Class); It; ++It)
			{
				return true;
			}
		}
		return false;
	};
	Done[3] = Has(AOnyxLightingEnvironment::StaticClass()) && Has(APlayerStart::StaticClass());
	Done[4] = Has(APaperTileMapActor::StaticClass());
	bool bHasFoe = false;
	if (World)
	{
		for (TActorIterator<AOnyxCharacter> It(World); It && !bHasFoe; ++It)
		{
			bHasFoe = It->FindComponentByClass<UOnyxPatrolComponent>() != nullptr;
		}
	}
	Done[5] = Has(AOnyxLight2D::StaticClass()) || Has(AOnyxPickup::StaticClass()) || bHasFoe;

	// 6. The level check finds no errors (once the level is built).
	bool bClean = Done[3] && Done[4];
	for (const TSharedPtr<FOnyxValidationIssue>& Issue : bClean ? FOnyxLevelValidator::Validate() : TArray<TSharedPtr<FOnyxValidationIssue>>())
	{
		bClean &= Issue->Severity != EOnyxIssueSeverity::Error;
	}
	Done[6] = bClean;
	return EActiveTimerReturnType::Continue;
}

void SOnyxHomeTool::OpenTool(FName TabId) const
{
	OnOpenToolDelegate.ExecuteIfBound(TabId);
}

void SOnyxHomeTool::CreateBlueprint(UClass* ParentClass) const
{
	const FString Suggested = TEXT("BP_My") + ParentClass->GetName().Replace(TEXT("Onyx"), TEXT(""));
	if (UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromClass(
		FText::Format(INVTEXT("New Blueprint from {0}"), FText::FromString(ParentClass->GetName())), ParentClass, Suggested))
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Blueprint);
	}
}

void SOnyxHomeTool::CreateCppClass(UClass* ParentClass) const
{
	FGameProjectGenerationModule::Get().OpenAddCodeToProjectDialog(FAddToProjectConfig()
		.ParentClass(ParentClass)
		.ParentWindow(FSlateApplication::Get().FindWidgetWindow(AsShared())));
}
