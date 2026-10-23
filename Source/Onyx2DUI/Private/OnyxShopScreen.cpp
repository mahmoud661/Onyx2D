// Copyright Onyx2D. All Rights Reserved.

#include "OnyxShopScreen.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Core/OnyxScoreSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Items/OnyxItem.h"
#include "Kismet/GameplayStatics.h"
#include "OnyxUISettings.h"
#include "OnyxUIStyle.h"
#include "World/OnyxShop.h"

using namespace OnyxUI;

void UOnyxShopScreen::SetShop(AOnyxShop* InShop, AActor* InBuyer)
{
	Shop = InShop;
	Buyer = InBuyer;
	FillMenu();
}

void UOnyxShopScreen::FillMenu()
{
	const AOnyxShop* Live = Shop.Get();
	if (TitleText)
	{
		TitleText->SetText(Live ? Live->ShopName : INVTEXT("Shop"));
	}
	if (!MenuBox)
	{
		return;
	}
	MenuBox->ClearChildren();
	Entries.Reset();
	if (Live)
	{
		for (int32 Index = 0; Index < Live->Stock.Num(); ++Index)
		{
			const UOnyxItem* Item = Live->Stock[Index];
			if (!Item)
			{
				continue;
			}
			const FText Label = FText::Format(INVTEXT("{0}   {1}"), Item->DisplayName, FText::AsNumber(Live->GetPrice(Item)));
			AddMenuEntry(MenuBox, Label, [this, Index]() { BuyEntry(Index); });
		}
	}
	AddMenuEntry(MenuBox, INVTEXT("Leave"), [this]() { Close(); });
}

void UOnyxShopScreen::BuildUI()
{
	const FOnyxUITheme& Style = Theme();
	UOverlay* Root = Make<UOverlay>();
	WidgetTree->RootWidget = Root;
	if (UOverlaySlot* BackSlot = Root->AddChildToOverlay(MakeImage(Black(0.55f), FVector2D(32.f, 32.f))))
	{
		BackSlot->SetHorizontalAlignment(HAlign_Fill);
		BackSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UVerticalBox* Column = Make<UVerticalBox>();
	if (UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column))
	{
		ColumnSlot->SetHorizontalAlignment(HAlign_Center);
		ColumnSlot->SetVerticalAlignment(VAlign_Center);
	}
	TitleText = MakeText(INVTEXT("Shop"), Font(EFont::Display, 54, 80), Style.TextBright);
	Column->AddChild(TitleText);
	CoinsText = MakeText(FText::GetEmpty(), Font(EFont::Bold, 22, 20), FLinearColor(1.f, 0.8f, 0.35f));
	Column->AddChild(CoinsText);
	Column->AddChild(MakeSpacer(FVector2D(1.f, 24.f)));
	MenuBox = Make<UVerticalBox>();
	Column->AddChild(MenuBox);
	FillMenu();
	Column->AddChild(MakeSpacer(FVector2D(1.f, 16.f)));
	NoteText = MakeText(FText::GetEmpty(), Font(EFont::Bold, 18, 10), Style.TextDim);
	Column->AddChild(NoteText);
}

void UOnyxShopScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (CoinsText)
	{
		const UOnyxScoreSubsystem* Score = UOnyxScoreSubsystem::Get(this);
		CoinsText->SetText(FText::Format(INVTEXT("Coins: {0}"), FText::AsNumber(Score ? Score->GetCoins() : 0)));
	}
	NoteAge += InDeltaTime;
	if (NoteText)
	{
		NoteText->SetRenderOpacity(FMath::Clamp(2.5f - NoteAge, 0.f, 1.f));
	}
}

void UOnyxShopScreen::BuyEntry(int32 Index)
{
	AOnyxShop* Live = Shop.Get();
	const bool bBought = Live && Live->Buy(Index, Buyer.Get());
	PlayUISound(bBought);
	if (NoteText && Live && Live->Stock.IsValidIndex(Index) && Live->Stock[Index])
	{
		NoteText->SetText(bBought ? FText::Format(INVTEXT("Bought {0}."), Live->Stock[Index]->DisplayName) : INVTEXT("Not enough coins (or no room to carry it)."));
		NoteAge = 0.f;
	}
}

void UOnyxShopScreen::Close()
{
	APlayerController* PC = GetOwningPlayer();
	RemoveFromParent();
	UGameplayStatics::SetGamePaused(this, false);
	if (PC)
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}
}
