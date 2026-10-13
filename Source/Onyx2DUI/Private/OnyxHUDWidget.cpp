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
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Input/OnyxInputConfig.h"
#include "Logic/OnyxDialogueSubsystem.h"
#include "World/OnyxInteractable.h"
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

	UImage* MakeSolid(UWidgetTree* Tree, const FLinearColor& Color, const FVector2D& Size)
	{
		UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
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
	}

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
		Box->SetWidthOverride(PipSize + 10.f);
		Box->SetHeightOverride(PipSize + 10.f);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Box->AddChild(Layers);

		UImage* Back = MakeSolid(WidgetTree, WithAlpha(Style.TextDim, 0.3f), FVector2D(PipSize, PipSize));
		Back->SetRenderTransformAngle(45.f);
		UImage* Fill = MakeSolid(WidgetTree, Style.Accent, FVector2D(PipSize - 4.f, PipSize - 4.f));
		Fill->SetRenderTransformAngle(45.f);
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

void UOnyxHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

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
