// Copyright Onyx2D. All Rights Reserved.

#include "MovesetEditor/SOnyxAnimationEditor.h"
#include "Animation/OnyxClipEvent.h"
#include "Animation/OnyxClipWindow.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxMoveset.h"
#include "Animation/OnyxMovesetComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AssetTools/OnyxMovesetAssets.h"
#include "Builders/OnyxAssetUtils.h"
#include "Builders/OnyxCharacterBuilder.h"
#include "Builders/OnyxContentKit.h"
#include "Builders/OnyxPixelLightBuilder.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Lighting/OnyxPixelLightComponent.h"
#include "Character/OnyxBehaviourLibrary.h"
#include "Character/OnyxCharacter.h"
#include "Combat/OnyxCombatTypes.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "IDetailsView.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "Sound/SoundBase.h"
#include "Style/OnyxEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWrapBox.h"
#include "ToolsWindow/OnyxToolWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOnyxBehaviourPicker.h"
#include "Widgets/SOnyxFlipbookPreview.h"
#include "Widgets/SOnyxSpriteSlicer.h"
#include "MovesetEditor/Timeline/OnyxClipTimelineModel.h"
#include "MovesetEditor/Timeline/SOnyxClipTimeline.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

using namespace OnyxTools;

namespace
{
	/** Standard animations offered as one-click slots while missing. */
	const FName AnimEditorSuggestedNames[] = { TEXT("Idle"), TEXT("Run"), TEXT("Jump"), TEXT("Fall"), TEXT("Attack1"), TEXT("Hurt"), TEXT("Death") };

	/** Names without the asset prefix: MS_Fox -> Fox. */
	FString AnimEditorBaseName(const UObject* Asset)
	{
		FString Name = Asset ? Asset->GetName() : FString(TEXT("Character"));
		Name.RemoveFromStart(TEXT("MS_"));
		Name.RemoveFromStart(TEXT("BP_"));
		return Name;
	}

	TSharedRef<SWidget> AnimEditorLabeled(const FText& Label, const TSharedRef<SWidget>& Content, float LabelWidth = 80.f)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(LabelWidth)[SNew(STextBlock).Text(Label).ColorAndOpacity(Muted())]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				Content
			];
	}

	TSharedRef<SWidget> AnimEditorBox(const TAttribute<FText>& Title, const TSharedRef<SWidget>& Content, TAttribute<EVisibility> Visibility = EVisibility::Visible)
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
			.Padding(FMargin(10.f, 8.f))
			.Visibility(Visibility)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
				[
					SNew(STextBlock).Text(Title).Font(Font(10, true))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					Content
				]
			];
	}

	/** Where the Moveset Editor keeps what you left open (Editor Per Project User Settings). */
	const TCHAR* AnimEditorConfigSection = TEXT("Onyx2D.MovesetEditor");

	/**
	 * A box like AnimEditorBox that folds: only its header (Title, and Summary muted beside it) until clicked open. For tools
	 * used now and then, which shouldn't take the panel's room; it stays as you left it, by Key.
	 */
	TSharedRef<SWidget> AnimEditorFoldout(const FText& Title, const FText& Summary, const TCHAR* Key, const TSharedRef<SWidget>& Content,
		TAttribute<EVisibility> Visibility = EVisibility::Visible)
	{
		bool bOpen = false;
		GConfig->GetBool(AnimEditorConfigSection, Key, bOpen, GEditorPerProjectIni);
		const FString KeyName(Key);
		return SNew(SBox)
			.Visibility(Visibility)
			[
				SNew(SExpandableArea)
				.InitiallyCollapsed(!bOpen)
				.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
				.BodyBorderImage(FAppStyle::GetBrush("Brushes.Panel"))
				.HeaderPadding(FMargin(8.f, 7.f))
				.Padding(FMargin(10.f, 2.f, 10.f, 8.f))
				.OnAreaExpansionChanged_Lambda([KeyName](bool bExpanded)
				{
					GConfig->SetBool(AnimEditorConfigSection, *KeyName, bExpanded, GEditorPerProjectIni);
				})
				.HeaderContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(Title).Font(Font(10, true))
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock).Text(Summary).ColorAndOpacity(Muted()).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					]
				]
				.BodyContent()
				[
					Content
				]
			];
	}
}

void SOnyxAnimationEditor::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	NotifyDetails = PropertyEditor.CreateDetailView(DetailsArgs);
	NotifyDetails->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent&)
	{
		if (UOnyxClip* Sequence = GetSequence())
		{
			Sequence->SortEvents();
			Sequence->MarkPackageDirty();
		}
	});

	TimelineModel = MakeShared<FOnyxClipTimelineModel>();
	TimelineModel->OnSelectionChanged.AddSP(this, &SOnyxAnimationEditor::SyncSelectionFromTimeline);
	TimelineModel->OnEdited.AddSP(this, &SOnyxAnimationEditor::RefreshPreview);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			ToolbarStrip(MakeToolbar())
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(SSplitter)
			+ SSplitter::Slot().Value(0.18f)
			[
				Section(TAttribute<FText>::CreateLambda([this]() { return Set.IsValid() ? FText::FromString(Set->GetName()) : INVTEXT("Clips"); }),
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SAssignNew(List, SListView<FEntryPtr>)
						.ListItemsSource(&Entries)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SOnyxAnimationEditor::MakeRow)
						.OnContextMenuOpening(this, &SOnyxAnimationEditor::MakeRowMenu)
						.OnMouseButtonClick_Lambda([this](FEntryPtr Entry)
						{
							if (Entry.IsValid() && !Entry->Sequence.IsValid())
							{
								NewAnimation(Entry->Name);
							}
							else if (Entry.IsValid())
							{
								Select(Entry->Name);
							}
						})
					]
					+ SOverlay::Slot().VAlign(VAlign_Center).Padding(12.f)
					[
						SNew(SBox).Visibility_Lambda([this]() { return Set.IsValid() || LooseSequence.IsValid() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
						[
							Hint(INVTEXT("Pick a Moveset in the toolbar, or press New Moveset. A moveset holds a character's clips by role: Idle, Run, Jump..."))
						]
					])
			]
			+ SSplitter::Slot().Value(0.57f)
			[
				SNew(SSplitter).Orientation(Orient_Vertical)
				+ SSplitter::Slot().Value(0.45f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
					.Padding(8.f)
					[
						SAssignNew(Preview, SOnyxFlipbookPreview)
						.Height(320.f)
						.EmptyText(INVTEXT("No clip selected"))
					]
				]
				+ SSplitter::Slot().Value(0.55f)
				[
					SAssignNew(Timeline, SOnyxClipTimeline, TimelineModel.ToSharedRef())
				]
			]
+ SSplitter::Slot().Value(0.25f)
			[
				Section(INVTEXT("Details"), MakeSidePanel())
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			StatusBar(TAttribute<FText>::CreateLambda([this]()
			{
				const UOnyxClip* Sequence = GetSequence();
				if (!Sequence)
				{
					return INVTEXT("Movesets: pick one, New Clip cuts frames from a sprite sheet, New Character makes it playable.");
				}
				return FText::Format(INVTEXT("{0} - {1} frames, {2} s, {3} events. Right-click a lane to add messages, sounds and effects."),
					FText::FromName(SelectedName.IsNone() ? FName(*Sequence->GetName()) : SelectedName), Sequence->GetFrameUnits(),
					FText::AsNumber(Sequence->GetLength()), Sequence->Events.Num());
			}))
		]
	];

	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SOnyxAnimationEditor::Tick));
}

TSharedRef<SWidget> SOnyxAnimationEditor::MakeToolbar()
{
	FSlimHorizontalToolBarBuilder Toolbar(nullptr, FMultiBoxCustomization::None);
	Toolbar.SetLabelVisibility(EVisibility::Visible);
	Toolbar.AddWidget(SNew(SBox).WidthOverride(230.f).VAlign(VAlign_Center).Padding(4.f, 0.f)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(UOnyxMoveset::StaticClass())
			.ObjectPath_Lambda([this]() { return Set.IsValid() ? Set->GetPathName() : FString(); })
			.OnObjectChanged_Lambda([this](const FAssetData& Asset) { Character.Reset(); LooseSequence.Reset(); SetSet(Cast<UOnyxMoveset>(Asset.GetAsset())); })
			.DisplayThumbnail(false)
		]);
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxAnimationEditor::NewSet)), NAME_None, INVTEXT("New Moveset"),
		INVTEXT("A new Moveset: a character's clips by role."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"));
	Toolbar.AddSeparator();
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxAnimationEditor::NewAnimation, FName())), NAME_None, INVTEXT("New Clip"),
		INVTEXT("Cut frames out of a sprite sheet into a new clip of this moveset."), FOnyxEditorStyle::Icon("Onyx2D.Tab.Animations"));
	Toolbar.AddComboButton(FUIAction(), FOnGetContent::CreateLambda([this]()
	{
		FContentBrowserModule& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		FAssetPickerConfig Config;
		Config.Filter.ClassPaths.Add(UOnyxClip::StaticClass()->GetClassPathName());
		Config.InitialAssetViewType = EAssetViewType::List;
		Config.bAllowNullSelection = false;
		Config.OnAssetSelected = FOnAssetSelected::CreateLambda([this](const FAssetData& Asset)
		{
			FSlateApplication::Get().DismissAllMenus();
			UOnyxClip* Sequence = Cast<UOnyxClip>(Asset.GetAsset());
			if (!Sequence || !Set.IsValid())
			{
				return;
			}
			FString Name = Sequence->GetName();
			Name.RemoveFromStart(TEXT("CL_"));
			Name.RemoveFromStart(AnimEditorBaseName(Set.Get()) + TEXT("_"));
			const FScopedTransaction Transaction(INVTEXT("Add Clip"));
			Set->SetClip(FName(*Name), Sequence);
			Set->MarkPackageDirty();
			Refresh();
			Select(FName(*Name));
		});
		return SNew(SBox).WidthOverride(320.f).HeightOverride(360.f)[ContentBrowser.Get().CreateAssetPicker(Config)];
	}), INVTEXT("Add Existing"), INVTEXT("Put a Clip you already have into this moveset."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"));
	Toolbar.AddSeparator();
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxAnimationEditor::NewCharacter),
		FCanExecuteAction::CreateLambda([this]() { return Set.IsValid() && !Set->Clips.IsEmpty(); })), NAME_None, INVTEXT("New Character"),
		INVTEXT("Make a playable character from this set: choose its behaviours (player, patrol...) and team."), FOnyxEditorStyle::Icon("Onyx2D.Tab.Character"));
	Toolbar.AddSeparator();
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxAnimationEditor::Save)), NAME_None, INVTEXT("Save"),
		INVTEXT("Save the set, its animations and their frames."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"));
	return Toolbar.MakeWidget();
}

TSharedRef<SWidget> SOnyxAnimationEditor::MakeSidePanel()
{
	const auto HasSequence = [this]() { return GetSequence() && GetSequence()->Flipbook ? EVisibility::Visible : EVisibility::Collapsed; };
	const auto HasFrame = [this]()
	{
		const UOnyxClip* Sequence = GetSequence();
		return Sequence && Sequence->Flipbook && Sequence->Flipbook->IsValidKeyFrameIndex(SelectedFrame) && !SelectedNotify.IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
	};
	auto FlipbookOf = [this]() { UOnyxClip* Sequence = GetSequence(); return Sequence ? Sequence->Flipbook.Get() : nullptr; };

	return SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(6.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				AnimEditorBox(TAttribute<FText>::CreateLambda([this]() { return FText::Format(INVTEXT("Clip: {0}"), FText::FromName(SelectedName)); }),
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
					[
						AnimEditorLabeled(INVTEXT("Loop"), SNew(SCheckBox)
							.ToolTipText(INVTEXT("Loops (idle, run) or plays once and holds the last frame (attack, hurt, death)."))
							.IsChecked_Lambda([this]() { return GetSequence() && GetSequence()->bLoop ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
							.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
							{
								if (UOnyxClip* Sequence = GetSequence())
								{
									const FScopedTransaction Transaction(INVTEXT("Change Loop"));
									Sequence->Modify();
									Sequence->bLoop = State == ECheckBoxState::Checked;
									Sequence->MarkPackageDirty();
								}
							}))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
					[
						AnimEditorLabeled(INVTEXT("Frames/sec"), SNew(SNumericEntryBox<float>)
							.MinValue(0.5f).MaxValue(120.f)
							.ToolTipText(INVTEXT("Frame units per second. A frame lasting 2 units shows twice as long."))
							.Value_Lambda([FlipbookOf]() { const UPaperFlipbook* Flipbook = FlipbookOf(); return Flipbook ? TOptional<float>(Flipbook->GetFramesPerSecond()) : TOptional<float>(); })
							.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { TimelineModel->SetFramesPerSecond(Value); }))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
					[
						AnimEditorLabeled(INVTEXT("Speed"), SNew(SNumericEntryBox<float>)
							.MinValue(0.05f).MaxValue(10.f)
							.ToolTipText(INVTEXT("Playback speed multiplier in game (1 = as authored)."))
							.Value_Lambda([this]() { return GetSequence() ? TOptional<float>(GetSequence()->PlayRate) : TOptional<float>(); })
							.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type)
							{
								if (UOnyxClip* Sequence = GetSequence())
								{
									const FScopedTransaction Transaction(INVTEXT("Change Speed"));
									Sequence->Modify();
									Sequence->PlayRate = FMath::Clamp(Value, 0.05f, 10.f);
									Sequence->MarkPackageDirty();
								}
							}))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).Text(INVTEXT("Add Frames")).ToolTipText(INVTEXT("Cut more frames from a sprite sheet and add them at the end."))
							.OnClicked_Lambda([this]()
							{
								UOnyxClip* Target = GetSequence();
								UTexture2D* Sheet = nullptr;
								if (Target && Target->Flipbook && Target->Flipbook->GetNumKeyFrames() > 0 && Target->Flipbook->GetKeyFrameChecked(0).Sprite)
								{
									Sheet = Target->Flipbook->GetKeyFrameChecked(0).Sprite->GetSourceTexture();
								}
								TWeakObjectPtr<UOnyxClip> WeakTarget(Target);
								SOnyxSpriteSlicer::Open(Sheet, GetFolder(), AnimEditorBaseName(Set.Get()) + TEXT("_More"), SelectedName,
									FOnOnyxClipSliced::CreateLambda([this, WeakTarget](UOnyxClip* Made, FName)
									{
										UOnyxClip* Into = WeakTarget.Get();
										if (!Into || !Into->Flipbook || !Made || !Made->Flipbook)
										{
											return;
										}
										const FScopedTransaction Transaction(INVTEXT("Add Frames"));
										Into->Flipbook->Modify();
										{
											FScopedFlipbookMutator EditLock(Into->Flipbook);
											for (int32 Index = 0; Index < Made->Flipbook->GetNumKeyFrames(); ++Index)
											{
												EditLock.KeyFrames.Add(Made->Flipbook->GetKeyFrameChecked(Index));
											}
										}
										Into->Flipbook->MarkPackageDirty();
										RefreshPreview();
									}));
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton).Text(INVTEXT("Open Asset")).ToolTipText(INVTEXT("Show the Clip asset in the Content Browser."))
							.OnClicked_Lambda([this]()
							{
								if (UOnyxClip* Sequence = GetSequence())
								{
									GEditor->SyncBrowserToObjects(TArray<UObject*>{ Sequence });
								}
								return FReply::Handled();
							})
						]
					],
					TAttribute<EVisibility>::CreateLambda(HasSequence))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				AnimEditorBox(TAttribute<FText>::CreateLambda([this]() { return FText::Format(INVTEXT("Frame {0}"), SelectedFrame + 1); }),
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
					[
						AnimEditorLabeled(INVTEXT("Hold"), SNew(SNumericEntryBox<int32>)
							.MinValue(1).MaxValue(60)
							.ToolTipText(INVTEXT("How many frame units it stays on screen (or drag its right edge in the timeline)."))
							.Value_Lambda([FlipbookOf, this]()
							{
								const UPaperFlipbook* Flipbook = FlipbookOf();
								return Flipbook && Flipbook->IsValidKeyFrameIndex(SelectedFrame) ? TOptional<int32>(Flipbook->GetKeyFrameChecked(SelectedFrame).FrameRun) : TOptional<int32>();
							})
							.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type) { TimelineModel->SetFrameHold(SelectedFrame, Value); }))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
					[
						AnimEditorLabeled(INVTEXT("Sprite"), SNew(SObjectPropertyEntryBox)
							.AllowedClass(UPaperSprite::StaticClass())
							.DisplayThumbnail(true)
							.ObjectPath_Lambda([FlipbookOf, this]()
							{
								const UPaperFlipbook* Flipbook = FlipbookOf();
								const UPaperSprite* Sprite = Flipbook && Flipbook->IsValidKeyFrameIndex(SelectedFrame) ? Flipbook->GetKeyFrameChecked(SelectedFrame).Sprite.Get() : nullptr;
								return Sprite ? Sprite->GetPathName() : FString();
							})
							.OnObjectChanged_Lambda([this, FlipbookOf](const FAssetData& Asset)
							{
								UPaperFlipbook* Flipbook = FlipbookOf();
								if (Flipbook && Flipbook->IsValidKeyFrameIndex(SelectedFrame))
								{
									const FScopedTransaction Transaction(INVTEXT("Change Frame Sprite"));
									Flipbook->Modify();
									FScopedFlipbookMutator(Flipbook).KeyFrames[SelectedFrame].Sprite = Cast<UPaperSprite>(Asset.GetAsset());
									Flipbook->MarkPackageDirty();
									RefreshPreview();
								}
							}))
					],
					TAttribute<EVisibility>::CreateLambda(HasFrame))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				AnimEditorBox(TAttribute<FText>::CreateLambda([this]()
					{
						return SelectedNotify.IsValid() ? FText::Format(INVTEXT("Event: {0}"), SelectedNotify->GetClass()->GetDisplayNameText()) : FText::GetEmpty();
					}),
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						NotifyDetails.ToSharedRef()
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
					[
						Hint(INVTEXT("Message events call the function or custom event with that name on the character (no parameters) and fire On Clip Message on its Moveset component."))
					],
					TAttribute<EVisibility>::CreateLambda([this]() { return SelectedNotify.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; }))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).Visibility_Lambda([this]() { return GetSequence() && !SelectedNotify.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					Hint(INVTEXT("Click a frame in the timeline to change its hold or sprite. Right-click a lane to add a message, a sound, an effect or a window at that time; click an event to edit it."))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				MakePixelLightPanel()
			]
		];
}

TSharedRef<SWidget> SOnyxAnimationEditor::MakePixelLightPanel()
{
	constexpr float PixelLabelWidth = 100.f; // "Painted mask" fits
	// Folded away until wanted: most clips never need it.
	return AnimEditorFoldout(INVTEXT("Pixel Light"), INVTEXT("make part of the art a light"), TEXT("PixelLightOpen"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			Hint(INVTEXT("Light from chosen pixels of the art: pick the colours that give light (a blade, a lantern's glass) in the frame on show, then Build. Every frame where they show lights the world around them, shaped like them."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SAssignNew(Swatches, SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(3.f, 3.f))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(STextBlock).ColorAndOpacity(Muted()).AutoWrapText(true).Text_Lambda([this]()
			{
				if (PixelLight.bFromPaintedMask)
				{
					return INVTEXT("Pixels from the glow mask painted beside each texture (<texture>_GlowMask).");
				}
				return PixelLight.Colors.IsEmpty() ? INVTEXT("No colours chosen: click the swatches above.")
					: FText::Format(INVTEXT("{0} colours give light."), PixelLight.Colors.Num());
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			AnimEditorLabeled(INVTEXT("Spread"), SNew(SNumericEntryBox<int32>)
				.ToolTipText(INVTEXT("How far the light spreads from those pixels, in art pixels."))
				.MinValue(1).MaxValue(64).AllowSpin(true).MinSliderValue(1).MaxSliderValue(40)
				.Value_Lambda([this]() { return PixelLight.Spread; })
				.OnValueChanged_Lambda([this](int32 Value) { PixelLight.Spread = Value; })
				.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type) { PixelLight.Spread = Value; }), PixelLabelWidth)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			AnimEditorLabeled(INVTEXT("Tolerance"), SNew(SNumericEntryBox<float>)
				.ToolTipText(INVTEXT("How close a colour must be to one chosen to count (0: exactly)."))
				.MinValue(0.f).MaxValue(1.f).AllowSpin(true).MinSliderValue(0.f).MaxSliderValue(0.25f)
				.Value_Lambda([this]() { return PixelLight.Tolerance; })
				.OnValueChanged_Lambda([this](float Value) { PixelLight.Tolerance = Value; })
				.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { PixelLight.Tolerance = Value; }), PixelLabelWidth)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			AnimEditorLabeled(INVTEXT("Brightness"), SNew(SNumericEntryBox<float>)
				.ToolTipText(INVTEXT("Also every pixel brighter than this (0: off), for art without set colours."))
				.MinValue(0.f).MaxValue(1.f).AllowSpin(true).MinSliderValue(0.f).MaxSliderValue(1.f)
				.Value_Lambda([this]() { return PixelLight.MinBrightness; })
				.OnValueChanged_Lambda([this](float Value) { PixelLight.MinBrightness = Value; })
				.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { PixelLight.MinBrightness = Value; }), PixelLabelWidth)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			AnimEditorLabeled(INVTEXT("Own colours"), SNew(SCheckBox)
				.ToolTipText(INVTEXT("The light takes the colour of the pixels it comes from (off: white)."))
				.IsChecked_Lambda([this]() { return PixelLight.bPixelColours ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { PixelLight.bPixelColours = State == ECheckBoxState::Checked; }), PixelLabelWidth)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			AnimEditorLabeled(INVTEXT("Painted mask"), SNew(SCheckBox)
				.ToolTipText(INVTEXT("Take the pixels from a glow mask painted for the art instead of the colours: a copy of each texture, white where it gives light, black elsewhere, imported beside it as <texture>_GlowMask. For a part whose colours others share (a spear painted with the hair's colours)."))
				.IsChecked_Lambda([this]() { return PixelLight.bFromPaintedMask ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { PixelLight.bFromPaintedMask = State == ECheckBoxState::Checked; }), PixelLabelWidth)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton).Text(INVTEXT("Build Pixel Light"))
				.ToolTipText(INVTEXT("Makes the light of every frame (a Pixel Light Set next to the moveset). A character open here gets an Onyx Pixel Light that plays it."))
				.IsEnabled_Lambda([this]() { return Set.IsValid() && PixelLight.NamesPixels(); })
				.OnClicked_Lambda([this]() { BuildPixelLight(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(INVTEXT("Glow Mask"))
				.ToolTipText(INVTEXT("Only those pixels glow by themselves (Glowing Pixels from the mask), not every bright pixel."))
				.IsEnabled_Lambda([this]() { return Set.IsValid() && PixelLight.NamesPixels(); })
				.OnClicked_Lambda([this]() { MakeGlowMask(); return FReply::Handled(); })
			]
		],
		TAttribute<EVisibility>::CreateLambda([this]() { return Set.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; }));
}

UPaperSprite* SOnyxAnimationEditor::GetShownSprite() const
{
	const UOnyxClip* Sequence = GetSequence();
	const UPaperFlipbook* Flipbook = Sequence ? Sequence->Flipbook.Get() : nullptr;
	return Flipbook && Flipbook->GetNumKeyFrames() > 0 ? Flipbook->GetSpriteAtFrame(FMath::Max(TimelineModel->GetUnitAt(TimelineModel->GetTime()), 0)) : nullptr;
}

void SOnyxAnimationEditor::RefreshSwatches()
{
	UPaperSprite* Sprite = GetShownSprite();
	if (!Swatches.IsValid() || Sprite == SwatchSprite.Get())
	{
		return;
	}
	SwatchSprite = Sprite;
	Swatches->ClearChildren();
	if (!Sprite)
	{
		return;
	}
	TArray<TPair<FColor, int32>>* Colours = ColourCache.Find(Sprite);
	if (!Colours)
	{
		Colours = &ColourCache.Add(Sprite, FOnyxPixelLightBuilder::CountColours(Sprite));
	}
	// The frame's colours, the most used first; the chosen ones ringed.
	const int32 Shown = FMath::Min(Colours->Num(), 40);
	for (int32 Index = 0; Index < Shown; ++Index)
	{
		const FColor Colour = (*Colours)[Index].Key;
		const int32 Count = (*Colours)[Index].Value;
		Swatches->AddSlot()
		[
			SNew(SBorder)
			.Padding(2.f)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor_Lambda([this, Colour]() { return PixelLight.Colors.Contains(Colour) ? FLinearColor::White : FLinearColor(0.f, 0.f, 0.f, 0.f); })
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(0.f)
				.ToolTipText(FText::Format(INVTEXT("#{0} ({1} pixels): click to choose it, again to drop it"), FText::FromString(Colour.ToHex().Left(6)), Count))
				.OnClicked_Lambda([this, Colour]()
				{
					if (!PixelLight.Colors.Remove(Colour))
					{
						PixelLight.Colors.Add(Colour);
					}
					return FReply::Handled();
				})
				[
					SNew(SBox).WidthOverride(18.f).HeightOverride(18.f)
					[
						SNew(SImage).Image(FAppStyle::GetBrush("WhiteBrush")).ColorAndOpacity(FLinearColor(Colour))
					]
				]
			]
		];
	}
}

FString SOnyxAnimationEditor::GetPixelLightName() const
{
	FString Folder;
	FString Name;
	FOnyxPixelLightBuilder::GetMovesetSetPath(Set.Get(), Folder, Name);
	return Name;
}

void SOnyxAnimationEditor::BuildPixelLight()
{
	UOnyxMoveset* Current = Set.Get();
	if (!Current)
	{
		return;
	}
	FText Error;
	UOnyxPixelLightSet* Made = FOnyxPixelLightBuilder::BuildForMoveset(Current, PixelLight, GetFolder(), GetPixelLightName(), Error);
	if (!Made)
	{
		Notify(Error, false);
		return;
	}
	// The character open here plays it.
	if (UBlueprint* Blueprint = Character.Get())
	{
		if (UOnyxPixelLightComponent* Light = FOnyxContentKit::AddComponent<UOnyxPixelLightComponent>(Blueprint, TEXT("PixelLight")))
		{
			Light->Modify();
			Light->LightSet = Made;
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
	}
	Notify(FText::Format(INVTEXT("Pixel Light built: {0} frames give light{1}"), Made->Frames.Num(),
		Character.IsValid() ? INVTEXT(", and the character plays it") : INVTEXT(" (add an Onyx Pixel Light to the character and give it this set)")), true);
}

void SOnyxAnimationEditor::MakeGlowMask()
{
	UOnyxMoveset* Current = Set.Get();
	if (!Current)
	{
		return;
	}
	FText Error;
	const int32 Given = FOnyxPixelLightBuilder::ApplyGlowMasks(FOnyxPixelLightBuilder::FlipbooksOf(Current), PixelLight, Error);
	if (Given == 0)
	{
		Notify(Error, false);
		return;
	}
	// The character open here glows from it.
	if (UBlueprint* Blueprint = Character.Get())
	{
		if (AOnyxCharacter* Defaults = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject<AOnyxCharacter>() : nullptr)
		{
			Defaults->Modify();
			Defaults->bGlowFromMask = true;
			Defaults->GlowPixels = Defaults->GlowPixels > 0.f ? Defaults->GlowPixels : 2.f; // past 1: they burn
			Blueprint->MarkPackageDirty();
		}
	}
	Notify(FText::Format(INVTEXT("Glow mask made for {0} frames{1}"), Given,
		Character.IsValid() ? INVTEXT(": only those pixels glow now") : INVTEXT(" (turn on the character's Glow From Mask)")), true);
}

TSharedRef<ITableRow> SOnyxAnimationEditor::MakeRow(FEntryPtr Entry, const TSharedRef<STableViewBase>& Owner)
{
	const bool bMissing = !Entry->Sequence.IsValid();
	return SNew(STableRow<FEntryPtr>, Owner)
		.Padding(FMargin(6.f, 4.f))
		.ToolTipText(bMissing ? FText::Format(INVTEXT("Make the {0} animation from a sprite sheet"), FText::FromName(Entry->Name)) : FText::GetEmpty())
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SImage)
				.Image(bMissing ? FAppStyle::GetBrush("Icons.Plus") : FOnyxEditorStyle::Get().GetBrush("Onyx2D.Tab.Animations"))
				.ColorAndOpacity(bMissing ? FSlateColor(Muted()) : FSlateColor::UseForeground())
				.DesiredSizeOverride(FVector2D(14.f, 14.f))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromName(Entry->Name))
				.Font(bMissing ? Font(9) : Font(10, Entry->Name == SelectedName))
				.ColorAndOpacity(bMissing ? FSlateColor(Muted()) : FSlateColor::UseForeground())
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Font(8)).ColorAndOpacity(Muted()).Text_Lambda([Entry]()
				{
					const UOnyxClip* Sequence = Entry->Sequence.Get();
					if (!Sequence)
					{
						return INVTEXT("missing");
					}
					return FText::Format(INVTEXT("{0}f{1}"), Sequence->GetFrameUnits(), Sequence->bLoop ? INVTEXT(" loop") : FText::GetEmpty());
				})
			]
		];
}

TSharedPtr<SWidget> SOnyxAnimationEditor::MakeRowMenu()
{
	const TArray<FEntryPtr> Picked = List->GetSelectedItems();
	if (Picked.IsEmpty() || !Picked[0]->Sequence.IsValid())
	{
		return nullptr;
	}
	FMenuBuilder Menu(true, nullptr);
	Menu.AddMenuEntry(INVTEXT("Rename"), INVTEXT("Change the name the character plays it by."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
		FUIAction(FExecuteAction::CreateSP(this, &SOnyxAnimationEditor::RenameSelected)));
	Menu.AddMenuEntry(INVTEXT("Remove from Moveset"), INVTEXT("The Clip asset is kept."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
		FUIAction(FExecuteAction::CreateSP(this, &SOnyxAnimationEditor::RemoveSelected)));
	Menu.AddMenuEntry(INVTEXT("Show in Content Browser"), FText::GetEmpty(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Search"),
		FUIAction(FExecuteAction::CreateLambda([Picked]() { GEditor->SyncBrowserToObjects(TArray<UObject*>{ Picked[0]->Sequence.Get() }); })));
	return Menu.MakeWidget();
}

void SOnyxAnimationEditor::Edit(UObject* Asset)
{
	if (UOnyxMoveset* AsSet = Cast<UOnyxMoveset>(Asset))
	{
		Character.Reset();
		LooseSequence.Reset();
		SetSet(AsSet);
		return;
	}
	if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		const AOnyxCharacter* Defaults = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject<AOnyxCharacter>() : nullptr;
		Character = Blueprint;
		LooseSequence.Reset();
		SetSet(Defaults && Defaults->MovesetComponent ? Defaults->MovesetComponent->Moveset.Get() : nullptr);
		return;
	}
	if (UOnyxClip* Sequence = Cast<UOnyxClip>(Asset))
	{
		// Open the set that uses it, when there is one.
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FName> Referencers;
		Registry.GetReferencers(Sequence->GetPackage()->GetFName(), Referencers);
		for (const FName& Package : Referencers)
		{
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPackageName(Package, Assets);
			for (const FAssetData& Candidate : Assets)
			{
				if (Candidate.AssetClassPath == UOnyxMoveset::StaticClass()->GetClassPathName())
				{
					if (UOnyxMoveset* Owner = Cast<UOnyxMoveset>(Candidate.GetAsset()))
					{
						Character.Reset();
						LooseSequence.Reset();
						SetSet(Owner);
						for (const TPair<FName, TObjectPtr<UOnyxClip>>& Pair : Owner->Clips)
						{
							if (Pair.Value == Sequence)
							{
								Select(Pair.Key);
							}
						}
						return;
					}
				}
			}
		}
		Character.Reset();
		SetSet(nullptr);
		LooseSequence = Sequence;
		SelectedName = FName(*Sequence->GetName());
		RefreshPreview();
	}
}

void SOnyxAnimationEditor::SetSet(UOnyxMoveset* NewSet)
{
	Set = NewSet;
	SelectedName = NAME_None;
	// Its Pixel Light, when it has one: carry on from how it was made.
	PixelLight = FOnyxPixelLightRecipe();
	if (NewSet)
	{
		const FString Path = GetFolder() / GetPixelLightName();
		if (const UOnyxPixelLightSet* Existing = LoadObject<UOnyxPixelLightSet>(nullptr, *(Path + TEXT(".") + GetPixelLightName()), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			PixelLight = Existing->Recipe;
		}
	}
	Refresh();
	if (NewSet)
	{
		// Open on Idle, or the first animation.
		FName First;
		for (const FEntryPtr& Entry : Entries)
		{
			if (Entry->Sequence.IsValid() && (First.IsNone() || Entry->Name == TEXT("Idle")))
			{
				First = Entry->Name;
			}
		}
		if (!First.IsNone())
		{
			Select(First);
		}
	}
}

void SOnyxAnimationEditor::Refresh()
{
	Entries.Reset();
	if (UOnyxMoveset* Current = Set.Get())
	{
		TArray<FName> Names;
		Current->Clips.GetKeys(Names);
		const TArray<FName>& Standard = UOnyxMoveset::GetStandardRoles();
		Names.Sort([&Standard](const FName& A, const FName& B)
		{
			const int32 IndexA = Standard.IndexOfByKey(A);
			const int32 IndexB = Standard.IndexOfByKey(B);
			if (IndexA != IndexB)
			{
				return IndexA == INDEX_NONE ? false : (IndexB == INDEX_NONE ? true : IndexA < IndexB);
			}
			return A.LexicalLess(B);
		});
		for (const FName& Name : Names)
		{
			Entries.Add(MakeShared<FEntry>(FEntry{ Name, Current->FindClip(Name) }));
		}
		for (const FName& Suggested : AnimEditorSuggestedNames)
		{
			if (!Current->Clips.Contains(Suggested))
			{
				Entries.Add(MakeShared<FEntry>(FEntry{ Suggested, nullptr }));
			}
		}
	}
	if (List.IsValid())
	{
		List->RequestListRefresh();
	}
	RefreshPreview();
}

void SOnyxAnimationEditor::Select(FName Name)
{
	SelectedName = Name;
	SelectedFrame = INDEX_NONE;
	SelectNotify(nullptr);
	TimelineModel->SetClip(GetSequence());
	if (List.IsValid())
	{
		if (const FEntryPtr* Found = Entries.FindByPredicate([Name](const FEntryPtr& Entry) { return Entry->Name == Name; }))
		{
			List->SetSelection(*Found, ESelectInfo::Direct);
		}
		List->RebuildList();
	}
	RefreshPreview();
}

UOnyxClip* SOnyxAnimationEditor::GetSequence() const
{
	if (const UOnyxMoveset* Current = Set.Get())
	{
		return Current->FindClip(SelectedName);
	}
	return LooseSequence.Get();
}

float SOnyxAnimationEditor::GetLength() const
{
	const UOnyxClip* Sequence = GetSequence();
	return Sequence ? Sequence->GetLength() : 0.f;
}

void SOnyxAnimationEditor::RefreshPreview()
{
	if (!Preview.IsValid())
	{
		return;
	}
	const UOnyxClip* Sequence = GetSequence();
	Preview->SetFlipbook(Sequence ? Sequence->Flipbook.Get() : nullptr);
	Preview->SetPlaying(false);
	if (Sequence && Sequence->Flipbook)
	{
		Preview->SetFrame(FMath::Clamp(TimelineModel->GetUnitAt(TimelineModel->GetTime()), 0, FMath::Max(Preview->GetFrameCount() - 1, 0)));
	}
}

EActiveTimerReturnType SOnyxAnimationEditor::Tick(double CurrentTime, float DeltaTime)
{
	// The timeline owns playback; the preview shows the frame at its time.
	UOnyxClip* Sequence = GetSequence();
	if (TimelineModel->GetClip() != Sequence)
	{
		TimelineModel->SetClip(Sequence);
		RefreshPreview();
	}
	TimelineModel->Tick(DeltaTime);
	if (Sequence && Sequence->Flipbook && Preview->GetFrameCount() > 0)
	{
		Preview->SetFrame(FMath::Clamp(TimelineModel->GetUnitAt(TimelineModel->GetTime()), 0, Preview->GetFrameCount() - 1));
	}
	RefreshSwatches();
	return EActiveTimerReturnType::Continue;
}

void SOnyxAnimationEditor::SyncSelectionFromTimeline()
{
	SelectNotify(TimelineModel->GetFocusedEvent());
	SelectedFrame = TimelineModel->GetSelectedFrame();
}

void SOnyxAnimationEditor::OnUndoRedo()
{
	Refresh();
	TimelineModel->NotifyExternalChange();
}

void SOnyxAnimationEditor::SelectNotify(UOnyxClipEvent* Notify)
{
	SelectedNotify = Notify;
	if (NotifyDetails.IsValid())
	{
		NotifyDetails->SetObject(Notify, true);
	}
	if (Notify)
	{
		SelectedFrame = INDEX_NONE;
	}
}

FString SOnyxAnimationEditor::GetFolder() const
{
	if (const UOnyxMoveset* Current = Set.Get())
	{
		return FPackageName::GetLongPackagePath(Current->GetPackage()->GetName());
	}
	return TEXT("/Game/Characters");
}

void SOnyxAnimationEditor::NewSet()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UOnyxMovesetFactory* Factory = NewObject<UOnyxMovesetFactory>();
	if (UOnyxMoveset* Created = Cast<UOnyxMoveset>(AssetTools.CreateAssetWithDialog(TEXT("MS_NewCharacter"), TEXT("/Game/Characters"),
		UOnyxMoveset::StaticClass(), Factory)))
	{
		Character.Reset();
		LooseSequence.Reset();
		SetSet(Created);
	}
}

void SOnyxAnimationEditor::NewAnimation(FName SuggestedName)
{
	if (!Set.IsValid())
	{
		NewSet();
		if (!Set.IsValid())
		{
			return;
		}
	}
	// Start from the sheet the current animation came from, if any.
	UTexture2D* Sheet = nullptr;
	if (const UOnyxClip* Current = GetSequence(); Current && Current->Flipbook && Current->Flipbook->GetNumKeyFrames() > 0 && Current->Flipbook->GetKeyFrameChecked(0).Sprite)
	{
		Sheet = Current->Flipbook->GetKeyFrameChecked(0).Sprite->GetSourceTexture();
	}
	OpenSlicer(Sheet, SuggestedName.IsNone() ? FName(TEXT("Idle")) : SuggestedName);
}

void SOnyxAnimationEditor::NewAnimationFrom(UTexture2D* Sheet)
{
	if (!Set.IsValid())
	{
		NewSet();
		if (!Set.IsValid())
		{
			return;
		}
	}
	OpenSlicer(Sheet, TEXT("Idle"));
}

void SOnyxAnimationEditor::OpenSlicer(UTexture2D* Sheet, FName Name)
{
	TWeakObjectPtr<UOnyxMoveset> WeakSet = Set;
	SOnyxSpriteSlicer::Open(Sheet, GetFolder(), AnimEditorBaseName(Set.Get()), Name, FOnOnyxClipSliced::CreateLambda([this, WeakSet](UOnyxClip* Sequence, FName Made)
	{
		UOnyxMoveset* Target = WeakSet.Get();
		if (!Target || !Sequence)
		{
			return;
		}
		const FScopedTransaction Transaction(INVTEXT("New Clip"));
		Target->SetClip(Made, Sequence);
		Target->MarkPackageDirty();
		FOnyxAssetUtils::SavePackages({ Target->GetPackage() });
		if (Set == Target)
		{
			Refresh();
			Select(Made);
		}
	}));
}

void SOnyxAnimationEditor::RenameSelected()
{
	UOnyxMoveset* Current = Set.Get();
	if (!Current || SelectedName.IsNone())
	{
		return;
	}
	const FName OldName = SelectedName;
	TSharedRef<SWindow> Window = SNew(SWindow).Title(INVTEXT("Rename Role")).ClientSize(FVector2D(320.f, 90.f)).SupportsMinimize(false).SupportsMaximize(false);
	TWeakPtr<SWindow> WeakWindow = Window;
	Window->SetContent(SNew(SBorder).BorderImage(FAppStyle::GetBrush("Brushes.Panel")).Padding(12.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SEditableTextBox)
				.Text(FText::FromName(OldName))
				.SelectAllTextWhenFocused(true)
				.OnTextCommitted_Lambda([this, OldName, WeakWindow](const FText& Text, ETextCommit::Type Commit)
				{
					const FName NewName(*FOnyxAssetUtils::SanitizeName(Text.ToString().TrimStartAndEnd()));
					UOnyxMoveset* Target = Set.Get();
					if (Commit == ETextCommit::OnEnter && Target && !NewName.IsNone() && NewName != OldName && !Target->Clips.Contains(NewName))
					{
						const FScopedTransaction Transaction(INVTEXT("Rename Role"));
						Target->Modify();
						TObjectPtr<UOnyxClip> Sequence = Target->Clips.FindAndRemoveChecked(OldName);
						Target->Clips.Add(NewName, Sequence);
						Target->MarkPackageDirty();
						Refresh();
						Select(NewName);
					}
					if (TSharedPtr<SWindow> Pinned = WeakWindow.Pin())
					{
						Pinned->RequestDestroyWindow();
					}
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				Hint(INVTEXT("Press Enter. Idle, Run, Jump, Fall, Crouch, Dash, WallSlide, Attack1-3, Hurt and Death play by themselves."))
			]
		]);
	FSlateApplication::Get().AddModalWindow(Window, AsShared());
}

void SOnyxAnimationEditor::RemoveSelected()
{
	if (UOnyxMoveset* Current = Set.Get(); Current && Current->Clips.Contains(SelectedName))
	{
		const FScopedTransaction Transaction(INVTEXT("Remove Clip"));
		Current->Modify();
		Current->Clips.Remove(SelectedName);
		Current->MarkPackageDirty();
		SelectedName = NAME_None;
		Refresh();
	}
}

void SOnyxAnimationEditor::NewCharacter()
{
	UOnyxMoveset* Current = Set.Get();
	if (!Current)
	{
		return;
	}
	struct FState
	{
		FString Name;
		FString Folder = TEXT("/Game/Characters");
		int32 Behaviours = int32(UOnyxBehaviourLibrary::PlayerPreset);
		uint8 Team = OnyxTeams::Player;
		float CentimetersPerPixel = 4.f;
	};
	TSharedRef<FState> State = MakeShared<FState>();
	State->Name = AnimEditorBaseName(Current);
	State->Folder = FPackageName::GetLongPackagePath(FPackageName::GetLongPackagePath(Current->GetPackage()->GetName()));

	TSharedRef<SWindow> Window = SNew(SWindow).Title(INVTEXT("New Character")).ClientSize(FVector2D(460.f, 250.f)).SupportsMinimize(false).SupportsMaximize(false);
	TWeakPtr<SWindow> WeakWindow = Window;
	TWeakObjectPtr<UOnyxMoveset> WeakSet = Current;
	Window->SetContent(SNew(SBorder).BorderImage(FAppStyle::GetBrush("Brushes.Panel")).Padding(14.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				AnimEditorLabeled(INVTEXT("Name"), SNew(SEditableTextBox).Text_Lambda([State]() { return FText::FromString(State->Name); })
					.OnTextChanged_Lambda([State](const FText& Text) { State->Name = Text.ToString(); }), 110.f)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				AnimEditorLabeled(INVTEXT("Folder"), SNew(SEditableTextBox).Text_Lambda([State]() { return FText::FromString(State->Folder); })
					.OnTextChanged_Lambda([State](const FText& Text) { State->Folder = Text.ToString(); }), 110.f)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				AnimEditorLabeled(INVTEXT("Behaviours"), SNew(SOnyxBehaviourPicker)
					.Behaviours_Lambda([State]() { return State->Behaviours; })
					.Team_Lambda([State]() { return State->Team; })
					.OnBehavioursChanged_Lambda([State](int32 Value) { State->Behaviours = Value; })
					.OnTeamChanged_Lambda([State](uint8 Value) { State->Team = Value; }), 110.f)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				AnimEditorLabeled(INVTEXT("Pixel Size (cm)"), SNew(SBox).WidthOverride(80.f).HAlign(HAlign_Left)
					[
						SNew(SNumericEntryBox<float>).MinValue(0.1f)
						.Value_Lambda([State]() { return State->CentimetersPerPixel; })
						.OnValueCommitted_Lambda([State](float Value, ETextCommit::Type) { State->CentimetersPerPixel = FMath::Max(Value, 0.1f); })
					], 110.f)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				Hint(INVTEXT("A character is the same class whatever it does: its behaviours are components you can add, remove or tune later in its Blueprint."))
			]
			+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Bottom).HAlign(HAlign_Right)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.Text(INVTEXT("Create Character"))
				.OnClicked_Lambda([State, WeakSet, WeakWindow]()
				{
					FText Error;
					if (UBlueprint* Made = FOnyxCharacterBuilder::CreateFromMoveset(State->Name, State->Folder, WeakSet.Get(), State->Behaviours, State->Team,
						State->CentimetersPerPixel, Error))
					{
						GEditor->SyncBrowserToObjects(TArray<UObject*>{ Made });
						Notify(FText::Format(INVTEXT("{0} created: drag it into the level, or set it as the Default Player Class."), FText::FromString(Made->GetName())), true);
						if (TSharedPtr<SWindow> Pinned = WeakWindow.Pin())
						{
							Pinned->RequestDestroyWindow();
						}
					}
					else
					{
						Notify(Error, false);
					}
					return FReply::Handled();
				})
			]
		]);
	FSlateApplication::Get().AddModalWindow(Window, AsShared());
}

void SOnyxAnimationEditor::Save()
{
	TArray<UPackage*> Packages;
	if (UOnyxMoveset* Current = Set.Get())
	{
		Packages.Add(Current->GetPackage());
		for (const TPair<FName, TObjectPtr<UOnyxClip>>& Pair : Current->Clips)
		{
			if (Pair.Value)
			{
				Packages.Add(Pair.Value->GetPackage());
				if (Pair.Value->Flipbook)
				{
					Packages.Add(Pair.Value->Flipbook->GetPackage());
				}
			}
		}
	}
	else if (UOnyxClip* Sequence = LooseSequence.Get())
	{
		Packages.Add(Sequence->GetPackage());
		if (Sequence->Flipbook)
		{
			Packages.Add(Sequence->Flipbook->GetPackage());
		}
	}
	FOnyxAssetUtils::SavePackages(Packages);
	Notify(INVTEXT("Moveset saved"), true);
}
