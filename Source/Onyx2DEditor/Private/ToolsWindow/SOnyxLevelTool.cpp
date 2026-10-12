// Copyright Onyx2D. All Rights Reserved.

#include "ToolsWindow/SOnyxLevelTool.h"
#include "AssetRegistry/AssetData.h"
#include "Level/OnyxLevelTools.h"
#include "Level/OnyxLevelValidator.h"
#include "Level/OnyxPlaceables.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Editor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Style/OnyxEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWrapBox.h"
#include "ToolsWindow/OnyxToolWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"

using namespace OnyxTools;

namespace OnyxIssueColumns
{
	const FName Severity(TEXT("Severity"));
	const FName Message(TEXT("Message"));
	const FName Fix(TEXT("Fix"));
}

namespace
{
	void OpenProjectSettings(const FName& Section)
	{
		if (ISettingsModule* Settings = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			Settings->ShowViewer("Project", "Plugins", Section);
		}
	}

	const FSlateBrush* SeverityIcon(EOnyxIssueSeverity Severity)
	{
		switch (Severity)
		{
		case EOnyxIssueSeverity::Error: return FAppStyle::GetBrush("Icons.ErrorWithColor");
		case EOnyxIssueSeverity::Warning: return FAppStyle::GetBrush("Icons.WarningWithColor");
		case EOnyxIssueSeverity::Ok: return FAppStyle::GetBrush("Icons.SuccessWithColor");
		default: return FAppStyle::GetBrush("Icons.Info");
		}
	}

	/** Placeable tile: click to drop it where the viewport looks, or drag it into the viewport. */
	class SOnyxPlaceTile : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SOnyxPlaceTile) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, const FOnyxPlaceable& InPlaceable)
		{
			Placeable = InPlaceable;
			const FSlateBrush* Icon = FSlateIconFinder::FindIconBrushForClass(Placeable.Class, "ClassIcon.Actor");

			ChildSlot
			[
				SNew(SBox)
				.WidthOverride(236.f)
				.HeightOverride(52.f)
				[
					SNew(SBorder)
					.BorderImage_Lambda([this]() { return FAppStyle::GetBrush(IsHovered() ? "Brushes.Hover" : "Brushes.Header"); })
					.Padding(FMargin(10.f, 6.f))
					.ToolTipText(FText::Format(INVTEXT("{0}\n\nClick: place it where the viewport is looking.\nDrag: drop it into the viewport."), Placeable.Description))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
						[
							SNew(SBox).WidthOverride(24.f).HeightOverride(24.f)
							[
								SNew(SImage).Image(Icon).ColorAndOpacity(FSlateColor::UseForeground())
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Text(Placeable.Label).Font(Font(9, true))
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Text(Placeable.Description).Font(Font(8)).ColorAndOpacity(FSlateColor::UseSubduedForeground()).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
							]
						]
					]
				]
			];
		}

		virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			return Event.GetEffectingButton() == EKeys::LeftMouseButton
				? FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton)
				: FReply::Unhandled();
		}

		virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			if (Event.GetEffectingButton() == EKeys::LeftMouseButton && IsHovered())
			{
				FOnyxLevelTools::PlaceInFrontOfCamera(Placeable.Class);
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

		virtual FReply OnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			return FReply::Handled().BeginDragDrop(FAssetDragDropOp::New(FAssetData(Placeable.Class), static_cast<UActorFactory*>(nullptr)));
		}

	private:
		FOnyxPlaceable Placeable;
	};

	class SOnyxIssueRow : public SMultiColumnTableRow<TSharedPtr<FOnyxValidationIssue>>
	{
	public:
		SLATE_BEGIN_ARGS(SOnyxIssueRow) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, const TSharedRef<STableViewBase>& Owner, TSharedPtr<FOnyxValidationIssue> InIssue, TFunction<void()> InOnFix)
		{
			Issue = InIssue;
			OnFix = MoveTemp(InOnFix);
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments().Padding(FMargin(0.f, 3.f)), Owner);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			if (Column == OnyxIssueColumns::Severity)
			{
				return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SImage).Image(SeverityIcon(Issue->Severity))];
			}
			if (Column == OnyxIssueColumns::Message)
			{
				return SNew(SBox).VAlign(VAlign_Center).Padding(FMargin(4.f, 0.f))
					[
						SNew(STextBlock).Text(Issue->Message).AutoWrapText(true)
					];
			}
			if (!Issue->HasFix())
			{
				return SNullWidget::NullWidget;
			}
			return SNew(SBox).VAlign(VAlign_Center).Padding(FMargin(4.f, 0.f))
				[
					SNew(SButton)
					.Text(Issue->FixLabel)
					.HAlign(HAlign_Center)
					.OnClicked_Lambda([this]() { OnFix(); return FReply::Handled(); })
				];
		}

	private:
		TSharedPtr<FOnyxValidationIssue> Issue;
		TFunction<void()> OnFix;
	};
}

void SOnyxLevelTool::Construct(const FArguments& InArgs)
{
	SAssignNew(IssueList, SListView<TSharedPtr<FOnyxValidationIssue>>)
		.ListItemsSource(&Issues)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SOnyxLevelTool::GenerateIssueRow)
		.OnMouseButtonDoubleClick_Lambda([](TSharedPtr<FOnyxValidationIssue> Issue)
		{
			if (AActor* Actor = Issue.IsValid() ? Issue->Actor.Get() : nullptr)
			{
				GEditor->SelectNone(false, true);
				GEditor->SelectActor(Actor, true, true);
				GEditor->MoveViewportCamerasToActor(*Actor, false);
			}
		})
		.HeaderRow
		(
			SNew(SHeaderRow)
			+ SHeaderRow::Column(OnyxIssueColumns::Severity).DefaultLabel(FText::GetEmpty()).FixedWidth(28.f)
			+ SHeaderRow::Column(OnyxIssueColumns::Message).DefaultLabel(INVTEXT("Issue (double-click to select the actor)")).FillWidth(1.f)
			+ SHeaderRow::Column(OnyxIssueColumns::Fix).DefaultLabel(INVTEXT("Fix")).FixedWidth(120.f)
		);

	const TSharedRef<SWidget> FixAllButton = SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.Text(INVTEXT("Fix All"))
		.ToolTipText(INVTEXT("Apply every available fix"))
		.IsEnabled_Lambda([this]() { return Issues.ContainsByPredicate([](const TSharedPtr<FOnyxValidationIssue>& Issue) { return Issue->HasFix(); }); })
		.OnClicked_Lambda([this]() { FixAll(); return FReply::Handled(); });

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
			+ SSplitter::Slot().Value(0.5f)
			[
				Section(INVTEXT("Place Actors"), MakePlaceGrid())
			]
			+ SSplitter::Slot().Value(0.5f)
			[
				Section(INVTEXT("Level Check"), IssueList.ToSharedRef(), FixAllButton)
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			StatusBar(TAttribute<FText>::CreateSP(this, &SOnyxLevelTool::GetStatus))
		]
	];

	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	MapChangedHandle = LevelEditor.OnMapChanged().AddLambda([this](UWorld*, EMapChangeType) { RunValidation(); });
	RunValidation();
}

SOnyxLevelTool::~SOnyxLevelTool()
{
	if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditor->OnMapChanged().Remove(MapChangedHandle);
	}
}

TSharedRef<SWidget> SOnyxLevelTool::MakeToolbar()
{
	FSlimHorizontalToolBarBuilder Toolbar(nullptr, FMultiBoxCustomization::None);
	Toolbar.SetLabelVisibility(EVisibility::Visible);

	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([this]()
	{
		const TArray<FString> Added = FOnyxLevelTools::SetupCurrentLevel();
		Notify(Added.IsEmpty() ? INVTEXT("The level was already set up.") : FText::FromString(TEXT("Added: ") + FString::Join(Added, TEXT(", "))), true);
		RunValidation();
	})), NAME_None, INVTEXT("Set Up Level"),
		INVTEXT("Add what a 2D level needs: Onyx game mode, camera bounds, player start, 2D light, moon light and ambient light. Safe to press again."),
		FOnyxEditorStyle::Icon("Onyx2D.Tab.Level"));
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([this]()
	{
		FOnyxLevelValidator::ConfigureProjectFor2D();
		Notify(INVTEXT("Project configured for 2D. Restart the editor for distance-field / shadow changes."), true);
		RunValidation();
	})), NAME_None, INVTEXT("Configure Project"),
		INVTEXT("Turn off Lumen, virtual shadow maps, distance fields, motion blur, auto exposure and TAA, and make Onyx Game Mode the default."),
		FOnyxEditorStyle::Icon("Onyx2D.Project"));
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxLevelTool::RunValidation)),
		NAME_None, INVTEXT("Check Level"), INVTEXT("Look for common 2D setup problems."), FOnyxEditorStyle::Icon("Onyx2D.Validate"));
	Toolbar.AddSeparator();
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([]()
	{
		FRequestPlaySessionParams Params;
		GEditor->RequestPlaySession(Params);
	})), NAME_None, INVTEXT("Play"), INVTEXT("Play the level in a new window."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Play"));
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([]() { OpenProjectSettings("Onyx2D"); })),
		NAME_None, INVTEXT("Settings"), INVTEXT("Project Settings > Plugins > Onyx2D"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings"));
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([]()
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Onyx2D")))
		{
			FPlatformProcess::LaunchFileInDefaultExternalApplication(*FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("README.md")));
		}
	})), NAME_None, INVTEXT("Help"), INVTEXT("Open the Onyx2D README"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Help"));
	return Toolbar.MakeWidget();
}

TSharedRef<SWidget> SOnyxLevelTool::MakePlaceGrid()
{
	TSharedRef<SWrapBox> Grid = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
	for (const FOnyxPlaceable& Placeable : GetOnyxPlaceables())
	{
		Grid->AddSlot()[SNew(SOnyxPlaceTile, Placeable)];
	}
	return SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(8.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				Grid
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
			[
				Hint(INVTEXT("Actors are placed on the gameplay plane (Y = 0), snapped to a 50 cm grid. They are also in the Place Actors panel under Onyx2D."))
			]
		];
}

TSharedRef<ITableRow> SOnyxLevelTool::GenerateIssueRow(TSharedPtr<FOnyxValidationIssue> Issue, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(SOnyxIssueRow, Owner, Issue, [this, Issue]() { ApplyFix(Issue); });
}

void SOnyxLevelTool::RunValidation()
{
	Issues = FOnyxLevelValidator::Validate();
	Issues.StableSort([](const TSharedPtr<FOnyxValidationIssue>& A, const TSharedPtr<FOnyxValidationIssue>& B) { return A->Severity < B->Severity; });
	if (IssueList.IsValid())
	{
		IssueList->RequestListRefresh();
	}
}

void SOnyxLevelTool::ApplyFix(TSharedPtr<FOnyxValidationIssue> Issue)
{
	if (Issue.IsValid() && Issue->HasFix())
	{
		Issue->Fix();
		RunValidation();
	}
}

void SOnyxLevelTool::FixAll()
{
	// Fixes are idempotent (level setup only adds what is missing), so shared fixes can run more than once.
	for (const TSharedPtr<FOnyxValidationIssue>& Issue : TArray<TSharedPtr<FOnyxValidationIssue>>(Issues))
	{
		if (Issue->HasFix())
		{
			Issue->Fix();
		}
	}
	RunValidation();
	Notify(INVTEXT("Applied all fixes"), true);
}

FText SOnyxLevelTool::GetStatus() const
{
	int32 Errors = 0;
	int32 Warnings = 0;
	for (const TSharedPtr<FOnyxValidationIssue>& Issue : Issues)
	{
		Errors += Issue->Severity == EOnyxIssueSeverity::Error ? 1 : 0;
		Warnings += Issue->Severity == EOnyxIssueSeverity::Warning ? 1 : 0;
	}
	return FText::Format(INVTEXT("Level check: {0} errors, {1} warnings"), Errors, Warnings);
}
