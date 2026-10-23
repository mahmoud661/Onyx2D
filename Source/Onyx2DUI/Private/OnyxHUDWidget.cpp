// Copyright Onyx2D. All Rights Reserved.

#include "OnyxHUDWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Character/OnyxPlayerControlComponent.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Input/OnyxInputConfig.h"
#include "Items/OnyxInventoryComponent.h"
#include "Items/OnyxItem.h"
#include "Time/OnyxCalendarSubsystem.h"
#include "Logic/OnyxDialogueSubsystem.h"
#include "World/OnyxInteractable.h"
#include "AI/OnyxBossComponent.h"
#include "Combat/OnyxEnergyComponent.h"
#include "Core/OnyxWorldSubsystem.h"
#include "Misc/App.h"
#include "Combat/OnyxHealthComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Core/OnyxScoreSubsystem.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "Movement/OnyxMovementComponent.h"
#include "OnyxUISettings.h"
#include "OnyxUIStyle.h"

using namespace OnyxUI;

namespace
{
	constexpr float PipSize = 16.f;
	constexpr float DashBarWidth = 140.f;

	UImage* MakeSolid(UWidgetTree* Tree, const FLinearColor& Color, const FVector2D& Size, UTexture2D* Texture = nullptr)
	{
		UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.SetResourceObject(Texture);
		Image->SetBrush(Brush);
		Image->SetColorAndOpacity(Color);
		return Image;
	}
}

bool UOnyxHUDWidget::Initialize()
{
	const bool bResult = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildUI();
	}
	else if (Prompt && !PromptSlot)
	{
		// A layout of its own: the prompt follows the player only when it sits in a Canvas Panel.
		PromptSlot = Cast<UCanvasPanelSlot>(Prompt->Slot);
	}
	return bResult;
}

void UOnyxHUDWidget::BuildUI()
{
	const FOnyxUITheme& Style = Theme();
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Root;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column))
	{
		ColumnSlot->SetHorizontalAlignment(HAlign_Left);
		ColumnSlot->SetVerticalAlignment(VAlign_Top);
		ColumnSlot->SetPadding(FMargin(56.f, 48.f));
	}

	PipRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChild(PipRow);
	EnergyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Cast<UVerticalBoxSlot>(Column->AddChild(EnergyRow))->SetPadding(FMargin(2.f, 10.f, 0.f, 0.f));

	USizeBox* Bar = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Bar->SetWidthOverride(DashBarWidth);
	Bar->SetHeightOverride(3.f);
	UOverlay* BarLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Bar->AddChild(BarLayers);
	if (UOverlaySlot* BackSlot = BarLayers->AddChildToOverlay(MakeSolid(WidgetTree, WithAlpha(Style.TextDim, 0.35f), FVector2D(8.f, 3.f))))
	{
		BackSlot->SetHorizontalAlignment(HAlign_Fill);
		BackSlot->SetVerticalAlignment(VAlign_Fill);
	}
	DashFill = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DashFill->AddChild(MakeSolid(WidgetTree, Style.Accent, FVector2D(8.f, 3.f)));
	if (UOverlaySlot* FillSlot = BarLayers->AddChildToOverlay(DashFill))
	{
		FillSlot->SetHorizontalAlignment(HAlign_Left);
		FillSlot->SetVerticalAlignment(VAlign_Fill);
	}
	DashBar = Bar;
	Cast<UVerticalBoxSlot>(Column->AddChild(Bar))->SetPadding(FMargin(2.f, 14.f, 0.f, 0.f));

	// Coin counter, top right: diamond icon + count. Hidden until the first coin.
	UHorizontalBox* Coins = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UImage* CoinIcon = MakeSolid(WidgetTree, FLinearColor(1.f, 0.75f, 0.15f), FVector2D(12.f, 12.f));
	CoinIcon->SetRenderTransformAngle(45.f);
	if (UHorizontalBoxSlot* IconSlot = Coins->AddChildToHorizontalBox(CoinIcon))
	{
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
	}
	CoinText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CoinText->SetFont(Font(EFont::Bold, 22, 40));
	CoinText->SetColorAndOpacity(Style.TextBright);
	CoinText->SetShadowOffset(FVector2D(0.f, 2.f));
	CoinText->SetShadowColorAndOpacity(Black(0.6f));
	if (UHorizontalBoxSlot* TextSlot = Coins->AddChildToHorizontalBox(CoinText))
	{
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}
	Coins->SetRenderTransformPivot(FVector2D(1.f, 0.5f));
	Coins->SetVisibility(ESlateVisibility::Collapsed);
	CoinPanel = Coins;
	if (UOverlaySlot* CoinSlot = Root->AddChildToOverlay(Coins))
	{
		CoinSlot->SetHorizontalAlignment(HAlign_Right);
		CoinSlot->SetVerticalAlignment(VAlign_Top);
		CoinSlot->SetPadding(FMargin(56.f, 44.f));
		CoinOverlaySlot = CoinSlot;
	}

	BuildFarmUI(Root);

	// Held items (keys...), under the coins. Hidden while there are none.
	ItemsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ItemsText->SetFont(Font(EFont::Bold, 15, 60));
	ItemsText->SetColorAndOpacity(FLinearColor(0.45f, 0.88f, 1.f));
	ItemsText->SetJustification(ETextJustify::Right);
	ItemsText->SetShadowOffset(FVector2D(0.f, 2.f));
	ItemsText->SetShadowColorAndOpacity(Black(0.6f));
	ItemsText->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* ItemsSlot = Root->AddChildToOverlay(ItemsText))
	{
		ItemsSlot->SetHorizontalAlignment(HAlign_Right);
		ItemsSlot->SetVerticalAlignment(VAlign_Top);
		ItemsSlot->SetPadding(FMargin(56.f, 88.f, 56.f, 0.f));
	}

	// Interact prompt over the player: a key cap and a verb.
	UCanvasPanel* PromptLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	if (UOverlaySlot* LayerSlot = Root->AddChildToOverlay(PromptLayer))
	{
		LayerSlot->SetHorizontalAlignment(HAlign_Fill);
		LayerSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UHorizontalBox* PromptRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UBorder* KeyCap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	KeyCap->SetBrush(OutlineBrush(WithAlpha(Style.Panel, 0.9f), Style.Accent, 1.5f, 4.f));
	KeyCap->SetPadding(FMargin(8.f, 2.f));
	PromptKey = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	PromptKey->SetFont(Font(EFont::Bold, 14));
	PromptKey->SetColorAndOpacity(Style.TextBright);
	KeyCap->SetContent(PromptKey);
	if (UHorizontalBoxSlot* KeySlot = PromptRow->AddChildToHorizontalBox(KeyCap))
	{
		KeySlot->SetVerticalAlignment(VAlign_Center);
		KeySlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}
	PromptVerb = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	PromptVerb->SetFont(Font(EFont::Bold, 16, 30));
	PromptVerb->SetColorAndOpacity(Style.TextBright);
	PromptVerb->SetShadowOffset(FVector2D(0.f, 2.f));
	PromptVerb->SetShadowColorAndOpacity(Black(0.7f));
	if (UHorizontalBoxSlot* VerbSlot = PromptRow->AddChildToHorizontalBox(PromptVerb))
	{
		VerbSlot->SetVerticalAlignment(VAlign_Center);
	}
	PromptRow->SetRenderOpacity(0.f);
	Prompt = PromptRow;
	PromptSlot = PromptLayer->AddChildToCanvas(PromptRow);
	PromptSlot->SetAutoSize(true);
	PromptSlot->SetAlignment(FVector2D(0.5f, 1.f));
}

FText UOnyxHUDWidget::GetInteractKeyName() const
{
	const APlayerController* PC = GetOwningPlayer();
	const UOnyxInputSubsystem* Input = PC ? UOnyxInputSubsystem::Get(PC) : nullptr;
	const UOnyxInputConfig* Config = Input ? Input->GetInputConfig() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* Enhanced = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!Config || !Config->Interact || !Enhanced)
	{
		return INVTEXT("E");
	}
	const TArray<FKey> Keys = Enhanced->QueryKeysMappedToAction(Config->Interact);
	// The keyboard key when there is one: it is what most players press first.
	for (const FKey& Key : Keys)
	{
		if (!Key.IsGamepadKey())
		{
			return Key.GetDisplayName(false);
		}
	}
	return Keys.Num() > 0 ? Keys[0].GetDisplayName(false) : INVTEXT("E");
}

void UOnyxHUDWidget::UpdatePrompt(float DeltaTime)
{
	APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = GetOwningPlayerPawn();
	const UOnyxPlayerControlComponent* Control = Pawn ? Pawn->FindComponentByClass<UOnyxPlayerControlComponent>() : nullptr;
	// No prompt while a conversation is open (Interact turns its pages then).
	const UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(this);
	const bool bTalking = Dialogue && Dialogue->IsTalking();
	UObject* Target = Control && Control->IsControlEnabled() && !bTalking ? Control->FindInteractable() : nullptr;
	if (Target)
	{
		if (PromptVerb)
		{
			const FText Verb = IOnyxInteractable::Execute_GetInteractionPrompt(Target);
			PromptVerb->SetText(Verb.IsEmpty() ? INVTEXT("Use") : Verb);
		}
		KeyNameAge += DeltaTime;
		if (KeyNameAge > 1.f && PromptKey)
		{
			KeyNameAge = 0.f;
			PromptKey->SetText(GetInteractKeyName());
		}
	}
	PromptShown = FMath::FInterpConstantTo(PromptShown, Target ? 1.f : 0.f, DeltaTime, 8.f);
	if (!Prompt)
	{
		return;
	}
	// Collapsed when faded out: render opacity doesn't fade the key cap's outline.
	Prompt->SetVisibility(PromptShown > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Prompt->SetRenderOpacity(SmoothStep01(PromptShown));
	if (Pawn && PC && PromptSlot && PromptShown > 0.f)
	{
		// Just above the head, bobbing gently.
		const float Bob = FMath::Sin(GetWorld()->GetTimeSeconds() * 4.f) * 3.f;
		const FVector Above = Pawn->GetActorLocation() + FVector(0.f, 0.f, Pawn->GetSimpleCollisionHalfHeight() + 36.f);
		FVector2D Screen;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Above, Screen, false))
		{
			PromptSlot->SetPosition(Screen + FVector2D(0.f, Bob - (1.f - PromptShown) * 8.f));
		}
	}
}

void UOnyxHUDWidget::UpdateItems()
{
	const UOnyxScoreSubsystem* Score = UOnyxScoreSubsystem::Get(this);
	if (!Score)
	{
		return;
	}
	TArray<FString> Lines;
	for (const TPair<FName, int32>& Item : Score->GetItems())
	{
		const FString Name = FName::NameToDisplayString(Item.Key.ToString(), false).ToUpper();
		Lines.Add(Item.Value > 1 ? FString::Printf(TEXT("%s  x%d"), *Name, Item.Value) : Name);
	}
	Lines.Sort();
	const FString Joined = FString::Join(Lines, TEXT("\n"));
	if (Joined != ShownItems && ItemsText)
	{
		ShownItems = Joined;
		ItemsText->SetText(FText::FromString(Joined));
		ItemsText->SetVisibility(Joined.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UOnyxHUDWidget::RebuildPips(int32 Count)
{
	const FOnyxUITheme& Style = Theme();
	PipRow->ClearChildren();
	PipFills.Reset();
	PipScale.Reset();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		const FVector2D BoxSize = PipTexture ? PipTextureSize + FVector2D(4.f, 4.f) : FVector2D(PipSize + 10.f);
		Box->SetWidthOverride(BoxSize.X);
		Box->SetHeightOverride(BoxSize.Y);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Box->AddChild(Layers);

		UImage* Back;
		UImage* Fill;
		if (PipTexture)
		{
			// The picture twice: dimmed underneath, full on top (the top one shrinks away when the point is lost).
			Back = MakeSolid(WidgetTree, EmptyPipColor, PipTextureSize, PipTexture);
			Fill = MakeSolid(WidgetTree, FLinearColor::White, PipTextureSize, PipTexture);
		}
		else
		{
			Back = MakeSolid(WidgetTree, WithAlpha(Style.TextDim, 0.3f), FVector2D(PipSize, PipSize));
			Back->SetRenderTransformAngle(45.f);
			Fill = MakeSolid(WidgetTree, Style.Accent, FVector2D(PipSize - 4.f, PipSize - 4.f));
			Fill->SetRenderTransformAngle(45.f);
		}
		for (UImage* Image : { Back, Fill })
		{
			if (UOverlaySlot* PipSlot = Layers->AddChildToOverlay(Image))
			{
				PipSlot->SetHorizontalAlignment(HAlign_Center);
				PipSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
		Cast<UHorizontalBoxSlot>(PipRow->AddChild(Box))->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		PipFills.Add(Fill);
		PipScale.Add(1.f);
	}
}

void UOnyxHUDWidget::RebuildEnergyCells(int32 Count)
{
	EnergyRow->ClearChildren();
	EnergyFills.Reset();
	EnergyFillImages.Reset();
	EnergyGlow.Reset();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// A cell: its empty part underneath, the energy on top growing from the left.
		USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Cell->SetWidthOverride(EnergyCellSize.X);
		Cell->SetHeightOverride(EnergyCellSize.Y);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Cell->AddChild(Layers);
		if (UOverlaySlot* BackSlot = Layers->AddChildToOverlay(MakeSolid(WidgetTree, EmptyEnergyColor, EnergyCellSize)))
		{
			BackSlot->SetHorizontalAlignment(HAlign_Fill);
			BackSlot->SetVerticalAlignment(VAlign_Fill);
		}
		USizeBox* Fill = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Fill->SetWidthOverride(0.f);
		UImage* FillImage = MakeSolid(WidgetTree, EnergyColor, EnergyCellSize);
		Fill->AddChild(FillImage);
		if (UOverlaySlot* FillSlot = Layers->AddChildToOverlay(Fill))
		{
			FillSlot->SetHorizontalAlignment(HAlign_Left);
			FillSlot->SetVerticalAlignment(VAlign_Fill);
		}
		Cell->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Cast<UHorizontalBoxSlot>(EnergyRow->AddChild(Cell))->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		EnergyFills.Add(Fill);
		EnergyFillImages.Add(FillImage);
		EnergyGlow.Add(0.f);
	}
}

void UOnyxHUDWidget::UpdateEnergy(float DeltaTime)
{
	const APawn* Pawn = GetOwningPlayerPawn();
	const UOnyxEnergyComponent* Energy = Pawn ? Pawn->FindComponentByClass<UOnyxEnergyComponent>() : nullptr;
	if (!EnergyRow)
	{
		return;
	}
	EnergyRow->SetVisibility(Energy ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!Energy)
	{
		return;
	}
	const float CellEnergy = FMath::Max(Energy->bCanMend ? Energy->MendCost : Energy->MaxEnergy, 1.f);
	const int32 Count = FMath::Clamp(FMath::FloorToInt(Energy->MaxEnergy / CellEnergy + 0.01f), 1, 12);
	if (EnergyFills.Num() != Count)
	{
		RebuildEnergyCells(Count);
	}
	const float Current = Energy->GetEnergy();
	const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 14.f);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Filled = FMath::Clamp((Current - Index * CellEnergy) / CellEnergy, 0.f, 1.f);
		const float Before = LastEnergy >= 0.f ? FMath::Clamp((LastEnergy - Index * CellEnergy) / CellEnergy, 0.f, 1.f) : Filled;
		if (Filled >= 1.f && Before < 1.f)
		{
			EnergyGlow[Index] = 1.f; // just filled: a flash and a pop
		}
		EnergyGlow[Index] = FMath::FInterpTo(EnergyGlow[Index], 0.f, DeltaTime, 5.f);
		EnergyFills[Index]->SetWidthOverride(EnergyCellSize.X * Filled);
		// Full cells shine; the one a mend is draining flickers.
		const bool bDraining = Energy->IsMending() && Filled > 0.f && Filled < 1.f && Index == FMath::FloorToInt(Current / CellEnergy);
		const float Shine = Filled >= 1.f ? 1.f : bDraining ? 0.55f + 0.45f * Pulse : 0.6f;
		const FLinearColor Color = FMath::Lerp(EnergyColor * Shine, FLinearColor::White, EnergyGlow[Index]);
		EnergyFillImages[Index]->SetColorAndOpacity(FLinearColor(Color.R, Color.G, Color.B, EnergyColor.A));
		if (UWidget* Cell = EnergyRow->GetChildAt(Index))
		{
			Cell->SetRenderScale(FVector2D(1.f + 0.3f * EnergyGlow[Index]));
		}
	}
	LastEnergy = Current;
}

void UOnyxHUDWidget::EnsureBossUI()
{
	if (BossPanel || !WidgetTree || !WidgetTree->RootWidget)
	{
		return;
	}
	const FOnyxUITheme& Style = Theme();
	// The bar: the boss's name over a long thin bar along the bottom of the screen.
	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BossNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BossNameText->SetFont(Font(EFont::Bold, 15, 160));
	BossNameText->SetColorAndOpacity(Style.TextBright);
	BossNameText->SetShadowOffset(FVector2D(0.f, 2.f));
	BossNameText->SetShadowColorAndOpacity(Black(0.8f));
	if (UVerticalBoxSlot* NameSlot = Panel->AddChildToVerticalBox(BossNameText))
	{
		NameSlot->SetHorizontalAlignment(HAlign_Center);
		NameSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrush(OutlineBrush(FLinearColor(0.01f, 0.015f, 0.025f, 0.85f), WithAlpha(Style.TextDim, 0.6f), 1.f, 2.f));
	Frame->SetPadding(FMargin(2.f));
	USizeBox* Bar = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Bar->SetWidthOverride(BossBarSize.X);
	Bar->SetHeightOverride(BossBarSize.Y);
	Frame->SetContent(Bar);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Bar->AddChild(Layers);
	BossTrail = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BossTrail->AddChild(MakeSolid(WidgetTree, FLinearColor(1.f, 0.92f, 0.85f, 0.85f), BossBarSize));
	BossFill = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BossFill->AddChild(MakeSolid(WidgetTree, BossBarColor, BossBarSize));
	for (USizeBox* Part : { BossTrail.Get(), BossFill.Get() })
	{
		Part->SetWidthOverride(BossBarSize.X);
		if (UOverlaySlot* PartSlot = Layers->AddChildToOverlay(Part))
		{
			PartSlot->SetHorizontalAlignment(HAlign_Left);
			PartSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	if (UVerticalBoxSlot* BarSlot = Panel->AddChildToVerticalBox(Frame))
	{
		BarSlot->SetHorizontalAlignment(HAlign_Center);
	}
	Panel->SetVisibility(ESlateVisibility::Collapsed);
	BossPanel = Panel;

	// The title card: its title small over its name, in the middle of the screen, low.
	UVerticalBox* Card = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BossCardTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BossCardTitle->SetFont(Font(EFont::Bold, 18, 320));
	BossCardTitle->SetColorAndOpacity(Style.TextDim);
	BossCardName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BossCardName->SetFont(Font(EFont::Bold, 50, 220));
	BossCardName->SetColorAndOpacity(Style.TextBright);
	for (UTextBlock* Text : { BossCardTitle.Get(), BossCardName.Get() })
	{
		Text->SetShadowOffset(FVector2D(0.f, 3.f));
		Text->SetShadowColorAndOpacity(Black(0.85f));
		Text->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* TextSlot = Card->AddChildToVerticalBox(Text))
		{
			TextSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}
	Card->SetVisibility(ESlateVisibility::Collapsed);
	BossCard = Card;

	// Into whatever this HUD's layout is: a canvas (a layout of its own) or the built-in overlay.
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
	{
		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(0.5f, 1.f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 1.f));
		PanelSlot->SetPosition(FVector2D(0.f, -40.f));
		PanelSlot->SetAutoSize(true);
		UCanvasPanelSlot* CardSlot = Canvas->AddChildToCanvas(Card);
		CardSlot->SetAnchors(FAnchors(0.5f, 0.68f));
		CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CardSlot->SetAutoSize(true);
	}
	else if (UOverlay* Root = Cast<UOverlay>(WidgetTree->RootWidget))
	{
		if (UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel))
		{
			PanelSlot->SetHorizontalAlignment(HAlign_Center);
			PanelSlot->SetVerticalAlignment(VAlign_Bottom);
			PanelSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 40.f));
		}
		if (UOverlaySlot* CardSlot = Root->AddChildToOverlay(Card))
		{
			CardSlot->SetHorizontalAlignment(HAlign_Center);
			CardSlot->SetVerticalAlignment(VAlign_Center);
			CardSlot->SetPadding(FMargin(0.f, 260.f, 0.f, 0.f));
		}
	}
	else if (UPanelWidget* Other = Cast<UPanelWidget>(WidgetTree->RootWidget))
	{
		Other->AddChild(Panel);
		Other->AddChild(Card);
	}
}

void UOnyxHUDWidget::UpdateBoss(float DeltaTime)
{
	const UOnyxWorldSubsystem* Registry = GetWorld() ? GetWorld()->GetSubsystem<UOnyxWorldSubsystem>() : nullptr;
	const UOnyxBossComponent* Boss = Registry ? Registry->GetActiveBoss() : nullptr;
	if (!Boss || (!Boss->IsFightActive() && !BossPanel))
	{
		if (BossPanel)
		{
			BossPanel->SetVisibility(ESlateVisibility::Collapsed);
			BossCard->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	EnsureBossUI();
	if (!BossPanel)
	{
		return;
	}
	const float Time = Boss->GetFightTime(); // real seconds: a death's slow motion doesn't hold the bar up
	const float RealDelta = float(FMath::Min(FApp::GetDeltaTime(), 0.1));
	// The bar comes in as the title card goes, and leaves a little after the boss falls.
	const float Target = Boss->IsFightActive() ? (Time > 1.2f ? 1.f : 0.f) : (Boss->IsDefeated() && Time < 2.2f ? 1.f : 0.f);
	BossShown = FMath::FInterpConstantTo(BossShown, Target, RealDelta, 2.5f);
	BossPanel->SetVisibility(BossShown > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	BossPanel->SetRenderOpacity(SmoothStep01(BossShown));
	BossPanel->SetRenderTranslation(FVector2D(0.f, (1.f - SmoothStep01(BossShown)) * 12.f));
	BossNameText->SetText(FText::FromString(Boss->BossName.ToString().ToUpper()));
	const float Fraction = Boss->GetHealthFraction();
	if (Fraction < LastBossFraction)
	{
		BossTrailHold = 0.4f; // what was lost lingers a moment, then drains
	}
	if (Fraction > BossTrailFraction)
	{
		BossTrailFraction = Fraction; // whole again (a new attempt)
	}
	LastBossFraction = Fraction;
	BossTrailHold -= RealDelta;
	if (BossTrailHold <= 0.f)
	{
		BossTrailFraction = FMath::FInterpConstantTo(BossTrailFraction, Fraction, RealDelta, 0.6f);
	}
	BossFill->SetWidthOverride(FMath::Max(BossBarSize.X * Fraction, 0.f));
	BossTrail->SetWidthOverride(FMath::Max(BossBarSize.X * BossTrailFraction, 0.f));

	// The title card, as the fight begins: in, held, out.
	float Card = 0.f;
	if (Boss->IsFightActive())
	{
		Card = SmoothStep01(FMath::Clamp((Time - 0.6f) / 0.7f, 0.f, 1.f)) * (1.f - SmoothStep01(FMath::Clamp((Time - 3.7f) / 0.8f, 0.f, 1.f)));
	}
	BossCard->SetVisibility(Card > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	BossCard->SetRenderOpacity(Card);
	BossCard->SetRenderTranslation(FVector2D(0.f, (1.f - Card) * 10.f));
	BossCardTitle->SetText(FText::FromString(Boss->BossTitle.ToString().ToUpper()));
	BossCardName->SetText(FText::FromString(Boss->BossName.ToString().ToUpper()));
}

void UOnyxHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	EnsureFarmUI();
	UpdateHotbar();
	UpdateClock();

	const APawn* Pawn = GetOwningPlayerPawn();
	const UOnyxHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UOnyxHealthComponent>() : nullptr;
	if (Health && PipRow)
	{
		const int32 PipCount = FMath::CeilToInt(Health->MaxHealth);
		if (PipFills.Num() != PipCount)
		{
			RebuildPips(PipCount);
		}
		const float Current = Health->GetHealth();
		for (int32 Index = 0; Index < PipFills.Num(); ++Index)
		{
			const bool bFull = Index < FMath::CeilToInt(Current - KINDA_SMALL_NUMBER);
			const bool bJustLost = LastHealth >= 0.f && Index < FMath::CeilToInt(LastHealth) && !bFull;
			if (bJustLost)
			{
				PipScale[Index] = 1.8f; // pop as it empties
			}
			PipScale[Index] = FMath::FInterpTo(PipScale[Index], bFull ? 1.f : 0.f, InDeltaTime, 10.f);
			PipFills[Index]->SetRenderScale(FVector2D(PipScale[Index]));
		}
		LastHealth = Current;
	}

	const UOnyxMovementComponent* Movement = Pawn ? Pawn->FindComponentByClass<UOnyxMovementComponent>() : nullptr;
	const bool bShowDash = Movement && Movement->DashSettings.bEnabled;
	if (DashBar)
	{
		DashBar->SetVisibility(bShowDash ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bShowDash)
	{
		const float Ready = 1.f - Movement->GetDashCooldownFraction();
		if (Ready >= 1.f && LastDashFraction < 1.f)
		{
			DashFlash = 1.f;
		}
		LastDashFraction = Ready;
		DashFlash = FMath::FInterpTo(DashFlash, 0.f, InDeltaTime, 6.f);
		if (DashFill)
		{
			// The meter's own width when it has one (a Size Box with a Width Override, or its laid-out width).
			const USizeBox* BarBox = Cast<USizeBox>(DashBar);
			const float LaidOut = DashBar ? DashBar->GetCachedGeometry().GetLocalSize().X : 0.f;
			const float FullWidth = BarBox && BarBox->IsWidthOverride() ? BarBox->GetWidthOverride() : LaidOut > 1.f ? LaidOut : DashBarWidth;
			DashFill->SetWidthOverride(FMath::Max(FullWidth * Ready, 0.5f));
		}
		if (DashBar)
		{
			DashBar->SetRenderOpacity(0.55f + 0.45f * FMath::Max(DashFlash, Ready >= 1.f ? 1.f : 0.f));
		}
	}

	UpdateEnergy(InDeltaTime);
	UpdateBoss(InDeltaTime);
	UpdatePrompt(InDeltaTime);
	UpdateItems();

	if (const UOnyxScoreSubsystem* Score = UOnyxScoreSubsystem::Get(this))
	{
		const int32 Coins = Score->GetCoins();
		if (Coins != LastCoins)
		{
			if (CoinText)
			{
				CoinText->SetText(FText::AsNumber(Coins));
			}
			if (CoinPanel)
			{
				CoinPanel->SetVisibility(Coins > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			}
			CoinPop = LastCoins >= 0 && Coins > LastCoins ? 1.35f : 1.f;
			LastCoins = Coins;
		}
		CoinPop = FMath::FInterpTo(CoinPop, 1.f, InDeltaTime, 12.f);
		if (CoinPanel)
		{
			CoinPanel->SetRenderScale(FVector2D(CoinPop));
		}
	}
}

void UOnyxHUDWidget::RebuildHotbar(int32 Count)
{
	if (!HotbarRow)
	{
		return;
	}
	HotbarRow->ClearChildren();
	HotbarBacks.Reset();
	HotbarIcons.Reset();
	HotbarCounts.Reset();
	const UOnyxUISettings* UISettings = UOnyxUISettings::Get();
	const float Scale = float(UISettings->PixelArtScale);
	UTexture2D* SlotArt = UISettings->SlotTexture.LoadSynchronous();
	const FVector2D SlotSize = SlotArt ? FVector2D(SlotArt->GetSizeX(), SlotArt->GetSizeY()) * Scale : FVector2D(20.f, 20.f) * Scale;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(SlotSize.X);
		Box->SetHeightOverride(SlotSize.Y);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Box->AddChild(Layers);
		UImage* Back = MakeSolid(WidgetTree, SlotArt ? FLinearColor::White : Black(0.55f), SlotSize, SlotArt);
		Layers->AddChildToOverlay(Back);
		UImage* Icon = MakeSolid(WidgetTree, FLinearColor::White, FVector2D(16.f, 16.f) * Scale);
		Icon->SetVisibility(ESlateVisibility::Hidden);
		if (UOverlaySlot* IconSlot = Layers->AddChildToOverlay(Icon))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* CountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		CountText->SetFont(Font(EFont::Bold, 15, 0));
		CountText->SetColorAndOpacity(Theme().TextBright);
		CountText->SetShadowOffset(FVector2D(0.f, 2.f));
		CountText->SetShadowColorAndOpacity(Black(0.8f));
		if (UOverlaySlot* CountSlot = Layers->AddChildToOverlay(CountText))
		{
			CountSlot->SetHorizontalAlignment(HAlign_Right);
			CountSlot->SetVerticalAlignment(VAlign_Bottom);
			CountSlot->SetPadding(FMargin(0.f, 0.f, Scale * 1.5f, Scale * 0.5f));
		}
		if (UHorizontalBoxSlot* BoxSlot = HotbarRow->AddChildToHorizontalBox(Box))
		{
			BoxSlot->SetPadding(FMargin(Scale * 0.5f, 0.f));
		}
		HotbarBacks.Add(Back);
		HotbarIcons.Add(Icon);
		HotbarCounts.Add(CountText);
	}
}

void UOnyxHUDWidget::UpdateHotbar()
{
	const APawn* Pawn = GetOwningPlayerPawn();
	const UOnyxInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UOnyxInventoryComponent>() : nullptr;
	if (!HotbarRow)
	{
		return;
	}
	HotbarRow->SetVisibility(Inventory ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!Inventory)
	{
		return;
	}
	const TArray<FOnyxItemStack>& Slots = Inventory->GetSlots();
	const int32 Count = FMath::Min(Inventory->HotbarSize, Slots.Num());
	if (HotbarBacks.Num() != Count)
	{
		RebuildHotbar(Count);
	}
	const UOnyxUISettings* UISettings = UOnyxUISettings::Get();
	UTexture2D* SlotArt = UISettings->SlotTexture.LoadSynchronous();
	UTexture2D* SelectedArt = UISettings->SelectedSlotTexture.LoadSynchronous();
	for (int32 Index = 0; Index < Count && HotbarBacks.IsValidIndex(Index); ++Index)
	{
		const bool bSelected = Index == Inventory->GetSelectedSlot();
		UTexture2D* Back = bSelected && SelectedArt ? SelectedArt : SlotArt;
		if (Back)
		{
			if (HotbarBacks[Index]->GetBrush().GetResourceObject() != Back)
			{
				HotbarBacks[Index]->SetBrushFromTexture(Back, false);
			}
			HotbarBacks[Index]->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			HotbarBacks[Index]->SetColorAndOpacity(bSelected ? WithAlpha(Theme().Accent, 0.8f) : Black(0.55f));
		}
		const FOnyxItemStack& Stack = Slots[Index];
		UTexture2D* Icon = !Stack.IsEmpty() && Stack.Item ? Stack.Item->Icon.Get() : nullptr;
		if (Icon)
		{
			if (HotbarIcons[Index]->GetBrush().GetResourceObject() != Icon)
			{
				HotbarIcons[Index]->SetBrushFromTexture(Icon, false);
				HotbarIcons[Index]->SetDesiredSizeOverride(FVector2D(Icon->GetSizeX(), Icon->GetSizeY()) * float(UISettings->PixelArtScale));
			}
			HotbarIcons[Index]->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			HotbarIcons[Index]->SetVisibility(ESlateVisibility::Hidden);
		}
		const bool bShowCount = !Stack.IsEmpty() && Stack.Item && Stack.Item->MaxStack > 1;
		HotbarCounts[Index]->SetText(bShowCount ? FText::AsNumber(Stack.Count) : FText::GetEmpty());
	}
}

void UOnyxHUDWidget::UpdateClock()
{
	const UOnyxCalendarSubsystem* Calendar = UOnyxCalendarSubsystem::Get(this);
	const bool bShow = Calendar && Calendar->IsRunning() && ClockPanel;
	if (ClockPanel)
	{
		ClockPanel->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (CoinOverlaySlot)
	{
		CoinOverlaySlot->SetPadding(FMargin(56.f, bShow ? 150.f : 44.f, 56.f, 0.f));
	}
	if (!bShow)
	{
		return;
	}
	ClockDate->SetText(Calendar->GetDateText());
	ClockTime->SetText(Calendar->GetTimeText());
}

void UOnyxHUDWidget::BuildFarmUI(UOverlay* Layer)
{
	const FOnyxUITheme& Style = Theme();
	// The clock (Onyx Calendar), top right: the day and the time on a panel. Hidden while the calendar is off.
	{
		const UOnyxUISettings* UISettings = UOnyxUISettings::Get();
		// The clock a step larger than the hotbar: it is read at a glance.
		const float Scale = float(UISettings->PixelArtScale + 1);
		UTexture2D* ClockArt = UISettings->ClockTexture.LoadSynchronous();
		// Sized to the panel's art at the pixel scale; the words stay inside it.
		const FVector2D PanelSize = ClockArt && ClockArt->GetSizeX() > 0 ? FVector2D(ClockArt->GetSizeX(), ClockArt->GetSizeY()) * Scale : FVector2D(200.f, 92.f);
		USizeBox* ClockBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ClockBox->SetWidthOverride(PanelSize.X);
		ClockBox->SetHeightOverride(PanelSize.Y);
		UOverlay* Clock = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		ClockBox->AddChild(Clock);
		if (UOverlaySlot* ArtSlot = Clock->AddChildToOverlay(MakeSolid(WidgetTree, ClockArt ? FLinearColor::White : Black(0.55f), PanelSize, ClockArt)))
		{
			ArtSlot->SetHorizontalAlignment(HAlign_Fill);
			ArtSlot->SetVerticalAlignment(VAlign_Fill);
		}
		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		ClockDate = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		ClockTime = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		for (UTextBlock* Text : { ClockDate.Get(), ClockTime.Get() })
		{
			Text->SetFont(Font(EFont::Bold, FMath::RoundToInt((Text == ClockDate ? 3.f : 3.6f) * Scale), 10));
			Text->SetColorAndOpacity(Style.TextBright);
			Text->SetShadowOffset(FVector2D(0.f, 2.f));
			Text->SetShadowColorAndOpacity(Black(0.7f));
			Text->SetJustification(ETextJustify::Right);
			if (UVerticalBoxSlot* LineSlot = Lines->AddChildToVerticalBox(Text))
			{
				LineSlot->SetHorizontalAlignment(HAlign_Right);
			}
		}
		if (UOverlaySlot* LinesSlot = Clock->AddChildToOverlay(Lines))
		{
			// The panel's left third is its sun-and-moon window: the words sit in the part to the right of it.
			LinesSlot->SetHorizontalAlignment(HAlign_Right);
			LinesSlot->SetVerticalAlignment(VAlign_Center);
			LinesSlot->SetPadding(FMargin(PanelSize.X * 0.36f, 0.f, 4.f * Scale, 0.f));
		}
		ClockBox->SetVisibility(ESlateVisibility::Collapsed);
		ClockPanel = ClockBox;
		if (UOverlaySlot* ClockSlot = Layer->AddChildToOverlay(ClockBox))
		{
			ClockSlot->SetHorizontalAlignment(HAlign_Right);
			ClockSlot->SetVerticalAlignment(VAlign_Top);
			ClockSlot->SetPadding(FMargin(48.f, 36.f));
		}
	}

	// The hotbar (Onyx Inventory), bottom middle. Hidden for a player without an inventory.
	HotbarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HotbarRow->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* HotbarSlot = Layer->AddChildToOverlay(HotbarRow))
	{
		HotbarSlot->SetHorizontalAlignment(HAlign_Center);
		HotbarSlot->SetVerticalAlignment(VAlign_Bottom);
		HotbarSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
	}
}

void UOnyxHUDWidget::EnsureFarmUI()
{
	// A HUD with a layout of its own (a Widget Blueprint): the hotbar and the clock go on a layer laid over all of it.
	if (HotbarRow || !WidgetTree || !WidgetTree->RootWidget)
	{
		return;
	}
	UOverlay* Layer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Layer->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
	{
		if (UCanvasPanelSlot* LayerSlot = Canvas->AddChildToCanvas(Layer))
		{
			LayerSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			LayerSlot->SetOffsets(FMargin(0.f));
		}
	}
	else if (UOverlay* Overlay = Cast<UOverlay>(WidgetTree->RootWidget))
	{
		if (UOverlaySlot* LayerSlot = Overlay->AddChildToOverlay(Layer))
		{
			LayerSlot->SetHorizontalAlignment(HAlign_Fill);
			LayerSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	else
	{
		return;
	}
	BuildFarmUI(Layer);
}
