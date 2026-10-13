// Copyright Onyx2D. All Rights Reserved.

#include "OnyxMainMenu.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "GeneralProjectSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/EngineVersion.h"
#include "OnyxUISettings.h"
#include "OnyxUIStyle.h"

using namespace OnyxUI;

namespace
{
	void MainMenuFill(UOverlaySlot* OverlaySlot)
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
	}
}

void UOnyxMainMenu::BuildUI()
{
	const FOnyxUITheme& Style = Theme();
	const UOnyxUISettings* Settings = UOnyxUISettings::Get();

	UOverlay* Root = Make<UOverlay>();
	WidgetTree->RootWidget = Root;

	UImage* Vignette = MakeTexImage(GetVignetteTexture(), Black(0.9f));
	Vignette->SetVisibility(ESlateVisibility::HitTestInvisible);
	MainMenuFill(Root->AddChildToOverlay(Vignette));
	UImage* Shade = MakeTexImage(GetFadeTexture(), Black(0.82f));
	Shade->SetVisibility(ESlateVisibility::HitTestInvisible);
	MainMenuFill(Root->AddChildToOverlay(Shade));

	UOverlay* Content = Make<UOverlay>();
	Content->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	MainMenuFill(Root->AddChildToOverlay(Content));
	MenuContent = Content;

	UVerticalBox* Column = Make<UVerticalBox>();
	if (UOverlaySlot* ColumnSlot = Content->AddChildToOverlay(Column))
	{
		ColumnSlot->SetHorizontalAlignment(HAlign_Left);
		ColumnSlot->SetVerticalAlignment(VAlign_Center);
		ColumnSlot->SetPadding(FMargin(150.f, 0.f, 0.f, 40.f));
	}

	Subtitle = MakeText(Settings->GameSubtitle, Font(EFont::Light, 18, 900), WithAlpha(Style.Accent, 0.9f));
	Cast<UVerticalBoxSlot>(Column->AddChild(Subtitle))->SetPadding(FMargin(6.f, 0.f, 0.f, 8.f));

	Title = MakeText(Settings->GameTitle, Font(EFont::Title, 120, 120), Style.TextBright);
	Title->SetShadowOffset(FVector2D(0.f, 5.f));
	Title->SetShadowColorAndOpacity(WithAlpha(Style.Accent, 0.25f));
	Column->AddChild(Title);

	Column->AddChild(MakeSpacer(FVector2D(1.f, 22.f)));
	Divider = MakeTexImage(GetFadeTexture(), WithAlpha(Style.Accent, 0.6f), FVector2D(340.f, 2.f));
	Divider->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
	Column->AddChild(Divider);
	Column->AddChild(MakeSpacer(FVector2D(1.f, 30.f)));

	UVerticalBox* Menu = Make<UVerticalBox>();
	Column->AddChild(Menu);
	AddMenuEntry(Menu, INVTEXT("Play"), [this]() { StartGame(); });
	AddMenuEntry(Menu, INVTEXT("Settings"), [this]() { OpenSettings(); });
	AddMenuEntry(Menu, INVTEXT("Quit"), [this]() { RequestQuit(); }, true);

	UOverlay* FooterLayer = Make<UOverlay>();
	FooterLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	MainMenuFill(Content->AddChildToOverlay(FooterLayer));
	Footer = FooterLayer;

	UHorizontalBox* Hints = Make<UHorizontalBox>();
	Hints->AddChild(MakeKeyHint(INVTEXT("ENTER"), INVTEXT("Select")));
	Hints->AddChild(MakeSpacer(FVector2D(28.f, 1.f)));
	Hints->AddChild(MakeKeyHint(INVTEXT("ESC"), INVTEXT("Back")));
	Hints->AddChild(MakeSpacer(FVector2D(28.f, 1.f)));
	Hints->AddChild(MakeKeyHint(INVTEXT("W / S"), INVTEXT("Navigate")));
	if (UOverlaySlot* HintsSlot = FooterLayer->AddChildToOverlay(Hints))
	{
		HintsSlot->SetHorizontalAlignment(HAlign_Left);
		HintsSlot->SetVerticalAlignment(VAlign_Bottom);
		HintsSlot->SetPadding(FMargin(150.f, 0.f, 0.f, 56.f));
	}

	const FText BuildInfo = FText::FromString(FString::Printf(TEXT("v%s   ·   UNREAL ENGINE %d.%d"),
		*GetDefault<UGeneralProjectSettings>()->ProjectVersion, FEngineVersion::Current().GetMajor(), FEngineVersion::Current().GetMinor()));
	if (UOverlaySlot* BuildSlot = FooterLayer->AddChildToOverlay(MakeText(BuildInfo, Font(EFont::Body, 12, 200), WithAlpha(Style.TextDim, 0.8f))))
	{
		BuildSlot->SetHorizontalAlignment(HAlign_Right);
		BuildSlot->SetVerticalAlignment(VAlign_Bottom);
		BuildSlot->SetPadding(FMargin(0.f, 0.f, 64.f, 58.f));
	}

	MainMenuFill(Root->AddChildToOverlay(BuildDialogLayer()));

	FadeOverlay = MakeImage(Black(1.f), FVector2D(32.f, 32.f));
	FadeOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	MainMenuFill(Root->AddChildToOverlay(FadeOverlay));
}

void UOnyxMainMenu::NativeConstruct()
{
	Super::NativeConstruct();
	// The fade covers the whole screen: it must never take the clicks meant for the buttons under it.
	if (FadeOverlay)
	{
		FadeOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UOnyxMainMenu::OnBackPressed()
{
	if (DialogLayer)
	{
		RequestQuit();
	}
}

void UOnyxMainMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const FOnyxUITheme& Style = Theme();

	// Each part is optional: a layout of its own may leave any of them out.
	if (Title)
	{
		const float TitleIn = SmoothStep01((Age - 0.05f) / 0.9f);
		Title->SetRenderOpacity(TitleIn);
		Title->SetRenderTranslation(FVector2D(0.f, (1.f - TitleIn) * 22.f));
		Title->SetShadowColorAndOpacity(WithAlpha(Style.Accent, 0.16f + 0.12f * FMath::Sin(Age * 1.3f)));
	}
	if (Subtitle)
	{
		const float SubIn = SmoothStep01((Age - 0.2f) / 0.7f);
		Subtitle->SetRenderOpacity(SubIn);
		Subtitle->SetRenderTranslation(FVector2D(0.f, (1.f - SubIn) * -10.f));
	}
	if (Divider)
	{
		Divider->SetRenderScale(FVector2D(FMath::Max(SmoothStep01((Age - 0.35f) / 0.8f), 0.001f), 1.f));
	}
	if (Footer)
	{
		Footer->SetRenderOpacity(SmoothStep01((Age - 0.9f) / 0.6f));
	}

	if (LeaveTime < 0.f)
	{
		if (FadeOverlay)
		{
			FadeOverlay->SetRenderOpacity(1.f - SmoothStep01(Age / 0.8f));
		}
		return;
	}
	LeaveTime += InDeltaTime;
	if (FadeOverlay)
	{
		FadeOverlay->SetRenderOpacity(SmoothStep01(LeaveTime / 0.8f));
	}
	// With nothing to fade, there is nothing to wait for.
	if (LeaveTime >= (FadeOverlay ? 0.9f : 0.f) && !bTravelled)
	{
		bTravelled = true;
		TravelToGame();
	}
}

void UOnyxMainMenu::StartGame()
{
	bInputLocked = true;
	LeaveTime = 0.f;
}

void UOnyxMainMenu::TravelToGame()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}
	const TSoftObjectPtr<UWorld>& Level = UOnyxUISettings::Get()->FirstGameLevel;
	if (!Level.IsNull())
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, Level);
	}
	else
	{
		RemoveFromParent(); // no game level configured: just play the current level
	}
}

void UOnyxMainMenu::RequestQuit()
{
	ShowConfirm(INVTEXT("Quit?"), INVTEXT("Are you sure you want to quit to the desktop?"), INVTEXT("QUIT"),
		[this]() { UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false); });
}
