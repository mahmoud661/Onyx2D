// Copyright Onyx2D. All Rights Reserved.

#include "OnyxPauseMenu.h"
#include "Components/BackgroundBlur.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "OnyxUISettings.h"
#include "OnyxUIStyle.h"

using namespace OnyxUI;

namespace
{
	constexpr float PauseBlurStrength = 7.f;

	void PauseMenuFill(UOverlaySlot* OverlaySlot)
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
	}
}

UOnyxPauseMenu::UOnyxPauseMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EntryIntroDelay = 0.06f;
	EntryStagger = 0.045f;
}

void UOnyxPauseMenu::BuildUI()
{
	const FOnyxUITheme& Style = Theme();

	UOverlay* Root = Make<UOverlay>();
	WidgetTree->RootWidget = Root;

	Blur = Make<UBackgroundBlur>();
	Blur->SetBlurStrength(0.f);
	PauseMenuFill(Root->AddChildToOverlay(Blur));

	UOverlay* BackdropLayer = Make<UOverlay>();
	BackdropLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
	PauseMenuFill(Root->AddChildToOverlay(BackdropLayer));
	Backdrop = BackdropLayer;
	PauseMenuFill(BackdropLayer->AddChildToOverlay(MakeImage(Black(0.35f), FVector2D(32.f, 32.f))));
	PauseMenuFill(BackdropLayer->AddChildToOverlay(MakeTexImage(GetVignetteTexture(), Black(0.75f))));
	PauseMenuFill(BackdropLayer->AddChildToOverlay(MakeTexImage(GetFadeTexture(), Black(0.8f))));

	UOverlay* Content = Make<UOverlay>();
	Content->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PauseMenuFill(Root->AddChildToOverlay(Content));
	MenuContent = Content;

	UVerticalBox* Column = Make<UVerticalBox>();
	if (UOverlaySlot* ColumnSlot = Content->AddChildToOverlay(Column))
	{
		ColumnSlot->SetHorizontalAlignment(HAlign_Left);
		ColumnSlot->SetVerticalAlignment(VAlign_Center);
		ColumnSlot->SetPadding(FMargin(150.f, 0.f, 0.f, 20.f));
	}

	Heading = MakeText(INVTEXT("Paused"), Font(EFont::Display, 76, 120), Style.TextBright);
	Heading->SetShadowOffset(FVector2D(0.f, 4.f));
	Heading->SetShadowColorAndOpacity(WithAlpha(Style.Accent, 0.18f));
	Column->AddChild(Heading);
	Column->AddChild(MakeSpacer(FVector2D(1.f, 14.f)));
	Divider = MakeTexImage(GetFadeTexture(), WithAlpha(Style.Accent, 0.6f), FVector2D(320.f, 2.f));
	Divider->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
	Column->AddChild(Divider);
	Column->AddChild(MakeSpacer(FVector2D(1.f, 28.f)));

	UVerticalBox* Menu = Make<UVerticalBox>();
	Column->AddChild(Menu);
	AddMenuEntry(Menu, INVTEXT("Resume"), [this]() { Resume(); });
	AddMenuEntry(Menu, INVTEXT("Settings"), [this]() { OpenSettings(); });
	if (!UOnyxUISettings::Get()->MainMenuLevel.IsNull())
	{
		AddMenuEntry(Menu, INVTEXT("Main Menu"), [this]()
		{
			ShowConfirm(INVTEXT("Return to Main Menu?"), INVTEXT("Unsaved progress will be lost."), INVTEXT("LEAVE"), [this]()
			{
				UGameplayStatics::SetGamePaused(this, false);
				UGameplayStatics::OpenLevelBySoftObjectPtr(this, UOnyxUISettings::Get()->MainMenuLevel);
			});
		});
	}
	AddMenuEntry(Menu, INVTEXT("Quit Game"), [this]()
	{
		ShowConfirm(INVTEXT("Quit Game?"), INVTEXT("Unsaved progress will be lost."), INVTEXT("QUIT"),
			[this]() { UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false); });
	}, true);

	UHorizontalBox* Hints = Make<UHorizontalBox>();
	Hints->AddChild(MakeKeyHint(INVTEXT("ENTER"), INVTEXT("Select")));
	Hints->AddChild(MakeSpacer(FVector2D(28.f, 1.f)));
	Hints->AddChild(MakeKeyHint(INVTEXT("ESC"), INVTEXT("Resume")));
	Footer = Hints;
	if (UOverlaySlot* HintsSlot = Content->AddChildToOverlay(Hints))
	{
		HintsSlot->SetHorizontalAlignment(HAlign_Left);
		HintsSlot->SetVerticalAlignment(VAlign_Bottom);
		HintsSlot->SetPadding(FMargin(150.f, 0.f, 0.f, 56.f));
	}

	PauseMenuFill(Root->AddChildToOverlay(BuildDialogLayer()));
}

void UOnyxPauseMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float In = SmoothStep01(Age / 0.3f);
	Blur->SetBlurStrength(PauseBlurStrength * In);
	Backdrop->SetRenderOpacity(In);
	const float HeadingIn = SmoothStep01(Age / 0.35f);
	Heading->SetRenderOpacity(HeadingIn);
	Heading->SetRenderTranslation(FVector2D(0.f, (1.f - HeadingIn) * 14.f));
	Divider->SetRenderScale(FVector2D(FMath::Max(SmoothStep01((Age - 0.08f) / 0.45f), 0.001f), 1.f));
	Footer->SetRenderOpacity(SmoothStep01((Age - 0.25f) / 0.4f));
}

FReply UOnyxPauseMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (!bDialogOpen && !IsSettingsOpen() && (Key == EKeys::Gamepad_Special_Right || Key == EKeys::P))
	{
		Resume();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UOnyxPauseMenu::Resume()
{
	APlayerController* PC = GetOwningPlayer();
	RemoveFromParent();
	UGameplayStatics::SetGamePaused(this, false);
	if (PC)
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}
	OnResumed.Broadcast();
}
