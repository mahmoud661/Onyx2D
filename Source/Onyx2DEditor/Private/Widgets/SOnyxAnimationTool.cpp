// Copyright Onyx2D. All Rights Reserved.

#include "Widgets/SOnyxAnimationTool.h"
#include "Animation/OnyxAnimatorComponent.h"
#include "AssetRegistry/AssetData.h"
#include "Assets/OnyxAssetFactory.h"
#include "Character/OnyxCharacter.h"
#include "Engine/Blueprint.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "PaperFlipbook.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/OnyxToolWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOnyxFlipbookPreview.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"

using namespace OnyxTools;

namespace OnyxAnimationColumns
{
	const FName Name(TEXT("Name"));
	const FName Flipbook(TEXT("Flipbook"));
	const FName PlayRate(TEXT("PlayRate"));
	const FName Loop(TEXT("Loop"));
	const FName Frames(TEXT("Frames"));
	const FName Frame(TEXT("Frame"));
	const FName Event(TEXT("Event"));
	const FName Remove(TEXT("Remove"));
}

namespace
{
	bool IsOnyxCharacterBlueprint(const FAssetData& Asset)
	{
		FString ParentPath;
		if (!Asset.GetTagValue(FBlueprintTags::NativeParentClassPath, ParentPath))
		{
			return false;
		}
		const UClass* Parent = FSoftClassPath(FPackageName::ExportTextPathToObjectPath(ParentPath)).ResolveClass();
		return Parent && Parent->IsChildOf(AOnyxCharacter::StaticClass());
	}

	/** One animation in the table: name (double-click to rename), flipbook, play rate, loop, frame count. */
	class SOnyxAnimationRow : public SMultiColumnTableRow<TSharedPtr<FName>>
	{
	public:
		SLATE_BEGIN_ARGS(SOnyxAnimationRow) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, const TSharedRef<STableViewBase>& Owner, TSharedPtr<FName> InItem, SOnyxAnimationTool* InTool)
		{
			Item = InItem;
			Tool = InTool;
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments().Padding(FMargin(0.f, 2.f)), Owner);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			const FName Name = *Item;
			SOnyxAnimationTool* LocalTool = Tool;
			auto Entry = [LocalTool, Name]() { return LocalTool->FindAnimation(Name); };

			if (Column == OnyxAnimationColumns::Name)
			{
				return SNew(SBox).VAlign(VAlign_Center).Padding(FMargin(6.f, 0.f))
					[
						SNew(SInlineEditableTextBlock)
						.Text(FText::FromName(Name))
						.Font(Font(9, true))
						.ToolTipText(INVTEXT("Double-click to rename. Idle, Run, Jump, Fall, WallSlide, Dash, Attack1-3, Hurt and Death are played automatically."))
						.OnTextCommitted_Lambda([LocalTool, Name](const FText& Text, ETextCommit::Type) { LocalTool->RenameAnimation(Name, *Text.ToString()); })
					];
			}
			if (Column == OnyxAnimationColumns::Flipbook)
			{
				return SNew(SBox).VAlign(VAlign_Center).Padding(FMargin(2.f, 0.f))
					[
						SNew(SObjectPropertyEntryBox)
						.AllowedClass(UPaperFlipbook::StaticClass())
						.DisplayThumbnail(false)
						.AllowCreate(false)
						.ObjectPath_Lambda([Entry]() { const FOnyxAnimation* A = Entry(); return A && A->Flipbook ? A->Flipbook->GetPathName() : FString(); })
						.OnObjectChanged_Lambda([LocalTool, Entry](const FAssetData& Asset)
						{
							LocalTool->BeginEdit(INVTEXT("Set Flipbook"));
							if (FOnyxAnimation* A = Entry())
							{
								A->Flipbook = Cast<UPaperFlipbook>(Asset.GetAsset());
							}
							LocalTool->EndEdit();
							LocalTool->RefreshPreview();
						})
					];
			}
			if (Column == OnyxAnimationColumns::PlayRate)
			{
				return SNew(SBox).VAlign(VAlign_Center).Padding(FMargin(2.f, 0.f))
					[
						SNew(SSpinBox<float>)
						.MinValue(0.05f).MaxValue(10.f).Delta(0.05f)
						.Value_Lambda([Entry]() { const FOnyxAnimation* A = Entry(); return A ? A->PlayRate : 1.f; })
						.OnBeginSliderMovement_Lambda([LocalTool]() { LocalTool->BeginEdit(INVTEXT("Change Play Rate")); })
						.OnValueChanged_Lambda([Entry](float Value) { if (FOnyxAnimation* A = Entry()) { A->PlayRate = Value; } })
						.OnEndSliderMovement_Lambda([LocalTool](float) { LocalTool->EndEdit(); LocalTool->RefreshPreview(); })
						.OnValueCommitted_Lambda([LocalTool, Entry](float Value, ETextCommit::Type)
						{
							LocalTool->BeginEdit(INVTEXT("Change Play Rate"));
							if (FOnyxAnimation* A = Entry()) { A->PlayRate = Value; }
							LocalTool->EndEdit();
							LocalTool->RefreshPreview();
						})
					];
			}
			if (Column == OnyxAnimationColumns::Loop)
			{
				return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([Entry]() { const FOnyxAnimation* A = Entry(); return A && A->bLoop ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([LocalTool, Entry](ECheckBoxState State)
						{
							LocalTool->BeginEdit(INVTEXT("Toggle Loop"));
							if (FOnyxAnimation* A = Entry()) { A->bLoop = State == ECheckBoxState::Checked; }
							LocalTool->EndEdit();
						})
					];
			}
			return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text_Lambda([Entry]() { const FOnyxAnimation* A = Entry(); return A && A->Flipbook ? FText::AsNumber(A->Flipbook->GetNumFrames()) : INVTEXT("-"); })
				];
		}

	private:
		TSharedPtr<FName> Item;
		SOnyxAnimationTool* Tool = nullptr;
	};

	/** One frame event: frame number, event name, remove button. */
	class SOnyxEventRow : public SMultiColumnTableRow<TSharedPtr<int32>>
	{
	public:
		SLATE_BEGIN_ARGS(SOnyxEventRow) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, const TSharedRef<STableViewBase>& Owner, TSharedPtr<int32> InItem, SOnyxAnimationTool* InTool)
		{
			Item = InItem;
			Tool = InTool;
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments().Padding(FMargin(0.f, 2.f)), Owner);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			const int32 Index = *Item;
			SOnyxAnimationTool* LocalTool = Tool;

			if (Column == OnyxAnimationColumns::Frame)
			{
				return SNew(SBox).VAlign(VAlign_Center).Padding(FMargin(4.f, 0.f))
					[
						SNew(SSpinBox<int32>)
						.MinValue(0).MaxValue(512)
						.Value_Lambda([LocalTool, Index]() { const FOnyxFrameEvent* E = LocalTool->FindEvent(Index); return E ? E->Frame : 0; })
						.OnValueCommitted_Lambda([LocalTool, Index](int32 Value, ETextCommit::Type)
						{
							LocalTool->BeginEdit(INVTEXT("Change Event Frame"));
							if (FOnyxFrameEvent* E = LocalTool->FindEvent(Index)) { E->Frame = Value; }
							LocalTool->EndEdit();
							LocalTool->PreviewFrame(Value);
						})
					];
			}
			if (Column == OnyxAnimationColumns::Event)
			{
				return SNew(SBox).VAlign(VAlign_Center).Padding(FMargin(2.f, 0.f))
					[
						SNew(SEditableTextBox)
						.Text_Lambda([LocalTool, Index]() { const FOnyxFrameEvent* E = LocalTool->FindEvent(Index); return E ? FText::FromName(E->EventName) : FText::GetEmpty(); })
						.OnTextCommitted_Lambda([LocalTool, Index](const FText& Text, ETextCommit::Type)
						{
							LocalTool->BeginEdit(INVTEXT("Rename Frame Event"));
							if (FOnyxFrameEvent* E = LocalTool->FindEvent(Index)) { E->EventName = *Text.ToString(); }
							LocalTool->EndEdit();
						})
					];
			}
			return SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(INVTEXT("Remove event"))
				.VAlign(VAlign_Center)
				.OnClicked_Lambda([LocalTool, Index]() { LocalTool->RemoveEvent(Index); return FReply::Handled(); })
				[
					SNew(SImage).Image(FAppStyle::GetBrush("Icons.Delete")).ColorAndOpacity(FSlateColor::UseForeground())
				];
		}

	private:
		TSharedPtr<int32> Item;
		SOnyxAnimationTool* Tool = nullptr;
	};
}

void SOnyxAnimationTool::Construct(const FArguments& InArgs)
{
	SAssignNew(AnimationList, SListView<TSharedPtr<FName>>)
		.ListItemsSource(&AnimationItems)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SOnyxAnimationTool::GenerateAnimationRow)
		.OnSelectionChanged(this, &SOnyxAnimationTool::HandleSelectionChanged)
		.HeaderRow
		(
			SNew(SHeaderRow)
			+ SHeaderRow::Column(OnyxAnimationColumns::Name).DefaultLabel(INVTEXT("Name")).FillWidth(0.22f)
			+ SHeaderRow::Column(OnyxAnimationColumns::Flipbook).DefaultLabel(INVTEXT("Flipbook")).FillWidth(0.48f)
			+ SHeaderRow::Column(OnyxAnimationColumns::PlayRate).DefaultLabel(INVTEXT("Play Rate")).FillWidth(0.14f)
			+ SHeaderRow::Column(OnyxAnimationColumns::Loop).DefaultLabel(INVTEXT("Loop")).FixedWidth(48.f).HAlignHeader(HAlign_Center)
			+ SHeaderRow::Column(OnyxAnimationColumns::Frames).DefaultLabel(INVTEXT("Frames")).FixedWidth(60.f).HAlignHeader(HAlign_Center)
		);

	SAssignNew(EventList, SListView<TSharedPtr<int32>>)
		.ListItemsSource(&EventItems)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow(this, &SOnyxAnimationTool::GenerateEventRow)
		.HeaderRow
		(
			SNew(SHeaderRow)
			+ SHeaderRow::Column(OnyxAnimationColumns::Frame).DefaultLabel(INVTEXT("Frame")).FixedWidth(90.f)
			+ SHeaderRow::Column(OnyxAnimationColumns::Event).DefaultLabel(INVTEXT("Event Name")).FillWidth(1.f)
			+ SHeaderRow::Column(OnyxAnimationColumns::Remove).DefaultLabel(FText::GetEmpty()).FixedWidth(32.f)
		);

	const TSharedRef<SWidget> AddEventButton = SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ToolTipText(INVTEXT("Add an event on the frame shown in the preview"))
		.IsEnabled_Lambda([this]() { return !Selected.IsNone(); })
		.OnClicked_Lambda([this]() { AddEvent(); return FReply::Handled(); })
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SImage).Image(FAppStyle::GetBrush("Icons.Plus")).ColorAndOpacity(FSlateColor::UseForeground())
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(INVTEXT("Add at current frame"))
			]
		];

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			ToolbarStrip(MakeToolbar(), MakeCharacterPicker())
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			.PhysicalSplitterHandleSize(2.f)
			.IsEnabled_Lambda([this]() { return GetAnimator() != nullptr; })
			+ SSplitter::Slot().Value(0.58f)
			[
				Section(INVTEXT("Animations"), AnimationList.ToSharedRef())
			]
			+ SSplitter::Slot().Value(0.42f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				.PhysicalSplitterHandleSize(2.f)
				+ SSplitter::Slot().Value(0.6f)
				[
					MakePreviewPanel()
				]
				+ SSplitter::Slot().Value(0.4f)
				[
					Section(INVTEXT("Frame Events"),
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().FillHeight(1.f)
						[
							EventList.ToSharedRef()
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 6.f)
						[
							Hint(INVTEXT("Events fire the Animator's On Frame Event when the animation reaches that frame (open a hitbox, play a footstep...)."))
						],
						AddEventButton)
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			StatusBar(TAttribute<FText>::CreateSP(this, &SOnyxAnimationTool::GetStatus))
		]
	];
	RefreshLists();
}

TSharedRef<SWidget> SOnyxAnimationTool::MakeToolbar()
{
	FSlimHorizontalToolBarBuilder Toolbar(nullptr, FMultiBoxCustomization::None);
	Toolbar.SetLabelVisibility(EVisibility::Visible);
	const FCanExecuteAction HasCharacter = FCanExecuteAction::CreateLambda([this]() { return Blueprint.IsValid(); });

	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxAnimationTool::Save), HasCharacter),
		NAME_None, INVTEXT("Save"), INVTEXT("Compile and save the character Blueprint."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"));
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxAnimationTool::OpenBlueprintEditor), HasCharacter),
		NAME_None, INVTEXT("Open Blueprint"), INVTEXT("Open the character in the Blueprint editor."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"));
	Toolbar.AddSeparator();
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxAnimationTool::AddAnimation), HasCharacter),
		NAME_None, INVTEXT("Add"), INVTEXT("Add an animation entry."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"));
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxAnimationTool::RemoveSelectedAnimation),
		FCanExecuteAction::CreateLambda([this]() { return !Selected.IsNone(); })),
		NAME_None, INVTEXT("Remove"), INVTEXT("Remove the selected animation and its frame events."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"));
	return Toolbar.MakeWidget();
}

TSharedRef<SWidget> SOnyxAnimationTool::MakeCharacterPicker()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(STextBlock).Text(INVTEXT("Character"))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(300.f)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UBlueprint::StaticClass())
				.DisplayThumbnail(false)
				.AllowCreate(false)
				.AllowClear(false)
				.OnShouldFilterAsset_Lambda([](const FAssetData& Asset) { return !IsOnyxCharacterBlueprint(Asset); })
				.ObjectPath_Lambda([this]() { return Blueprint.IsValid() ? Blueprint->GetPathName() : FString(); })
				.OnObjectChanged_Lambda([this](const FAssetData& Asset) { SetBlueprint(Cast<UBlueprint>(Asset.GetAsset())); })
			]
		];
}

TSharedRef<SWidget> SOnyxAnimationTool::MakePreviewPanel()
{
	auto Transport = [this](TFunction<FName()> Icon, const FText& Tooltip, TFunction<void()> OnClicked) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(Tooltip)
			.ContentPadding(FMargin(6.f, 3.f))
			.OnClicked_Lambda([OnClicked]() { OnClicked(); return FReply::Handled(); })
			[
				SNew(SImage).ColorAndOpacity(FSlateColor::UseForeground()).Image_Lambda([Icon]() { return FAppStyle::GetBrush(Icon()); })
			];
	};

	return Section(INVTEXT("Preview"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(Preview, SOnyxFlipbookPreview).Height(0.f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
			.Padding(FMargin(6.f, 2.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					Transport([]() { return FName("Animation.Backward_Step"); }, INVTEXT("Previous frame"), [this]() { PreviewFrame(Preview->GetFrame() - 1); })
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					Transport([this]() { return Preview.IsValid() && Preview->IsPlaying() ? FName("Animation.Pause") : FName("Animation.Forward"); },
						INVTEXT("Play / pause"), [this]() { Preview->SetPlaying(!Preview->IsPlaying()); })
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					Transport([]() { return FName("Animation.Forward_Step"); }, INVTEXT("Next frame"), [this]() { PreviewFrame(Preview->GetFrame() + 1); })
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f, 0.f)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text_Lambda([this]()
					{
						const int32 Count = Preview.IsValid() ? Preview->GetFrameCount() : 0;
						return Count == 0 ? INVTEXT("No frames") : FText::Format(INVTEXT("Frame {0}  ({1} / {2})"), Preview->GetFrame(), Preview->GetFrame() + 1, Count);
					})
				]
			]
		],
		SNew(STextBlock).Text_Lambda([this]() { return FText::FromName(Selected); }).ColorAndOpacity(FSlateColor::UseSubduedForeground()));
}

TSharedRef<ITableRow> SOnyxAnimationTool::GenerateAnimationRow(TSharedPtr<FName> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(SOnyxAnimationRow, Owner, Item, this);
}

TSharedRef<ITableRow> SOnyxAnimationTool::GenerateEventRow(TSharedPtr<int32> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(SOnyxEventRow, Owner, Item, this);
}

UOnyxAnimatorComponent* SOnyxAnimationTool::GetAnimator() const
{
	const UBlueprint* BP = Blueprint.Get();
	const AOnyxCharacter* Defaults = BP && BP->GeneratedClass ? BP->GeneratedClass->GetDefaultObject<AOnyxCharacter>() : nullptr;
	return Defaults ? Defaults->Animator.Get() : nullptr;
}

FOnyxAnimation* SOnyxAnimationTool::FindAnimation(FName Name) const
{
	UOnyxAnimatorComponent* Animator = GetAnimator();
	return Animator ? Animator->Animations.Find(Name) : nullptr;
}

FOnyxFrameEvent* SOnyxAnimationTool::FindEvent(int32 Index) const
{
	UOnyxAnimatorComponent* Animator = GetAnimator();
	return Animator && Animator->FrameEvents.IsValidIndex(Index) ? &Animator->FrameEvents[Index] : nullptr;
}

void SOnyxAnimationTool::BeginEdit(const FText& Description)
{
	if (UOnyxAnimatorComponent* Animator = GetAnimator())
	{
		Transaction = MakeUnique<FScopedTransaction>(Description);
		Animator->Modify();
	}
}

void SOnyxAnimationTool::EndEdit()
{
	if (UBlueprint* BP = Blueprint.Get())
	{
		BP->MarkPackageDirty();
	}
	Transaction.Reset();
}

void SOnyxAnimationTool::SetBlueprint(UBlueprint* InBlueprint)
{
	Blueprint = InBlueprint;
	Selected = NAME_None;
	RefreshLists();
	if (AnimationItems.Num() > 0)
	{
		const TSharedPtr<FName>* Idle = AnimationItems.FindByPredicate([](const TSharedPtr<FName>& Item) { return *Item == TEXT("Idle"); });
		AnimationList->SetSelection(Idle ? *Idle : AnimationItems[0]);
	}
}

void SOnyxAnimationTool::RefreshLists()
{
	AnimationItems.Reset();
	EventItems.Reset();
	TSharedPtr<FName> SelectedItem;
	if (const UOnyxAnimatorComponent* Animator = GetAnimator())
	{
		TArray<FName> Names;
		Animator->Animations.GetKeys(Names);
		Names.Sort(FNameLexicalLess());
		for (const FName& Name : Names)
		{
			AnimationItems.Add(MakeShared<FName>(Name));
			SelectedItem = Name == Selected ? AnimationItems.Last() : SelectedItem;
		}
		for (int32 Index = 0; Index < Animator->FrameEvents.Num(); ++Index)
		{
			if (Animator->FrameEvents[Index].Animation == Selected)
			{
				EventItems.Add(MakeShared<int32>(Index));
			}
		}
	}
	AnimationList->RequestListRefresh();
	EventList->RequestListRefresh();
	if (SelectedItem.IsValid())
	{
		AnimationList->SetItemSelection(SelectedItem, true, ESelectInfo::Direct);
	}
}

void SOnyxAnimationTool::HandleSelectionChanged(TSharedPtr<FName> Item, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct && !Item.IsValid())
	{
		return;
	}
	const FName NewSelection = Item.IsValid() ? *Item : NAME_None;
	if (NewSelection != Selected)
	{
		Selected = NewSelection;
		RefreshPreview();
		RefreshLists();
	}
}

void SOnyxAnimationTool::RefreshPreview()
{
	const FOnyxAnimation* Animation = FindAnimation(Selected);
	Preview->SetFlipbook(Animation ? Animation->Flipbook.Get() : nullptr, Animation ? Animation->PlayRate : 1.f);
	Preview->SetPlaying(true);
}

void SOnyxAnimationTool::PreviewFrame(int32 Frame)
{
	Preview->SetPlaying(false);
	const int32 Count = Preview->GetFrameCount();
	Preview->SetFrame(Count > 0 ? (Frame % Count + Count) % Count : 0);
}

bool SOnyxAnimationTool::RenameAnimation(FName OldName, FName NewName)
{
	NewName = *FOnyxAssetFactory::SanitizeName(NewName.ToString());
	UOnyxAnimatorComponent* Animator = GetAnimator();
	if (!Animator || NewName.IsNone() || NewName == OldName)
	{
		return false;
	}
	if (Animator->Animations.Contains(NewName))
	{
		Notify(FText::Format(INVTEXT("There is already an animation called {0}."), FText::FromName(NewName)), false);
		RefreshLists();
		return false;
	}
	BeginEdit(INVTEXT("Rename Animation"));
	const FOnyxAnimation Moved = Animator->Animations.FindAndRemoveChecked(OldName);
	Animator->Animations.Add(NewName, Moved);
	for (FOnyxFrameEvent& Event : Animator->FrameEvents)
	{
		Event.Animation = Event.Animation == OldName ? NewName : Event.Animation;
	}
	EndEdit();
	Selected = Selected == OldName ? NewName : Selected;
	RefreshLists();
	return true;
}

void SOnyxAnimationTool::AddAnimation()
{
	UOnyxAnimatorComponent* Animator = GetAnimator();
	if (!Animator)
	{
		return;
	}
	FName Name(TEXT("NewAnimation"));
	for (int32 Suffix = 1; Animator->Animations.Contains(Name); ++Suffix)
	{
		Name = *FString::Printf(TEXT("NewAnimation%d"), Suffix);
	}
	BeginEdit(INVTEXT("Add Animation"));
	Animator->Animations.Add(Name, FOnyxAnimation());
	EndEdit();
	Selected = Name;
	RefreshLists();
	RefreshPreview();
}

void SOnyxAnimationTool::RemoveSelectedAnimation()
{
	UOnyxAnimatorComponent* Animator = GetAnimator();
	if (!Animator || Selected.IsNone())
	{
		return;
	}
	const FName Name = Selected;
	BeginEdit(INVTEXT("Remove Animation"));
	Animator->Animations.Remove(Name);
	Animator->FrameEvents.RemoveAll([Name](const FOnyxFrameEvent& Event) { return Event.Animation == Name; });
	EndEdit();
	Selected = NAME_None;
	Preview->Clear();
	RefreshLists();
}

void SOnyxAnimationTool::AddEvent()
{
	UOnyxAnimatorComponent* Animator = GetAnimator();
	if (!Animator || Selected.IsNone())
	{
		return;
	}
	BeginEdit(INVTEXT("Add Frame Event"));
	FOnyxFrameEvent Event;
	Event.Animation = Selected;
	Event.Frame = Preview->GetFrame();
	Event.EventName = TEXT("HitboxOn");
	Animator->FrameEvents.Add(Event);
	EndEdit();
	RefreshLists();
}

void SOnyxAnimationTool::RemoveEvent(int32 Index)
{
	UOnyxAnimatorComponent* Animator = GetAnimator();
	if (Animator && Animator->FrameEvents.IsValidIndex(Index))
	{
		BeginEdit(INVTEXT("Remove Frame Event"));
		Animator->FrameEvents.RemoveAt(Index);
		EndEdit();
		RefreshLists();
	}
}

void SOnyxAnimationTool::Save()
{
	if (UBlueprint* BP = Blueprint.Get())
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
		const bool bSaved = FOnyxAssetFactory::SavePackages({ BP->GetPackage() });
		Notify(bSaved ? FText::Format(INVTEXT("Saved {0}"), FText::FromString(BP->GetName())) : INVTEXT("Could not save the character"), bSaved);
	}
}

void SOnyxAnimationTool::OpenBlueprintEditor()
{
	if (UBlueprint* BP = Blueprint.Get())
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(BP);
	}
}

FText SOnyxAnimationTool::GetStatus() const
{
	const UOnyxAnimatorComponent* Animator = GetAnimator();
	if (!Animator)
	{
		return INVTEXT("Pick a character Blueprint (any child of Onyx Character) in the toolbar.");
	}
	return FText::Format(INVTEXT("{0}: {1} animations, {2} frame events{3}"), FText::FromString(Blueprint->GetName()),
		Animator->Animations.Num(), Animator->FrameEvents.Num(), Blueprint->GetPackage()->IsDirty() ? INVTEXT("  ·  unsaved changes") : FText::GetEmpty());
}
