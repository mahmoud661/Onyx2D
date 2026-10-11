// Copyright Onyx2D. All Rights Reserved.

#include "Widgets/SOnyxCharacterTool.h"
#include "Assets/OnyxCharacterBuilder.h"
#include "Assets/OnyxImageAnalysis.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "IDetailsView.h"
#include "Logging/MessageLog.h"
#include "Style/OnyxEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Tools/OnyxToolOptions.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/OnyxToolWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOnyxFlipbookPreview.h"
#include "Widgets/SOnyxSpriteSheetView.h"
#include "Widgets/Text/STextBlock.h"

using namespace OnyxTools;

void SOnyxCharacterTool::Construct(const FArguments& InArgs)
{
	Options.Reset(NewObject<UOnyxCharacterWizardOptions>(GetTransientPackage(), NAME_None, RF_Transactional));
	Details = MakeDetailsView(Options.Get());
	Details->OnFinishedChangingProperties().AddSP(this, &SOnyxCharacterTool::HandlePropertyChanged);

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
			.Orientation(Orient_Horizontal)
			.PhysicalSplitterHandleSize(2.f)
			+ SSplitter::Slot().Value(0.38f)
			[
				Section(INVTEXT("Character"), Details.ToSharedRef())
			]
			+ SSplitter::Slot().Value(0.62f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				.PhysicalSplitterHandleSize(2.f)
				+ SSplitter::Slot().Value(0.55f)
				[
					Section(INVTEXT("Sprite Sheet"),
						SNew(SOnyxSpriteSheetView)
						.Texture_Lambda([this]() { return Options->SpriteSheet.Get(); })
						.CellSize_Lambda([this]() { return Options->FrameSize; })
						.HighlightRects(this, &SOnyxCharacterTool::GetSelectedFrameRects)
						.MaxHeight(4096.f),
						SNew(STextBlock).Text(this, &SOnyxCharacterTool::GetSheetInfo).ColorAndOpacity(FSlateColor::UseSubduedForeground()))
				]
				+ SSplitter::Slot().Value(0.45f)
				[
					MakePreviewPanel()
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			StatusBar(TAttribute<FText>::CreateSP(this, &SOnyxCharacterTool::GetStatus))
		]
	];

	RefreshPreview();
}

TSharedRef<SWidget> SOnyxCharacterTool::MakeToolbar()
{
	FSlimHorizontalToolBarBuilder Toolbar(nullptr, FMultiBoxCustomization::None);
	Toolbar.SetLabelVisibility(EVisibility::Visible);

	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxCharacterTool::CreateCharacter),
		FCanExecuteAction::CreateLambda([this]() { return Options->SpriteSheet != nullptr; })),
		NAME_None, INVTEXT("Create Character"), INVTEXT("Create the sprites, flipbooks and a ready-to-play character Blueprint."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"));
	Toolbar.AddSeparator();
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxCharacterTool::UseContentBrowserSelection)),
		NAME_None, INVTEXT("Use Selected"), INVTEXT("Use the texture selected in the Content Browser as the sprite sheet."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"));
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxCharacterTool::DetectFrameSize),
		FCanExecuteAction::CreateLambda([this]() { return Options->SpriteSheet != nullptr; })),
		NAME_None, INVTEXT("Detect Frame Size"), INVTEXT("Guess the frame size from the transparent gaps in the sheet."),
		FOnyxEditorStyle::Icon("Onyx2D.DetectGrid"));
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxCharacterTool::ResetAnimations)),
		NAME_None, INVTEXT("Reset Animations"), INVTEXT("Restore the standard animation list (one row each)."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"));
	return Toolbar.MakeWidget();
}

TSharedRef<SWidget> SOnyxCharacterTool::MakeTransportButton(FName Icon, const FText& Tooltip, TFunction<void()> OnClicked, TFunction<FName()> DynamicIcon)
{
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ToolTipText(Tooltip)
		.ContentPadding(FMargin(6.f, 3.f))
		.OnClicked_Lambda([OnClicked]() { OnClicked(); return FReply::Handled(); })
		[
			SNew(SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image_Lambda([Icon, DynamicIcon]() { return FAppStyle::GetBrush(DynamicIcon ? DynamicIcon() : Icon); })
		];
}

TSharedRef<SWidget> SOnyxCharacterTool::MakePreviewPanel()
{
	SAssignNew(AnimationCombo, SComboBox<TSharedPtr<FName>>)
		.OptionsSource(&AnimationChoices)
		.OnGenerateWidget_Lambda([](TSharedPtr<FName> Item) { return SNew(STextBlock).Text(FText::FromName(*Item)); })
		.OnSelectionChanged_Lambda([this](TSharedPtr<FName> Item, ESelectInfo::Type) { SelectAnimation(Item); })
		[
			SNew(STextBlock).Text_Lambda([this]() { return SelectedAnimation.IsNone() ? INVTEXT("(no animation)") : FText::FromName(SelectedAnimation); })
		];

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
					MakeTransportButton("Animation.Backward_Step", INVTEXT("Previous frame"), [this]() { Preview->SetPlaying(false); Preview->SetFrame(Preview->GetFrame() - 1); })
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					MakeTransportButton("Animation.Forward", INVTEXT("Play / pause"), [this]() { Preview->SetPlaying(!Preview->IsPlaying()); },
						[this]() { return Preview.IsValid() && Preview->IsPlaying() ? FName("Animation.Pause") : FName("Animation.Forward"); })
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					MakeTransportButton("Animation.Forward_Step", INVTEXT("Next frame"), [this]() { Preview->SetPlaying(false); Preview->SetFrame(Preview->GetFrame() + 1); })
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f, 0.f)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text_Lambda([this]()
					{
						const int32 Count = Preview.IsValid() ? Preview->GetFrameCount() : 0;
						return Count == 0 ? INVTEXT("No frames") : FText::Format(INVTEXT("Frame {0} / {1}"), Preview->GetFrame() + 1, Count);
					})
				]
			]
		],
		AnimationCombo);
}

void SOnyxCharacterTool::HandlePropertyChanged(const FPropertyChangedEvent& Event)
{
	LastResult = FText::GetEmpty();
	if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UOnyxCharacterWizardOptions, SpriteSheet) && Options->SpriteSheet)
	{
		SetSpriteSheet(Options->SpriteSheet);
		return;
	}
	RefreshPreview();
}

void SOnyxCharacterTool::SetSpriteSheet(UTexture2D* Texture)
{
	if (!Texture)
	{
		return;
	}
	Options->SpriteSheet = Texture;
	FString Name = Texture->GetName();
	Name.RemoveFromStart(TEXT("T_"));
	Options->Name = Name;
	if (const TOptional<FIntPoint> Size = FOnyxImageAnalysis::DetectFrameSize(Texture))
	{
		Options->FrameSize = *Size;
	}
	Details->ForceRefresh();
	RefreshPreview();
}

void SOnyxCharacterTool::RebuildAnimationChoices()
{
	AnimationChoices.Reset();
	TSharedPtr<FName> Selected;
	for (const FOnyxWizardAnimation& Row : Options->Animations)
	{
		if (Row.bEnabled && !Row.Name.IsNone())
		{
			AnimationChoices.Add(MakeShared<FName>(Row.Name));
			if (Row.Name == SelectedAnimation)
			{
				Selected = AnimationChoices.Last();
			}
		}
	}
	if (!Selected && AnimationChoices.Num() > 0)
	{
		Selected = AnimationChoices[0];
		SelectedAnimation = *Selected;
	}
	if (AnimationCombo.IsValid())
	{
		AnimationCombo->RefreshOptions();
		AnimationCombo->SetSelectedItem(Selected);
	}
}

int32 SOnyxCharacterTool::FindSelectedIndex() const
{
	return Options->Animations.IndexOfByPredicate([this](const FOnyxWizardAnimation& Row) { return Row.bEnabled && Row.Name == SelectedAnimation; });
}

void SOnyxCharacterTool::SelectAnimation(TSharedPtr<FName> Name)
{
	if (Name.IsValid() && *Name != SelectedAnimation)
	{
		SelectedAnimation = *Name;
		RefreshPreview();
	}
}

void SOnyxCharacterTool::RefreshPreview()
{
	RebuildAnimationChoices();
	const int32 Index = FindSelectedIndex();
	if (!Preview.IsValid() || Index == INDEX_NONE)
	{
		if (Preview.IsValid())
		{
			Preview->Clear();
		}
		return;
	}

	const FOnyxCharacterSpec Spec = Options->ToSpec();
	const FOnyxAnimationSpec& Animation = Spec.Animations[Index];
	TArray<FOnyxPreviewFrame> Frames;
	UTexture2D* Texture = FOnyxCharacterBuilder::ResolveTexture(Spec, Animation);
	for (const FIntRect& Rect : FOnyxCharacterBuilder::GetFrameRects(Spec, Animation))
	{
		Frames.Add({ Texture, Rect });
	}
	Preview->SetFrames(Frames, Animation.FramesPerSecond);
	Preview->SetPlaying(true);
}

TArray<FIntRect> SOnyxCharacterTool::GetSelectedFrameRects() const
{
	const int32 Index = FindSelectedIndex();
	if (Index == INDEX_NONE || Options->Animations[Index].TextureOverride)
	{
		return {};
	}
	const FOnyxCharacterSpec Spec = Options->ToSpec();
	return FOnyxCharacterBuilder::GetFrameRects(Spec, Spec.Animations[Index]);
}

FText SOnyxCharacterTool::GetSheetInfo() const
{
	const UTexture2D* Texture = Options->SpriteSheet;
	if (!Texture || Options->FrameSize.X <= 0 || Options->FrameSize.Y <= 0)
	{
		return FText::GetEmpty();
	}
	const FIntPoint Size = Texture->GetImportedSize();
	return FText::Format(INVTEXT("{0} x {1} px  ·  {2} columns x {3} rows of {4} x {5}"),
		Size.X, Size.Y, Size.X / Options->FrameSize.X, Size.Y / Options->FrameSize.Y, Options->FrameSize.X, Options->FrameSize.Y);
}

FText SOnyxCharacterTool::GetStatus() const
{
	if (!LastResult.IsEmpty())
	{
		return LastResult;
	}
	if (!Options->SpriteSheet)
	{
		return INVTEXT("Choose a sprite sheet (or right-click a texture in the Content Browser > Create Onyx Character).");
	}
	const FOnyxCharacterSpec Spec = Options->ToSpec();
	int32 Animations = 0;
	int32 Frames = 0;
	for (const FOnyxAnimationSpec& Animation : Spec.Animations)
	{
		if (Animation.bEnabled)
		{
			const int32 Count = FOnyxCharacterBuilder::ResolveFrameCount(Spec, Animation);
			Animations += Count > 0 ? 1 : 0;
			Frames += Count;
		}
	}
	return FText::Format(INVTEXT("{0} animations, {1} frames  →  {2}/{3}/BP_{3}"), Animations, Frames, FText::FromString(Spec.Folder), FText::FromString(Spec.Name));
}

void SOnyxCharacterTool::CreateCharacter()
{
	const FOnyxCharacterBuildResult Result = FOnyxCharacterBuilder::Build(Options->ToSpec());
	FMessageLog Log(TEXT("Onyx2D"));
	for (const FString& Warning : Result.Warnings)
	{
		Log.Warning(FText::FromString(Warning));
	}
	if (!Result.Succeeded())
	{
		Log.Error(Result.Error);
		Log.Notify(INVTEXT("Onyx2D: character not created"));
		LastResult = Result.Error;
		return;
	}

	LastResult = FText::Format(INVTEXT("Created {0}: {1} flipbooks, {2} sprites{3}."), FText::FromString(Result.Blueprint->GetName()),
		Result.FlipbookCount, Result.SpriteCount,
		Result.CapsuleSize.IsZero() ? FText::GetEmpty() : FText::Format(INVTEXT(", capsule {0} x {1} cm"), FMath::RoundToInt(Result.CapsuleSize.X * 2.f), FMath::RoundToInt(Result.CapsuleSize.Y * 2.f)));
	Log.Info(LastResult);
	if (Result.Warnings.Num() > 0)
	{
		Log.Notify(INVTEXT("Onyx2D: character created with warnings"), EMessageSeverity::Warning);
	}
	else
	{
		Notify(LastResult, true);
	}
	TArray<UObject*> Created = { Result.Blueprint };
	GEditor->SyncBrowserToObjects(Created);
}

void SOnyxCharacterTool::DetectFrameSize()
{
	const TOptional<FIntPoint> Size = FOnyxImageAnalysis::DetectFrameSize(Options->SpriteSheet);
	if (!Size.IsSet())
	{
		Notify(INVTEXT("Could not find a frame grid (the sheet needs a transparent background). Set Frame Size by hand."), false);
		return;
	}
	Options->FrameSize = *Size;
	Details->ForceRefresh();
	RefreshPreview();
	Notify(FText::Format(INVTEXT("Frame size: {0} x {1}"), Size->X, Size->Y), true);
}

void SOnyxCharacterTool::UseContentBrowserSelection()
{
	TArray<FAssetData> Selection;
	FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().GetSelectedAssets(Selection);
	for (const FAssetData& Asset : Selection)
	{
		if (UTexture2D* Texture = Cast<UTexture2D>(Asset.GetAsset()))
		{
			SetSpriteSheet(Texture);
			return;
		}
	}
	Notify(INVTEXT("Select a texture in the Content Browser first."), false);
}

void SOnyxCharacterTool::ResetAnimations()
{
	Options->ResetAnimations();
	Details->ForceRefresh();
	RefreshPreview();
}
