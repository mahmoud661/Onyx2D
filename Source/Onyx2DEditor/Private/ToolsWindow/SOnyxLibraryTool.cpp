// Copyright Onyx2D. All Rights Reserved.

#include "ToolsWindow/SOnyxLibraryTool.h"
#include "Animation/OnyxMovesetComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetThumbnail.h"
#include "Level/OnyxLevelTools.h"
#include "Character/OnyxCharacter.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxMoveset.h"
#include "Core/Onyx2DSettings.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookActor.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "PaperTileMap.h"
#include "PaperTileSet.h"
#include "ScopedTransaction.h"
#include "SourceCodeNavigation.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWrapBox.h"
#include "ToolsWindow/OnyxToolWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOnyxFlipbookPreview.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

using namespace OnyxTools;

namespace
{
	const TCHAR* RuntimePackage = TEXT("/Script/Onyx2DRuntime");

	/** The first class up the chain that Onyx2D defines (null when the class isn't built on Onyx2D). */
	const UClass* FindOnyxBase(const UClass* Class)
	{
		for (; Class; Class = Class->GetSuperClass())
		{
			if (Class->GetOutermost()->GetName() == RuntimePackage)
			{
				return Class;
			}
		}
		return nullptr;
	}

	/** Character, enemy or other placeable Onyx actor; All when it isn't one (game modes, controllers: nothing to place). */
	EOnyxLibraryCategory ClassCategory(const UClass* Class)
	{
		if (!Class || !FindOnyxBase(Class) || !Class->IsChildOf(AActor::StaticClass()) || Class->IsChildOf(AGameModeBase::StaticClass())
			|| Class->IsChildOf(AController::StaticClass()) || Class->IsChildOf(AGameStateBase::StaticClass()) || Class->IsChildOf(APlayerState::StaticClass()))
		{
			return EOnyxLibraryCategory::All;
		}
		return Class->IsChildOf(AOnyxCharacter::StaticClass()) ? EOnyxLibraryCategory::Character : EOnyxLibraryCategory::Object;
	}

	const EOnyxLibraryCategory Categories[] = { EOnyxLibraryCategory::All, EOnyxLibraryCategory::Character,
		EOnyxLibraryCategory::Object, EOnyxLibraryCategory::TileSet, EOnyxLibraryCategory::TileMap, EOnyxLibraryCategory::Animation };
}

FText SOnyxLibraryTool::CategoryName(EOnyxLibraryCategory InCategory)
{
	switch (InCategory)
	{
	case EOnyxLibraryCategory::Character: return INVTEXT("Characters");
	case EOnyxLibraryCategory::Object: return INVTEXT("Objects");
	case EOnyxLibraryCategory::TileSet: return INVTEXT("Tile Sets");
	case EOnyxLibraryCategory::TileMap: return INVTEXT("Tile Maps");
	case EOnyxLibraryCategory::Animation: return INVTEXT("Movesets");
	default: return INVTEXT("All");
	}
}

void SOnyxLibraryTool::Construct(const FArguments& InArgs)
{
	OnEditAnimations = InArgs._OnEditAnimations;
	OnPaintTileMap = InArgs._OnPaintTileMap;
	OnPaintTileSet = InArgs._OnPaintTileSet;
	ThumbnailPool = MakeShared<FAssetThumbnailPool>(256);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			ToolbarStrip(MakeToolbar(), MakeCategoryBar())
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(SSplitter)
			+ SSplitter::Slot().Value(0.62f)
			[
				Section(INVTEXT("Project"),
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SAssignNew(TileView, STileView<FEntryPtr>)
						.ListItemsSource(&Filtered)
						.ItemWidth(112.f)
						.ItemHeight(132.f)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateTile(this, &SOnyxLibraryTool::MakeTile)
						.OnSelectionChanged_Lambda([this](FEntryPtr Entry, ESelectInfo::Type) { Select(Entry); })
						.OnMouseButtonDoubleClick_Lambda([this](FEntryPtr Entry) { Select(Entry); OpenSelected(); })
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(380.f)
						.Visibility_Lambda([this]() { return Filtered.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						[
							Hint(INVTEXT("Nothing here yet. Import art in the Import tab, make Blueprints or C++ classes from the Home tab, "
								"or build the Sunny Land example: everything Onyx-ready in the project shows up here."))
						]
					])
			]
			+ SSplitter::Slot().Value(0.38f)
			[
				Section(TAttribute<FText>::CreateLambda([this]() { return Selected.IsValid() ? FText::FromString(Selected->Name) : INVTEXT("Preview"); }),
					MakePreviewPanel())
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			StatusBar(TAttribute<FText>::CreateLambda([this]()
			{
				return FText::Format(INVTEXT("{0} of {1} shown. Double-click to open; Place in Level puts it in front of the camera."), Filtered.Num(), Entries.Num());
			}))
		]
	];

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AddedHandle = Registry.OnAssetAdded().AddLambda([this](const FAssetData&) { MarkDirty(); });
	RemovedHandle = Registry.OnAssetRemoved().AddLambda([this](const FAssetData&) { MarkDirty(); });
	RenamedHandle = Registry.OnAssetRenamed().AddLambda([this](const FAssetData&, const FString&) { MarkDirty(); });
	RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SOnyxLibraryTool::CheckDirty));

	Refresh();
}

SOnyxLibraryTool::~SOnyxLibraryTool()
{
	if (FAssetRegistryModule* Module = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
	{
		IAssetRegistry& Registry = Module->Get();
		Registry.OnAssetAdded().Remove(AddedHandle);
		Registry.OnAssetRemoved().Remove(RemovedHandle);
		Registry.OnAssetRenamed().Remove(RenamedHandle);
	}
}

EActiveTimerReturnType SOnyxLibraryTool::CheckDirty(double CurrentTime, float DeltaTime)
{
	if (bDirty)
	{
		bDirty = false;
		Refresh();
	}
	return EActiveTimerReturnType::Continue;
}

TSharedRef<SWidget> SOnyxLibraryTool::MakeToolbar()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 2.f)
		[
			SNew(SBox).WidthOverride(200.f)
			[
				SNew(SSearchBox)
				.HintText(INVTEXT("Search"))
				.OnTextChanged_Lambda([this](const FText& Text) { Search = Text.ToString(); ApplyFilter(); })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(INVTEXT("Look for new assets and classes"))
			.OnClicked_Lambda([this]() { Refresh(); return FReply::Handled(); })
			[
				SNew(SImage).Image(FAppStyle::GetBrush("Icons.Refresh")).ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
}

TSharedRef<SWidget> SOnyxLibraryTool::MakeCategoryBar()
{
	TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);
	for (const EOnyxLibraryCategory Each : Categories)
	{
		Bar->AddSlot().AutoWidth().Padding(1.f, 2.f)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.IsChecked_Lambda([this, Each]() { return Category == Each ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this, Each](ECheckBoxState) { Category = Each; ApplyFilter(); })
			[
				SNew(SBox).Padding(FMargin(8.f, 2.f))
				[
					SNew(STextBlock).Text(CategoryName(Each))
				]
			]
		];
	}
	return Bar;
}

TSharedRef<SWidget> SOnyxLibraryTool::MakePreviewPanel()
{
	auto Action = [this](const FText& Label, const FText& Tooltip, TFunction<void()> OnClicked, TFunction<bool()> IsVisible, bool bPrimary = false)
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), bPrimary ? "PrimaryButton" : "Button")
			.ToolTipText(Tooltip)
			.ContentPadding(FMargin(8.f, 3.f))
			.Visibility_Lambda([this, IsVisible]() { return Selected.IsValid() && IsVisible() ? EVisibility::Visible : EVisibility::Collapsed; })
			.OnClicked_Lambda([OnClicked]() { OnClicked(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(Label)
			];
	};
	auto Is = [this](std::initializer_list<EOnyxLibraryCategory> Kinds)
	{
		TArray<EOnyxLibraryCategory> List(Kinds);
		return [this, List]() { return Selected.IsValid() && List.Contains(Selected->Category); };
	};
	const auto IsActor = Is({ EOnyxLibraryCategory::Character, EOnyxLibraryCategory::Object });

	return SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(8.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
				.Padding(6.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SAssignNew(Preview, SOnyxFlipbookPreview)
						.Height(260.f)
						.EmptyText_Lambda([this]() { return Selected.IsValid() ? FText::GetEmpty() : INVTEXT("Pick something to preview it"); })
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SAssignNew(StillPreview, SBox).WidthOverride(256.f).HeightOverride(256.f)
						.Visibility_Lambda([this]() { return Preview->GetFrameCount() == 0 && Selected.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				SAssignNew(AnimationButtons, SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(3.f, 3.f))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.ColorAndOpacity(Muted())
				.Text_Lambda([this]()
				{
					if (!Selected.IsValid())
					{
						return FText::GetEmpty();
					}
					if (UClass* Native = Selected->NativeClass.Get())
					{
						return FText::Format(INVTEXT("{0} - C++ class {1}{2} (based on {3})"), CategoryName(Selected->Category),
							FText::FromString(Native->GetPrefixCPP()), FText::FromString(Native->GetName()), FText::FromString(FindOnyxBase(Native) ? FindOnyxBase(Native)->GetName() : FString()));
					}
					return FText::Format(INVTEXT("{0} - {1}"), CategoryName(Selected->Category), FText::FromString(Selected->Asset.GetObjectPathString()));
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f))
				+ SWrapBox::Slot()
				[
					Action(INVTEXT("Place in Level"), INVTEXT("Put it in the open level in front of the camera."), [this]() { PlaceSelected(); },
						Is({ EOnyxLibraryCategory::Character, EOnyxLibraryCategory::Object, EOnyxLibraryCategory::Animation }), true)
				]
				+ SWrapBox::Slot()
				[
					Action(INVTEXT("Edit Moveset"), INVTEXT("Open its Moveset in the Moveset Editor: clips, frames, timeline events."), [this]()
					{
						OnEditAnimations.ExecuteIfBound(Selected->Asset.GetAsset());
					}, [this, IsActor]()
					{
						const bool bMovesetAsset = Selected.IsValid() && (Selected->Asset.IsInstanceOf<UOnyxMoveset>() || Selected->Asset.IsInstanceOf<UOnyxClip>());
						return bMovesetAsset || (IsActor() && Selected->Asset.IsValid() && Animations.Num() > 0);
					})
				]
				+ SWrapBox::Slot()
				[
					Action(INVTEXT("Paint With It"), INVTEXT("Open the Level Painter with this tile set."), [this]()
					{
						OnPaintTileSet.ExecuteIfBound(Cast<UPaperTileSet>(Selected->Asset.GetAsset()));
					}, Is({ EOnyxLibraryCategory::TileSet }), true)
				]
				+ SWrapBox::Slot()
				[
					Action(INVTEXT("Open in Painter"), INVTEXT("Edit this tile map in the Level Painter."), [this]()
					{
						OnPaintTileMap.ExecuteIfBound(Cast<UPaperTileMap>(Selected->Asset.GetAsset()));
					}, Is({ EOnyxLibraryCategory::TileMap }), true)
				]
				+ SWrapBox::Slot()
				[
					Action(INVTEXT("Open"), INVTEXT("Open the asset editor (C++ classes: the source file)."), [this]() { OpenSelected(); }, []() { return true; })
				]
				+ SWrapBox::Slot()
				[
					Action(INVTEXT("Show in Content Browser"), INVTEXT("Find it in the Content Browser."), [this]()
					{
						GEditor->SyncBrowserToObjects(TArray<FAssetData>{ Selected->Asset });
					}, [this]() { return Selected->Asset.IsValid(); })
				]
			]
		];
}

TSharedRef<ITableRow> SOnyxLibraryTool::MakeTile(FEntryPtr Entry, const TSharedRef<STableViewBase>& Owner)
{
	TSharedRef<SWidget> Image = SNullWidget::NullWidget;
	if (Entry->Thumbnail.IsValid())
	{
		Image = Entry->Thumbnail->MakeThumbnailWidget();
	}
	else if (UClass* Native = Entry->NativeClass.Get())
	{
		Image = SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SImage).Image(FSlateIconFinder::FindIconBrushForClass(Native)).DesiredSizeOverride(FVector2D(40.f, 40.f))
			];
	}

	return SNew(STableRow<FEntryPtr>, Owner)
		.Padding(4.f)
		.ToolTipText(FText::FromString(Entry->Asset.IsValid() ? Entry->Asset.GetObjectPathString() : Entry->Name + TEXT(" (C++)")))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(88.f).HeightOverride(88.f)
				[
					Image
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(FText::FromString(Entry->Name)).Justification(ETextJustify::Center).WrapTextAt(104.f)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Entry->NativeClass.IsValid() ? FText::Format(INVTEXT("{0} (C++)"), CategoryName(Entry->Category)) : CategoryName(Entry->Category))
				.Justification(ETextJustify::Center).Font(Font(8)).ColorAndOpacity(Muted())
			]
		];
}

void SOnyxLibraryTool::Refresh()
{
	const FString SelectedKey = Selected.IsValid() ? (Selected->Asset.IsValid() ? Selected->Asset.GetObjectPathString() : Selected->Name) : FString();
	Entries.Reset();

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FARFilter Filter;
	Filter.PackagePaths.Add(TEXT("/Game"));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths = { UBlueprint::StaticClass()->GetClassPathName(), UPaperTileSet::StaticClass()->GetClassPathName(),
		UPaperTileMap::StaticClass()->GetClassPathName(), UPaperFlipbook::StaticClass()->GetClassPathName(), UOnyxClip::StaticClass()->GetClassPathName(),
		UOnyxMoveset::StaticClass()->GetClassPathName() };
	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);

	for (const FAssetData& Asset : Assets)
	{
		EOnyxLibraryCategory EntryCategory = EOnyxLibraryCategory::All;
		const FTopLevelAssetPath Class = Asset.AssetClassPath;
		if (Class == UPaperTileSet::StaticClass()->GetClassPathName())
		{
			EntryCategory = EOnyxLibraryCategory::TileSet;
		}
		else if (Class == UPaperTileMap::StaticClass()->GetClassPathName())
		{
			EntryCategory = EOnyxLibraryCategory::TileMap;
		}
		else if (Class == UPaperFlipbook::StaticClass()->GetClassPathName() || Class == UOnyxClip::StaticClass()->GetClassPathName()
			|| Class == UOnyxMoveset::StaticClass()->GetClassPathName())
		{
			EntryCategory = EOnyxLibraryCategory::Animation;
		}
		else
		{
			// Blueprints: only those built on an Onyx class (also through your own C++ classes).
			FString NativeParent;
			if (Asset.GetTagValue(FBlueprintTags::NativeParentClassPath, NativeParent))
			{
				const UClass* Parent = FindObject<UClass>(nullptr, *FPackageName::ExportTextPathToObjectPath(NativeParent));
				EntryCategory = ClassCategory(Parent);
			}
		}
		if (EntryCategory == EOnyxLibraryCategory::All)
		{
			continue;
		}
		FEntryPtr Entry = MakeShared<FOnyxLibraryEntry>();
		Entry->Asset = Asset;
		Entry->Category = EntryCategory;
		Entry->Name = Asset.AssetName.ToString();
		Entry->Thumbnail = MakeShared<FAssetThumbnail>(Asset, 88, 88, ThumbnailPool);
		Entries.Add(Entry);
	}

	// Your C++ classes deriving from Onyx actors (in your game module, not the plugin's).
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Hidden) || Class->ClassGeneratedBy
			|| Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}
		const FString Package = Class->GetOutermost()->GetName();
		if (!Package.StartsWith(TEXT("/Script/")) || Package.StartsWith(TEXT("/Script/Onyx2D")))
		{
			continue;
		}
		const EOnyxLibraryCategory EntryCategory = ClassCategory(Class);
		if (EntryCategory == EOnyxLibraryCategory::All)
		{
			continue;
		}
		FEntryPtr Entry = MakeShared<FOnyxLibraryEntry>();
		Entry->NativeClass = Class;
		Entry->Category = EntryCategory;
		Entry->Name = Class->GetName();
		Entries.Add(Entry);
	}

	Entries.Sort([](const FEntryPtr& A, const FEntryPtr& B)
	{
		return A->Category != B->Category ? uint8(A->Category) < uint8(B->Category) : A->Name < B->Name;
	});
	ApplyFilter();

	const FEntryPtr* Again = SelectedKey.IsEmpty() ? nullptr : Entries.FindByPredicate([&SelectedKey](const FEntryPtr& Entry)
	{
		return (Entry->Asset.IsValid() ? Entry->Asset.GetObjectPathString() : Entry->Name) == SelectedKey;
	});
	if (Again && TileView.IsValid())
	{
		TileView->SetSelection(*Again);
	}
}

void SOnyxLibraryTool::ApplyFilter()
{
	Filtered = Entries.FilterByPredicate([this](const FEntryPtr& Entry)
	{
		return (Category == EOnyxLibraryCategory::All || Entry->Category == Category) && (Search.IsEmpty() || Entry->Name.Contains(Search));
	});
	if (TileView.IsValid())
	{
		TileView->RequestListRefresh();
	}
}

UClass* SOnyxLibraryTool::GetEntryClass(const FOnyxLibraryEntry& Entry) const
{
	if (UClass* Native = Entry.NativeClass.Get())
	{
		return Native;
	}
	const UBlueprint* Blueprint = Cast<UBlueprint>(Entry.Asset.GetAsset());
	return Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
}

UObject* SOnyxLibraryTool::GetEntryObject(const FOnyxLibraryEntry& Entry) const
{
	return Entry.Asset.IsValid() ? Entry.Asset.GetAsset() : nullptr;
}

void SOnyxLibraryTool::Select(FEntryPtr Entry)
{
	Selected = Entry;
	Animations.Reset();
	CurrentAnimation = NAME_None;
	Preview->Clear();
	StillPreview->SetContent(SNullWidget::NullWidget);
	AnimationButtons->ClearChildren();
	if (!Entry.IsValid())
	{
		return;
	}

	switch (Entry->Category)
	{
	case EOnyxLibraryCategory::Character:
	case EOnyxLibraryCategory::Object:
	{
		UClass* Class = GetEntryClass(*Entry);
		const AActor* Defaults = Class ? Class->GetDefaultObject<AActor>() : nullptr;
		if (const AOnyxCharacter* Character = Cast<AOnyxCharacter>(Defaults); Character && Character->MovesetComponent)
		{
			AddMovesetClips(Character->MovesetComponent->Moveset);
		}
		// Other actors: the first flipbook or sprite on them (native components or ones added in the Blueprint).
		TArray<const UActorComponent*> Components;
		if (Defaults)
		{
			for (const UActorComponent* Component : Defaults->GetComponents())
			{
				Components.Add(Component);
			}
		}
		if (const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Class); Generated && Generated->SimpleConstructionScript)
		{
			for (const USCS_Node* Node : Generated->SimpleConstructionScript->GetAllNodes())
			{
				Components.Add(Node->ComponentTemplate);
			}
		}
		for (const UActorComponent* Component : Components)
		{
			if (!Animations.IsEmpty())
			{
				break;
			}
			if (UPaperFlipbookComponent* Flipbook = Cast<UPaperFlipbookComponent>(const_cast<UActorComponent*>(Component)); Flipbook && Flipbook->GetFlipbook())
			{
				Animations.Add(TEXT("Default"), { Flipbook->GetFlipbook(), 1.f });
			}
			else if (UPaperSpriteComponent* SpriteComponent = Cast<UPaperSpriteComponent>(const_cast<UActorComponent*>(Component)); SpriteComponent && SpriteComponent->GetSprite())
			{
				const UPaperSprite* Sprite = SpriteComponent->GetSprite();
				if (UTexture2D* Texture = Sprite->GetSourceTexture())
				{
					const FIntPoint Min(FMath::RoundToInt(Sprite->GetSourceUV().X), FMath::RoundToInt(Sprite->GetSourceUV().Y));
					const FIntPoint Size(FMath::RoundToInt(Sprite->GetSourceSize().X), FMath::RoundToInt(Sprite->GetSourceSize().Y));
					Preview->SetFrames({ FOnyxPreviewFrame{ Texture, FIntRect(Min, Min + Size) } }, 1.f);
				}
			}
		}
		ShowRoleButtons();
		break;
	}
	case EOnyxLibraryCategory::Animation:
		if (const UOnyxMoveset* Moveset = Cast<UOnyxMoveset>(Entry->Asset.GetAsset()))
		{
			AddMovesetClips(Moveset);
			ShowRoleButtons();
		}
		else if (const UOnyxClip* Clip = Cast<UOnyxClip>(Entry->Asset.GetAsset()))
		{
			Preview->SetFlipbook(Clip->Flipbook, Clip->PlayRate);
		}
		else
		{
			Preview->SetFlipbook(Cast<UPaperFlipbook>(Entry->Asset.GetAsset()));
		}
		break;
	case EOnyxLibraryCategory::TileSet:
		if (const UPaperTileSet* TileSet = Cast<UPaperTileSet>(Entry->Asset.GetAsset()); TileSet && TileSet->GetTileSheetTexture())
		{
			UTexture2D* Sheet = TileSet->GetTileSheetTexture();
			Preview->SetFrames({ FOnyxPreviewFrame{ Sheet, FIntRect(0, 0, Sheet->Source.GetSizeX(), Sheet->Source.GetSizeY()) } }, 1.f);
		}
		break;
	default:
		break;
	}

	// Nothing animated to show (tile maps, actors without art): the asset's thumbnail, large.
	if (Preview->GetFrameCount() == 0 && Entry->Asset.IsValid())
	{
		StillPreview->SetContent(MakeShared<FAssetThumbnail>(Entry->Asset, 256, 256, ThumbnailPool)->MakeThumbnailWidget());
	}
}

void SOnyxLibraryTool::AddMovesetClips(const UOnyxMoveset* Moveset)
{
	if (!Moveset)
	{
		return;
	}
	for (const TPair<FName, TObjectPtr<UOnyxClip>>& Pair : Moveset->Clips)
	{
		if (Pair.Value && Pair.Value->Flipbook)
		{
			Animations.Add(Pair.Key, { Pair.Value->Flipbook.Get(), Pair.Value->PlayRate });
		}
	}
}

void SOnyxLibraryTool::ShowRoleButtons()
{
	// Opens on Idle; every role gets a button.
	TArray<FName> Names;
	Animations.GetKeys(Names);
	for (const FName Name : Names)
	{
		AnimationButtons->AddSlot()
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.IsChecked_Lambda([this, Name]() { return CurrentAnimation == Name ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this, Name](ECheckBoxState) { ShowAnimation(Name); })
			[
				SNew(SBox).Padding(FMargin(6.f, 1.f))[SNew(STextBlock).Text(FText::FromName(Name))]
			]
		];
	}
	if (!Names.IsEmpty())
	{
		ShowAnimation(Animations.Contains(TEXT("Idle")) ? FName(TEXT("Idle")) : Names[0]);
	}
}

void SOnyxLibraryTool::ShowAnimation(FName Animation)
{
	if (const TPair<TWeakObjectPtr<UPaperFlipbook>, float>* Found = Animations.Find(Animation))
	{
		CurrentAnimation = Animation;
		Preview->SetFlipbook(Found->Key.Get(), Found->Value);
	}
}

void SOnyxLibraryTool::PlaceSelected()
{
	if (!Selected.IsValid())
	{
		return;
	}
	if (Selected->Category == EOnyxLibraryCategory::Animation)
	{
		// A flipbook, a clip's frames or a moveset's Idle clip.
		UObject* Asset = Selected->Asset.GetAsset();
		UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(Asset);
		if (const UOnyxClip* Clip = Cast<UOnyxClip>(Asset))
		{
			Flipbook = Clip->Flipbook;
		}
		else if (Animations.Contains(CurrentAnimation))
		{
			Flipbook = Animations[CurrentAnimation].Key.Get();
		}
		if (!Flipbook)
		{
			return;
		}
		if (APaperFlipbookActor* Actor = Cast<APaperFlipbookActor>(FOnyxLevelTools::PlaceInFrontOfCamera(APaperFlipbookActor::StaticClass())); Actor && Flipbook)
		{
			UPaperFlipbookComponent* Component = Actor->GetRenderComponent();
			Component->Modify();
			Component->SetMobility(EComponentMobility::Movable);
			Component->SetFlipbook(Flipbook);
			if (UMaterialInterface* Lit = UOnyx2DSettings::Get()->LitSpriteMaterial.LoadSynchronous())
			{
				Component->SetMaterial(0, Lit); // lit by the 2D lights like the rest of the level
			}
			Actor->SetActorLabel(Flipbook->GetName());
		}
		return;
	}
	if (UClass* Class = GetEntryClass(*Selected))
	{
		if (!FOnyxLevelTools::PlaceInFrontOfCamera(Class))
		{
			Notify(INVTEXT("Open a level first."), false);
		}
	}
}

void SOnyxLibraryTool::OpenSelected()
{
	if (!Selected.IsValid())
	{
		return;
	}
	if (UClass* Native = Selected->NativeClass.Get())
	{
		if (FSourceCodeNavigation::CanNavigateToClass(Native))
		{
			FSourceCodeNavigation::NavigateToClass(Native);
		}
		else
		{
			Notify(INVTEXT("No source code access set up: pick your IDE in Editor Preferences > Source Code."), false);
		}
		return;
	}
	if (UObject* Object = GetEntryObject(*Selected))
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Object);
	}
}
