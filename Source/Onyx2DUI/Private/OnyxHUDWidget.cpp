// Copyright Onyx2D. All Rights Reserved.

#include "OnyxHUDWidget.h"
#include "Blueprint/WidgetTree.h"
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
	if (Health)
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
	DashBar->SetVisibility(bShowDash ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShowDash)
	{
		const float Ready = 1.f - Movement->GetDashCooldownFraction();
		if (Ready >= 1.f && LastDashFraction < 1.f)
		{
			DashFlash = 1.f;
		}
		LastDashFraction = Ready;
		DashFlash = FMath::FInterpTo(DashFlash, 0.f, InDeltaTime, 6.f);
		DashFill->SetWidthOverride(FMath::Max(DashBarWidth * Ready, 0.5f));
		DashBar->SetRenderOpacity(0.55f + 0.45f * FMath::Max(DashFlash, Ready >= 1.f ? 1.f : 0.f));
	}

	if (const UOnyxScoreSubsystem* Score = UOnyxScoreSubsystem::Get(this))
	{
		const int32 Coins = Score->GetCoins();
		if (Coins != LastCoins)
		{
			CoinText->SetText(FText::AsNumber(Coins));
			CoinPanel->SetVisibility(Coins > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			CoinPop = LastCoins >= 0 && Coins > LastCoins ? 1.35f : 1.f;
			LastCoins = Coins;
		}
		CoinPop = FMath::FInterpTo(CoinPop, 1.f, InDeltaTime, 12.f);
		CoinPanel->SetRenderScale(FVector2D(CoinPop));
	}
}
