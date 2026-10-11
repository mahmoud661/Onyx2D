// Copyright Onyx2D. All Rights Reserved.

#include "Widgets/SOnyxImportTool.h"
#include "Assets/OnyxAssetFactory.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDesktopPlatform.h"
#include "Input/DragAndDrop.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Style/OnyxEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/OnyxToolWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

using namespace OnyxTools;

void SOnyxImportTool::Construct(const FArguments& InArgs)
{
	for (const EOnyxArtKind Kind : { EOnyxArtKind::TileSet, EOnyxArtKind::Character, EOnyxArtKind::Sprite, EOnyxArtKind::Background, EOnyxArtKind::Skip })
	{
		Kinds.Add(MakeShared<EOnyxArtKind>(Kind));
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			ToolbarStrip(MakeToolbar())
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 2.f, 0.f, 0.f)
		[
			Section(INVTEXT("Drop Art Here"),
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(List, SListView<FItemPtr>)
					.ListItemsSource(&Items)
					.SelectionMode(ESelectionMode::None)
					.OnGenerateRow(this, &SOnyxImportTool::MakeRow)
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					MakeEmptyHint()
				]
				+ SOverlay::Slot()
				[
					// Highlight while files are dragged over the tool.
					SNew(SBorder)
					.Visibility_Lambda([this]() { return bDragOver ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(Accent().CopyWithNewOpacity(0.12f))
					.HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(INVTEXT("Release to add")).Font(Font(16, true)).ColorAndOpacity(Accent())
					]
				],
				MakeSettings())
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			StatusBar(TAttribute<FText>::CreateSP(this, &SOnyxImportTool::GetStatus))
		]
	];
}

TSharedRef<SWidget> SOnyxImportTool::MakeToolbar()
{
	FSlimHorizontalToolBarBuilder Toolbar(nullptr, FMultiBoxCustomization::None);
	Toolbar.SetLabelVisibility(EVisibility::Visible);
	const FCanExecuteAction HasItems = FCanExecuteAction::CreateLambda([this]() { return !Items.IsEmpty(); });

	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxImportTool::AddFiles)),
		NAME_None, INVTEXT("Add Files"), INVTEXT("Pick PNG files: tile sheets, animation frames or strips, sprite sheets, props, backgrounds."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"));
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxImportTool::AddFolder)),
		NAME_None, INVTEXT("Add Folder"), INVTEXT("Pick a folder (an art pack, a character's folder...): every PNG inside is sorted out."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderOpen"));
	Toolbar.AddSeparator();
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxImportTool::BuildAll), HasItems),
		NAME_None, INVTEXT("Build All"), INVTEXT("Create every tile set, character, sprite and background in the list."),
		FOnyxEditorStyle::Icon("Onyx2D.Tab.Import"));
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxImportTool::Clear), HasItems),
		NAME_None, INVTEXT("Clear"), INVTEXT("Empty the list (assets already built are kept)."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"));
	return Toolbar.MakeWidget();
}

TSharedRef<SWidget> SOnyxImportTool::MakeSettings()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
		[
			SNew(STextBlock).Text(INVTEXT("Folder"))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(160.f)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([this]() { return FText::FromString(Settings.Folder); })
				.ToolTipText(INVTEXT("Content folder for the new assets; every item gets its own sub-folder."))
				.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { Settings.Folder = Text.ToString(); })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 4.f, 0.f)
		[
			SNew(STextBlock).Text(INVTEXT("Pixel Size"))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(60.f)
			[
				SNew(SNumericEntryBox<float>)
				.Value_Lambda([this]() { return Settings.CentimetersPerPixel; })
				.MinValue(0.1f)
				.ToolTipText(INVTEXT("World size of one art pixel in cm. Tiles, characters and sprites share it so their pixels match (4 = default)."))
				.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { Settings.CentimetersPerPixel = FMath::Max(Value, 0.1f); })
			]
		];
}

TSharedRef<SWidget> SOnyxImportTool::MakeEmptyHint()
{
	return SNew(SBox)
		.WidthOverride(460.f)
		.Visibility_Lambda([this]() { return Items.IsEmpty() && !bDragOver ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(SImage).Image(FOnyxEditorStyle::Get().GetBrush("Onyx2D.Tab.Import")).DesiredSizeOverride(FVector2D(40.f, 40.f))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(STextBlock).Text(INVTEXT("Drop PNG files or folders here")).Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Hint(INVTEXT("Onyx2D sorts them out and builds everything:\n"
					"- Tile sheets become tile sets: tile size and gaps detected, padded (no seams), normal-mapped for 2D lights, solid tiles get collision.\n"
					"- Animations become a playable character: frames (run-1.png, run-2.png...), one strip per animation (idle.png, run.png) "
					"or a sheet with one animation per row. Words like idle, run, jump, fall, attack, hurt, death pick the animation; "
					"the rest of the name (or the folder) groups them per character.\n"
					"- Other images become sprites (numbered frames = animated) or background textures.\n"
					"Check the list, change anything that was guessed wrong, then press Build All."))
			]
		];
}

TSharedRef<SWidget> SOnyxImportTool::MakeKindPicker(FItemPtr Item)
{
	TSharedPtr<EOnyxArtKind> Selected;
	for (const TSharedPtr<EOnyxArtKind>& Kind : Kinds)
	{
		Selected = *Kind == Item->Kind ? Kind : Selected;
	}
	return SNew(SBox).WidthOverride(110.f)
	[
		SNew(SComboBox<TSharedPtr<EOnyxArtKind>>)
		.OptionsSource(&Kinds)
		.InitiallySelectedItem(Selected)
		.OnGenerateWidget_Lambda([](TSharedPtr<EOnyxArtKind> Kind) { return SNew(STextBlock).Text(FOnyxArtImporter::GetKindName(*Kind)); })
		.OnSelectionChanged_Lambda([this, Item](TSharedPtr<EOnyxArtKind> Kind, ESelectInfo::Type)
		{
			if (!Kind.IsValid() || *Kind == Item->Kind)
			{
				return;
			}
			// A character needs animations; a single image can't become one here (use the Character Wizard for sheets).
			if (*Kind == EOnyxArtKind::Character && Item->Animations.IsEmpty())
			{
				Notify(INVTEXT("This image has no animation frames. Use the Character Wizard tab for unusual sheets."), false);
				Refresh();
				return;
			}
			// Single-image kinds read the first file; a tile set also gets its grid detected.
			if (Item->ImageFile.IsEmpty() && !Item->Animations.IsEmpty())
			{
				Item->ImageFile = Item->Animations[0].Files[0];
			}
			TArray<FColor> Pixels;
			if (!Item->ImageFile.IsEmpty() && FOnyxArtImporter::LoadPixels(Item->ImageFile, Pixels, Item->ImageSize) && *Kind == EOnyxArtKind::TileSet)
			{
				FOnyxArtImporter::DetectTileGrid(Item->ImageFile, Pixels, Item->ImageSize, Item->TileSize, Item->Margin, Item->Spacing);
			}
			Item->Kind = *Kind;
			Item->Result.Reset();
			Refresh();
		})
		[
			SNew(STextBlock).Text_Lambda([Item]() { return FOnyxArtImporter::GetKindName(Item->Kind); })
		]
	];
}

TSharedRef<SWidget> SOnyxImportTool::MakeItemOptions(FItemPtr Item)
{
	if (Item->Kind == EOnyxArtKind::TileSet)
	{
		auto SizeBox = [Item](const FText& Label, int32* Value, const FText& Tooltip, int32 Min)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 3.f, 0.f)
				[
					SNew(STextBlock).Text(Label).ColorAndOpacity(Muted())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(44.f)
					[
						SNew(SNumericEntryBox<int32>)
						.ToolTipText(Tooltip)
						.MinValue(Min)
						.Value_Lambda([Value]() { return *Value; })
						.OnValueCommitted_Lambda([Item, Value, Min](int32 NewValue, ETextCommit::Type) { *Value = FMath::Max(NewValue, Min); Item->Result.Reset(); })
					]
				];
		};
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[SizeBox(INVTEXT("Tile"), &Item->TileSize.X, INVTEXT("Tile width in pixels."), 1)]
			+ SHorizontalBox::Slot().AutoWidth()[SizeBox(INVTEXT("x"), &Item->TileSize.Y, INVTEXT("Tile height in pixels."), 1)]
			+ SHorizontalBox::Slot().AutoWidth()[SizeBox(INVTEXT("Gap"), &Item->Spacing, INVTEXT("Pixels between tiles in the sheet."), 0)]
			+ SHorizontalBox::Slot().AutoWidth()[SizeBox(INVTEXT("Border"), &Item->Margin, INVTEXT("Pixels around the whole sheet."), 0)];
	}
	if (Item->Kind == EOnyxArtKind::Character)
	{
		return SNew(SCheckBox)
			.ToolTipText(INVTEXT("Player: controlled by the player, with the Onyx camera. Off: an enemy that patrols and hurts on contact."))
			.IsChecked_Lambda([Item]() { return Item->bPlayer ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Item](ECheckBoxState State) { Item->bPlayer = State == ECheckBoxState::Checked; Item->Result.Reset(); })
			[
				SNew(STextBlock).Text(INVTEXT("Player"))
			];
	}
	return SNullWidget::NullWidget;
}

TSharedRef<ITableRow> SOnyxImportTool::MakeRow(FItemPtr Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(STableRow<FItemPtr>, Owner)
		.Padding(FMargin(4.f, 3.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				MakeKindPicker(Item)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)
			[
				SNew(SBox).WidthOverride(150.f)
				[
					SNew(SEditableTextBox)
					.Text_Lambda([Item]() { return FText::FromString(Item->Name); })
					.ToolTipText(INVTEXT("Asset name (the sub-folder and assets are named after it)."))
					.OnTextCommitted_Lambda([Item](const FText& Text, ETextCommit::Type)
					{
						const FString Name = FOnyxAssetFactory::SanitizeName(Text.ToString().TrimStartAndEnd());
						if (Name != Item->Name)
						{
							Item->Name = Name;
							Item->Result.Reset();
						}
					})
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text_Lambda([Item]() { return Item->Describe(); })
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(Item->Source)).ColorAndOpacity(Muted()).Font(Font(8))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)
			[
				MakeItemOptions(Item)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([Item]() { return Item->Result.IsValid() ? INVTEXT("Built") : FText::GetEmpty(); })
				.ColorAndOpacity(FLinearColor(0.35f, 0.85f, 0.4f))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(INVTEXT("Remove from the list"))
				.OnClicked_Lambda([this, Item]()
				{
					Items.Remove(Item);
					Refresh();
					return FReply::Handled();
				})
				[
					SNew(SImage).Image(FAppStyle::GetBrush("Icons.X")).ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		];
}

void SOnyxImportTool::AddPaths(const TArray<FString>& Paths)
{
	if (Paths.IsEmpty())
	{
		return;
	}
	LastDirectory = FPaths::GetPath(Paths[0]);
	TArray<FItemPtr> Found = FOnyxArtImporter::Analyze(Paths);
	if (Found.IsEmpty())
	{
		Notify(INVTEXT("No PNG images found there."), false);
		return;
	}
	// Dropping the same art again replaces its entries instead of doubling them.
	Items.RemoveAll([&Found](const FItemPtr& Existing)
	{
		return Found.ContainsByPredicate([&Existing](const FItemPtr& New) { return New->Source == Existing->Source && New->Kind == Existing->Kind; });
	});
	Items.Append(Found);
	Refresh();
}

void SOnyxImportTool::AddFiles()
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	TArray<FString> Files;
	if (Desktop && Desktop->OpenFileDialog(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()), TEXT("Add art (PNG)"),
		LastDirectory, TEXT(""), TEXT("PNG images (*.png)|*.png"), EFileDialogFlags::Multiple, Files))
	{
		AddPaths(Files);
	}
}

void SOnyxImportTool::AddFolder()
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	FString Folder;
	if (Desktop && Desktop->OpenDirectoryDialog(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()), TEXT("Add an art folder"), LastDirectory, Folder))
	{
		AddPaths({ Folder });
	}
}

void SOnyxImportTool::BuildAll()
{
	TArray<FItemPtr> ToBuild = Items.FilterByPredicate([](const FItemPtr& Item) { return Item->Kind != EOnyxArtKind::Skip && !Item->Result.IsValid(); });
	if (ToBuild.IsEmpty())
	{
		Notify(INVTEXT("Everything in the list is already built."), true);
		return;
	}
	FScopedSlowTask Progress(ToBuild.Num(), INVTEXT("Building art..."));
	Progress.MakeDialog();

	TArray<UObject*> Built;
	TArray<FString> Failed;
	for (const FItemPtr& Item : ToBuild)
	{
		Progress.EnterProgressFrame(1.f, FText::Format(INVTEXT("{0} {1}"), FOnyxArtImporter::GetKindName(Item->Kind), FText::FromString(Item->Name)));
		FText Error;
		if (FOnyxArtImporter::Build(*Item, Settings, Error) && Item->Result.IsValid())
		{
			Built.Add(Item->Result.Get());
		}
		else
		{
			Failed.Add(FString::Printf(TEXT("%s: %s"), *Item->Name, *Error.ToString()));
		}
	}
	if (GEditor && !Built.IsEmpty())
	{
		GEditor->SyncBrowserToObjects(Built);
	}
	if (Failed.IsEmpty())
	{
		Notify(FText::Format(INVTEXT("Built {0} asset(s) in {1}. Characters are ready to drop in the level; paint tile sets in the Level Painter."),
			Built.Num(), FText::FromString(Settings.Folder)), true);
	}
	else
	{
		Notify(FText::FromString(FString::Printf(TEXT("Built %d, failed %d:\n%s"), Built.Num(), Failed.Num(), *FString::Join(Failed, TEXT("\n")))), false);
	}
	Refresh();
}

void SOnyxImportTool::Clear()
{
	Items.Reset();
	Refresh();
}

void SOnyxImportTool::Refresh()
{
	if (List.IsValid())
	{
		List->RebuildList();
	}
}

FText SOnyxImportTool::GetStatus() const
{
	if (Items.IsEmpty())
	{
		return INVTEXT("Drop PNG files or folders from the file explorer, or use Add Files / Add Folder.");
	}
	const int32 Built = Items.FilterByPredicate([](const FItemPtr& Item) { return Item->Result.IsValid(); }).Num();
	return FText::Format(INVTEXT("{0} item(s), {1} built. Change a guess with the type menu, then press Build All."), Items.Num(), Built);
}

void SOnyxImportTool::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const TSharedPtr<FExternalDragOperation> Files = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	bDragOver = Files.IsValid() && Files->HasFiles();
}

void SOnyxImportTool::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	bDragOver = false;
}

FReply SOnyxImportTool::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const TSharedPtr<FExternalDragOperation> Files = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	return Files.IsValid() && Files->HasFiles() ? FReply::Handled() : FReply::Unhandled();
}

FReply SOnyxImportTool::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	bDragOver = false;
	const TSharedPtr<FExternalDragOperation> Files = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	if (!Files.IsValid() || !Files->HasFiles())
	{
		return FReply::Unhandled();
	}
	AddPaths(Files->GetFiles());
	return FReply::Handled();
}
