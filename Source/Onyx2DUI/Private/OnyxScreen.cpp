// Copyright Onyx2D. All Rights Reserved.

#include "OnyxScreen.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "OnyxButton.h"
#include "OnyxSettingsMenu.h"
#include "OnyxUISettings.h"
#include "OnyxUIStyle.h"
#include "Sound/SoundBase.h"
#include "Widgets/SViewport.h"

using namespace OnyxUI;

UOnyxScreen::UOnyxScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

bool UOnyxScreen::Initialize()
{
	const bool bResult = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildUI();
	}
	return bResult;
}

void UOnyxScreen::NativeConstruct()
{
	Super::NativeConstruct();
	Age = 0.f;
	FocusDefault();
}

void UOnyxScreen::FocusDefault()
{
	if (Entries.Num() > 0)
	{
		FocusEntry(FMath::Clamp(LastFocusedEntry, 0, Entries.Num() - 1));
	}
}

void UOnyxScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Age += InDeltaTime;
	TickEntries(InDeltaTime);

	if (DialogLayer)
	{
		DialogAlpha = FMath::FInterpTo(DialogAlpha, bDialogOpen ? 1.f : 0.f, InDeltaTime, 16.f);
		DialogLayer->SetRenderOpacity(DialogAlpha);
		if (DialogPanel)
		{
			DialogPanel->SetRenderTranslation(FVector2D(0.f, (1.f - DialogAlpha) * 18.f));
		}
		if (!bDialogOpen && DialogAlpha < 0.01f && DialogLayer->GetVisibility() != ESlateVisibility::Collapsed)
		{
			DialogLayer->SetVisibility(ESlateVisibility::Collapsed);
		}
		TickFramedButton(DialogConfirm, InDeltaTime);
		TickFramedButton(DialogCancel, InDeltaTime);
	}

	if (bInputLocked || IsSettingsOpen() || !CanReclaimFocus())
	{
		return;
	}

	if (bDialogOpen)
	{
		if (DialogCancel.Button && DialogConfirm.Button && !DialogCancel.Button->HasKeyboardFocus() && !DialogConfirm.Button->HasKeyboardFocus())
		{
			DialogCancel.Button->SetKeyboardFocus();
		}
		return;
	}

	if (Entries.Num() > 0)
	{
		bool bAnyFocused = false;
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			if (Entries[Index].Button && Entries[Index].Button->HasKeyboardFocus())
			{
				bAnyFocused = true;
				if (Index != LastFocusedEntry)
				{
					LastFocusedEntry = Index;
					PlayUISound(false);
				}
				break;
			}
		}
		if (!bAnyFocused)
		{
			FocusEntry(LastFocusedEntry);
		}
	}
}

FReply UOnyxScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bInputLocked)
	{
		return FReply::Handled();
	}

	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		if (bDialogOpen)
		{
			PlayUISound(false);
			CloseConfirm();
		}
		else
		{
			OnBackPressed();
		}
		return FReply::Handled();
	}

	if (!bDialogOpen && !IsSettingsOpen() && Entries.Num() > 0 && (Key == EKeys::W || Key == EKeys::S))
	{
		FocusEntry(FMath::Clamp(LastFocusedEntry + (Key == EKeys::W ? -1 : 1), 0, Entries.Num() - 1));
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ---------------------------------------------------------------------------
// Factory helpers
// ---------------------------------------------------------------------------

UTextBlock* UOnyxScreen::MakeText(const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color)
{
	UTextBlock* Block = Make<UTextBlock>();
	Block->SetText(Text);
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	return Block;
}

UImage* UOnyxScreen::MakeImage(const FLinearColor& Color, const FVector2D& Size)
{
	UImage* Image = Make<UImage>();
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = Size;
	Image->SetBrush(Brush);
	Image->SetColorAndOpacity(Color);
	return Image;
}

UImage* UOnyxScreen::MakeTexImage(UTexture2D* Texture, const FLinearColor& Color, const FVector2D& Size)
{
	UImage* Image = Make<UImage>();
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetResourceObject(Texture);
	Brush.ImageSize = Size;
	Image->SetBrush(Brush);
	Image->SetColorAndOpacity(Color);
	return Image;
}

USpacer* UOnyxScreen::MakeSpacer(const FVector2D& Size)
{
	USpacer* Spacer = Make<USpacer>();
	Spacer->SetSize(Size);
	return Spacer;
}

UOnyxButton* UOnyxScreen::MakeButton(bool bFocusable)
{
	UOnyxButton* Button = Make<UOnyxButton>();
	Button->SetStyle(TransparentButtonStyle());
	Button->InitFocusable(bFocusable);
	Button->BindNative();
	return Button;
}

UWidget* UOnyxScreen::MakeKeyHint(const FText& Key, const FText& Action)
{
	const FOnyxUITheme& Style = Theme();
	UHorizontalBox* Box = Make<UHorizontalBox>();

	UBorder* Chip = Make<UBorder>();
	Chip->SetBrush(OutlineBrush(Black(0.35f), WithAlpha(Style.Accent, 0.45f), 1.f, 3.f));
	Chip->SetPadding(FMargin(8.f, 3.f));
	Chip->SetContent(MakeText(Key, Font(EFont::Bold, 11, 120), Style.Accent));
	Cast<UHorizontalBoxSlot>(Box->AddChild(Chip))->SetVerticalAlignment(VAlign_Center);
	Box->AddChild(MakeSpacer(FVector2D(10.f, 1.f)));
	Cast<UHorizontalBoxSlot>(Box->AddChild(MakeText(Action, Font(EFont::Body, 13, 60), Style.TextDim)))->SetVerticalAlignment(VAlign_Center);
	return Box;
}

FOnyxFramedButton UOnyxScreen::MakeFramedButton(const FText& Text, bool bDanger, bool bFocusable, TFunction<void()> OnClicked)
{
	const FOnyxUITheme& Style = Theme();
	FOnyxFramedButton Result;
	Result.bDanger = bDanger;
	Result.Button = MakeButton(bFocusable);

	const FLinearColor Tone = bDanger ? Style.Danger : Style.Accent;
	Result.Frame = Make<UBorder>();
	Result.Frame->SetBrush(OutlineBrush(WithAlpha(Tone, 0.f), WithAlpha(Tone, 0.55f), 1.f, 3.f));
	Result.Frame->SetPadding(FMargin(26.f, 10.f));
	Result.Label = MakeText(Text, Font(EFont::Bold, 13, 220), Style.TextDim);
	Result.Frame->SetContent(Result.Label);
	Result.Button->SetContent(Result.Frame);

	Result.Button->OnClickedFn = [this, OnClicked = MoveTemp(OnClicked)]()
	{
		if (!bInputLocked)
		{
			PlayUISound(true);
			if (OnClicked)
			{
				OnClicked();
			}
		}
	};
	return Result;
}

void UOnyxScreen::TickFramedButton(FOnyxFramedButton& Button, float DeltaTime)
{
	if (!Button.Button)
	{
		return;
	}
	const FOnyxUITheme& Style = Theme();
	const bool bActive = Button.Button->HasKeyboardFocus() || Button.Button->IsHovered();
	Button.Focus = FMath::FInterpTo(Button.Focus, bActive ? 1.f : 0.f, DeltaTime, 14.f);
	Button.Frame->SetBrushColor(WithAlpha(Button.bDanger ? Style.Danger : Style.Accent, 0.2f * Button.Focus));
	Button.Label->SetColorAndOpacity(FSlateColor(FMath::Lerp(Style.TextDim, Style.TextBright, Button.Focus)));
}

// ---------------------------------------------------------------------------
// Menu entries
// ---------------------------------------------------------------------------

int32 UOnyxScreen::AddMenuEntry(UVerticalBox* Box, const FText& Label, TFunction<void()> Action, bool bDanger)
{
	const FOnyxUITheme& Style = Theme();
	const FLinearColor Tone = bDanger ? Style.Danger : Style.Accent;

	FOnyxMenuEntry Entry;
	Entry.bDanger = bDanger;
	Entry.Button = MakeButton(true);

	UHorizontalBox* Row = Make<UHorizontalBox>();

	USizeBox* MarkerBox = Make<USizeBox>();
	MarkerBox->SetWidthOverride(22.f);
	MarkerBox->SetHeightOverride(22.f);
	Entry.Marker = MakeImage(Tone, FVector2D(9.f, 9.f));
	Entry.Marker->SetRenderTransformAngle(45.f);
	if (USizeBoxSlot* MarkerSlot = Cast<USizeBoxSlot>(MarkerBox->AddChild(Entry.Marker)))
	{
		MarkerSlot->SetHorizontalAlignment(HAlign_Center);
		MarkerSlot->SetVerticalAlignment(VAlign_Center);
	}
	Cast<UHorizontalBoxSlot>(Row->AddChild(MarkerBox))->SetVerticalAlignment(VAlign_Center);
	Row->AddChild(MakeSpacer(FVector2D(14.f, 1.f)));

	Entry.Label = MakeText(Label, Font(EFont::Display, 34, 60), Style.TextDim);
	Cast<UHorizontalBoxSlot>(Row->AddChild(Entry.Label))->SetVerticalAlignment(VAlign_Center);
	Row->AddChild(MakeSpacer(FVector2D(18.f, 1.f)));

	Entry.Trail = MakeTexImage(GetFadeTexture(), WithAlpha(Tone, 0.5f), FVector2D(64.f, 2.f));
	Entry.Trail->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
	Cast<UHorizontalBoxSlot>(Row->AddChild(Entry.Trail))->SetVerticalAlignment(VAlign_Center);

	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Entry.Button->AddChild(Row)))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Left);
		ContentSlot->SetPadding(FMargin(0.f, 2.f));
	}
	if (UVerticalBoxSlot* EntrySlot = Cast<UVerticalBoxSlot>(Box->AddChild(Entry.Button)))
	{
		EntrySlot->SetHorizontalAlignment(HAlign_Left);
		EntrySlot->SetPadding(FMargin(0.f, 3.f));
	}

	Entry.Button->OnClickedFn = [this, Action = MoveTemp(Action)]()
	{
		if (!bInputLocked && !bDialogOpen)
		{
			PlayUISound(true);
			if (Action)
			{
				Action();
			}
		}
	};
	Entry.Button->SetRenderOpacity(0.f);
	return Entries.Add(Entry);
}

void UOnyxScreen::FocusEntry(int32 Index)
{
	if (Entries.IsValidIndex(Index) && Entries[Index].Button)
	{
		LastFocusedEntry = Index;
		Entries[Index].Button->SetKeyboardFocus();
	}
}

void UOnyxScreen::TickEntries(float DeltaTime)
{
	const FOnyxUITheme& Style = Theme();
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		FOnyxMenuEntry& Entry = Entries[Index];
		if (!Entry.Button)
		{
			continue;
		}
		const bool bFocused = Entry.Button->HasKeyboardFocus() && !bDialogOpen;
		Entry.Focus = FMath::FInterpTo(Entry.Focus, bFocused ? 1.f : 0.f, DeltaTime, 13.f);

		const float Appear = SmoothStep01((Age - EntryIntroDelay - Index * EntryStagger) / 0.45f);
		Entry.Button->SetRenderOpacity(Appear);
		Entry.Button->SetRenderTranslation(FVector2D(FMath::Lerp(-36.f, 0.f, Appear) + 10.f * Entry.Focus, 0.f));

		Entry.Label->SetColorAndOpacity(FSlateColor(FMath::Lerp(Style.TextDim, Entry.bDanger ? Style.Danger : Style.TextBright, Entry.Focus)));
		Entry.Marker->SetRenderOpacity(Entry.Focus);
		Entry.Marker->SetRenderScale(FVector2D(0.3f + 0.7f * Entry.Focus));
		Entry.Trail->SetRenderOpacity(Entry.Focus);
		Entry.Trail->SetRenderScale(FVector2D(FMath::Max(Entry.Focus, 0.001f), 1.f));
	}
}

// ---------------------------------------------------------------------------
// Confirm dialog
// ---------------------------------------------------------------------------

UWidget* UOnyxScreen::BuildDialogLayer()
{
	const FOnyxUITheme& Style = Theme();
	UOverlay* Layer = Make<UOverlay>();

	if (UOverlaySlot* DimSlot = Layer->AddChildToOverlay(MakeImage(Black(0.72f), FVector2D(32.f, 32.f))))
	{
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);
	}

	USizeBox* Size = Make<USizeBox>();
	Size->SetWidthOverride(640.f);
	DialogPanel = Size;

	UBorder* Panel = Make<UBorder>();
	Panel->SetBrush(OutlineBrush(Style.Panel, WithAlpha(Style.Accent, 0.3f), 1.f, 4.f));
	Panel->SetPadding(FMargin(48.f, 40.f, 48.f, 36.f));
	Size->AddChild(Panel);

	UVerticalBox* Column = Make<UVerticalBox>();
	Panel->SetContent(Column);

	DialogTitle = MakeText(FText::GetEmpty(), Font(EFont::Display, 40, 60), Style.TextBright);
	Column->AddChild(DialogTitle);
	Column->AddChild(MakeSpacer(FVector2D(1.f, 12.f)));
	Column->AddChild(MakeTexImage(GetFadeTexture(), WithAlpha(Style.Accent, 0.6f), FVector2D(160.f, 2.f)));
	Column->AddChild(MakeSpacer(FVector2D(1.f, 20.f)));

	DialogMessage = MakeText(FText::GetEmpty(), Font(EFont::Body, 17), Style.Text);
	DialogMessage->SetAutoWrapText(true);
	Column->AddChild(DialogMessage);
	Column->AddChild(MakeSpacer(FVector2D(1.f, 34.f)));

	UHorizontalBox* Buttons = Make<UHorizontalBox>();
	Cast<UVerticalBoxSlot>(Column->AddChild(Buttons))->SetHorizontalAlignment(HAlign_Right);

	DialogCancel = MakeFramedButton(INVTEXT("CANCEL"), false, true, [this]() { CloseConfirm(); });
	DialogConfirm = MakeFramedButton(INVTEXT("CONFIRM"), true, true, [this]()
	{
		TFunction<void()> Callback = MoveTemp(PendingConfirm);
		CloseConfirm();
		if (Callback)
		{
			Callback();
		}
	});
	Buttons->AddChild(DialogCancel.Button);
	Buttons->AddChild(MakeSpacer(FVector2D(14.f, 1.f)));
	Buttons->AddChild(DialogConfirm.Button);

	if (UOverlaySlot* PanelSlot = Layer->AddChildToOverlay(Size))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
	}

	Layer->SetVisibility(ESlateVisibility::Collapsed);
	Layer->SetRenderOpacity(0.f);
	DialogLayer = Layer;
	return Layer;
}

void UOnyxScreen::ShowConfirm(const FText& Title, const FText& Message, const FText& ConfirmText, TFunction<void()> OnConfirm, bool bDanger)
{
	if (!DialogLayer)
	{
		if (OnConfirm)
		{
			OnConfirm();
		}
		return;
	}

	const FOnyxUITheme& Style = Theme();
	DialogTitle->SetText(Title);
	DialogMessage->SetText(Message);
	DialogConfirm.Label->SetText(ConfirmText);
	DialogConfirm.bDanger = bDanger;
	const FLinearColor Tone = bDanger ? Style.Danger : Style.Accent;
	DialogConfirm.Frame->SetBrush(OutlineBrush(WithAlpha(Tone, 0.f), WithAlpha(Tone, 0.55f), 1.f, 3.f));

	PendingConfirm = MoveTemp(OnConfirm);
	bDialogOpen = true;
	DialogLayer->SetVisibility(ESlateVisibility::Visible);
	if (MenuContent)
	{
		MenuContent->SetIsEnabled(false);
	}
	DialogCancel.Button->SetKeyboardFocus();
}

void UOnyxScreen::CloseConfirm()
{
	if (!bDialogOpen)
	{
		return;
	}
	bDialogOpen = false;
	PendingConfirm = nullptr;
	if (MenuContent)
	{
		MenuContent->SetIsEnabled(true);
	}
	FocusDefault();
}

// ---------------------------------------------------------------------------
// Settings overlay
// ---------------------------------------------------------------------------

void UOnyxScreen::OpenSettings()
{
	if (ActiveSettings)
	{
		return;
	}
	UOnyxSettingsMenu* Settings = GetOwningPlayer()
		? CreateWidget<UOnyxSettingsMenu>(GetOwningPlayer(), UOnyxSettingsMenu::StaticClass())
		: CreateWidget<UOnyxSettingsMenu>(GetWorld(), UOnyxSettingsMenu::StaticClass());
	if (!Settings)
	{
		return;
	}

	TWeakObjectPtr<UOnyxScreen> WeakThis(this);
	Settings->OnClosed = [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnSettingsClosed();
		}
	};
	ActiveSettings = Settings;
	Settings->AddToViewport(100);
	if (MenuContent)
	{
		MenuContent->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UOnyxScreen::OnSettingsClosed()
{
	ActiveSettings = nullptr;
	if (MenuContent)
	{
		MenuContent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	FocusDefault();
}

// ---------------------------------------------------------------------------
// Misc
// ---------------------------------------------------------------------------

bool UOnyxScreen::CanReclaimFocus() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}
	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (!Focused.IsValid() || HasFocusedDescendants() || Focused == GetCachedWidget())
	{
		return true;
	}
	if (const UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			const TSharedPtr<SViewport> ViewportWidget = Viewport->GetGameViewportWidget();
			if (ViewportWidget.IsValid() && Focused.Get() == ViewportWidget.Get())
			{
				return true;
			}
		}
	}
	return false;
}

void UOnyxScreen::PlayUISound(bool bConfirm) const
{
	const FOnyxUITheme& Style = Theme();
	USoundBase* Sound = (bConfirm ? Style.ConfirmSound : Style.NavigateSound).LoadSynchronous();
	if (Sound && GetWorld())
	{
		UGameplayStatics::PlaySound2D(this, Sound, Style.SoundVolume * UOnyxAudioSubsystem::GetCategoryVolume(EOnyxSoundCategory::UI), 1.f, 0.f, nullptr, nullptr, true);
	}
}

UTexture2D* UOnyxScreen::GetFadeTexture()
{
	if (!FadeTexture)
	{
		FadeTexture = CreateAlphaTexture(256, 4, [](float U, float) { return FMath::Pow(1.f - U, 1.35f); });
	}
	return FadeTexture;
}

UTexture2D* UOnyxScreen::GetVignetteTexture()
{
	if (!VignetteTexture)
	{
		VignetteTexture = CreateAlphaTexture(128, 128, [](float U, float V)
		{
			const float Dist = FVector2D(U - 0.5f, V - 0.5f).Size() / 0.7071f;
			return SmoothStep01((Dist - 0.35f) / 0.65f);
		});
	}
	return VignetteTexture;
}
