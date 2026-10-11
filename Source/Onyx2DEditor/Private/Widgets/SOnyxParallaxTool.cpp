// Copyright Onyx2D. All Rights Reserved.

#include "Widgets/SOnyxParallaxTool.h"
#include "Assets/OnyxLevelTools.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "IDetailsView.h"
#include "ScopedTransaction.h"
#include "Style/OnyxEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/OnyxToolWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "World/OnyxParallaxBackground.h"

using namespace OnyxTools;

void SOnyxParallaxTool::Construct(const FArguments& InArgs)
{
	Details = MakeDetailsView(nullptr);
	ShowOnlyCategories(Details.ToSharedRef(), AOnyxParallaxBackground::StaticClass(), { TEXT("Onyx2D") });

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			ToolbarStrip(MakeToolbar())
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 2.f, 0.f, 0.f)
		[
			Section(INVTEXT("Parallax Background"),
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					Details.ToSharedRef()
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(420.f)
					.Visibility_Lambda([this]() { return Target.IsValid() ? EVisibility::Collapsed : EVisibility::Visible; })
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 8.f)
						[
							SNew(STextBlock).Text(INVTEXT("This level has no parallax background yet")).Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							Hint(INVTEXT("Press Add Background, then add layers back to front: a sky (Factor 1 = locked to the camera), far mountains (0.8), "
								"near trees (0.4)... Select textures in the Content Browser and press Add Layers to create one layer per texture."))
						]
					]
				])
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			StatusBar(TAttribute<FText>::CreateSP(this, &SOnyxParallaxTool::GetStatus))
		]
	];

	SelectionHandle = USelection::SelectionChangedEvent.AddSP(this, &SOnyxParallaxTool::HandleSelectionChanged);
	SetTarget(FindInLevel());
}

SOnyxParallaxTool::~SOnyxParallaxTool()
{
	USelection::SelectionChangedEvent.Remove(SelectionHandle);
}

TSharedRef<SWidget> SOnyxParallaxTool::MakeToolbar()
{
	FSlimHorizontalToolBarBuilder Toolbar(nullptr, FMultiBoxCustomization::None);
	Toolbar.SetLabelVisibility(EVisibility::Visible);
	const FCanExecuteAction HasTarget = FCanExecuteAction::CreateLambda([this]() { return Target.IsValid(); });

	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxParallaxTool::AddBackground)),
		NAME_None, INVTEXT("Add Background"), INVTEXT("Place a parallax background in the level (or select the existing one)."), FOnyxEditorStyle::Icon("Onyx2D.Tab.Parallax"));
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxParallaxTool::AddLayersFromSelection), HasTarget),
		NAME_None, INVTEXT("Add Layers"), INVTEXT("Add one layer per texture selected in the Content Browser, each further back."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"));
	Toolbar.AddSeparator();
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this, &SOnyxParallaxTool::FocusBackground), HasTarget),
		NAME_None, INVTEXT("Select"), INVTEXT("Select the background and frame it in the viewport."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Search"));
	Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([this]() { SetTarget(FindInLevel()); })),
		NAME_None, INVTEXT("Refresh"), INVTEXT("Find the background in the open level."), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"));
	return Toolbar.MakeWidget();
}

void SOnyxParallaxTool::SetTarget(AOnyxParallaxBackground* Background)
{
	Target = Background;
	Details->SetObject(Background, true);
}

AOnyxParallaxBackground* SOnyxParallaxTool::FindInLevel() const
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<AOnyxParallaxBackground> It(World);
	return It ? *It : nullptr;
}

void SOnyxParallaxTool::HandleSelectionChanged(UObject* Selection)
{
	if (USelection* Actors = Cast<USelection>(Selection))
	{
		if (AOnyxParallaxBackground* Background = Actors->GetTop<AOnyxParallaxBackground>())
		{
			SetTarget(Background);
		}
	}
	if (!Target.IsValid())
	{
		SetTarget(FindInLevel());
	}
}

void SOnyxParallaxTool::AddBackground()
{
	if (AOnyxParallaxBackground* Existing = FindInLevel())
	{
		SetTarget(Existing);
		FocusBackground();
		return;
	}
	if (AOnyxParallaxBackground* Background = Cast<AOnyxParallaxBackground>(FOnyxLevelTools::PlaceInFrontOfCamera(AOnyxParallaxBackground::StaticClass())))
	{
		SetTarget(Background);
		Notify(INVTEXT("Parallax background added. Add layers in the Details panel or with Add Layers."), true);
	}
}

void SOnyxParallaxTool::AddLayersFromSelection()
{
	AOnyxParallaxBackground* Background = Target.Get();
	TArray<FAssetData> Selection;
	FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().GetSelectedAssets(Selection);
	TArray<UTexture2D*> Textures;
	for (const FAssetData& Asset : Selection)
	{
		if (UTexture2D* Texture = Cast<UTexture2D>(Asset.GetAsset()))
		{
			Textures.Add(Texture);
		}
	}
	if (!Background || Textures.IsEmpty())
	{
		Notify(INVTEXT("Select one or more textures in the Content Browser first (far layers first)."), false);
		return;
	}

	const FScopedTransaction Transaction(INVTEXT("Add Parallax Layers"));
	Background->Modify();
	const int32 Existing = Background->BackgroundLayers.Num();
	for (int32 Index = 0; Index < Textures.Num(); ++Index)
	{
		// Far to near: the first texture is furthest back and moves least with the world.
		const float T = Textures.Num() > 1 ? float(Index) / (Textures.Num() - 1) : 0.5f;
		FOnyxParallaxLayer& Layer = Background->BackgroundLayers.AddDefaulted_GetRef();
		Layer.Name = *Textures[Index]->GetName();
		Layer.Texture = Textures[Index];
		Layer.Factor = FVector2D(FMath::Lerp(0.85f, 0.25f, T), FMath::Lerp(0.6f, 0.1f, T));
		Layer.Depth = FMath::Lerp(1600.f, 300.f, T);
		Layer.Height = FMath::Lerp(1800.f, 1200.f, T);
	}
	Background->RerunConstructionScripts();
	Details->ForceRefresh();
	Notify(FText::Format(INVTEXT("Added {0} layers ({1} in total)."), Textures.Num(), Existing + Textures.Num()), true);
}

void SOnyxParallaxTool::FocusBackground()
{
	if (AOnyxParallaxBackground* Background = Target.Get())
	{
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(Background, true, true);
		GEditor->MoveViewportCamerasToActor(*Background, false);
	}
}

FText SOnyxParallaxTool::GetStatus() const
{
	const AOnyxParallaxBackground* Background = Target.Get();
	return Background
		? FText::Format(INVTEXT("{0}: {1} layers. Factor 0 = moves with the world, 1 = locked to the camera."), FText::FromString(Background->GetActorLabel()), Background->BackgroundLayers.Num())
		: INVTEXT("No parallax background in this level.");
}
