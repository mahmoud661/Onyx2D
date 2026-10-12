// Copyright Onyx2D. All Rights Reserved.

#include "MovesetEditor/Timeline/OnyxClipTimelineStyle.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxClipEvent.h"
#include "Animation/OnyxClipWindow.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Style/OnyxEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SWindow.h"

namespace OnyxTimeline
{
	namespace
	{
		FLinearColor Hex(uint8 R, uint8 G, uint8 B, float Alpha = 1.f)
		{
			return FLinearColor::FromSRGBColor(FColor(R, G, B)).CopyWithNewOpacity(Alpha);
		}
	}

	FLinearColor Panel() { return Hex(0x1C, 0x1C, 0x1E); }
	FLinearColor Bar() { return Hex(0x24, 0x24, 0x26); }
	FLinearColor Header() { return Hex(0x23, 0x23, 0x25); }
	FLinearColor RulerColor() { return Hex(0x20, 0x20, 0x22); }
	FLinearColor FramesRow() { return Hex(0x1F, 0x1F, 0x21); }
	FLinearColor FrameCell() { return Hex(0x2B, 0x2B, 0x2E); }
	FLinearColor FrameCellLine() { return Hex(0x3A, 0x3A, 0x3E); }
	FLinearColor Divider() { return Hex(0x12, 0x12, 0x13); }
	FLinearColor Playhead() { return Hex(0xFF, 0x8A, 0x3D); }
	FLinearColor Accent() { return Hex(0x2D, 0x6C, 0xDF); }
	FLinearColor TextColor() { return Hex(0xD6, 0xD6, 0xD6); }
	FLinearColor MutedText() { return Hex(0x8C, 0x8C, 0x8C); }

	FLinearColor LaneRow(const FLinearColor& LaneColor, int32 Index)
	{
		const FLinearColor Base = Index % 2 ? Hex(0x1E, 0x1E, 0x20) : Panel();
		return FMath::Lerp(Base, LaneColor.CopyWithNewOpacity(1.f), 0.05f);
	}

	FLinearColor EventFill(const FLinearColor& Color)
	{
		return FMath::Lerp(Color, FLinearColor::White, 0.3f).CopyWithNewOpacity(1.f);
	}

	FLinearColor EventInk(const FLinearColor& Color)
	{
		return FMath::Lerp(Color, FLinearColor::Black, 0.86f).CopyWithNewOpacity(1.f);
	}

	const FSlateBrush* Rounded(float Radius)
	{
		static TMap<int32, TUniquePtr<FSlateRoundedBoxBrush>> Brushes;
		TUniquePtr<FSlateRoundedBoxBrush>& Brush = Brushes.FindOrAdd(FMath::RoundToInt(Radius * 2.f));
		if (!Brush)
		{
			Brush = MakeUnique<FSlateRoundedBoxBrush>(FLinearColor::White, Radius);
		}
		return Brush.Get();
	}

	const FSlateBrush* FlagTag()
	{
		static FSlateRoundedBoxBrush Brush(FLinearColor::White, FVector4(9.f, 9.f, 9.f, 2.f));
		return &Brush;
	}

	FName EventIconName(const UClass* Class)
	{
		if (!Class)
		{
			return TEXT("Onyx2D.Event.Custom.Small");
		}
		struct FKind { const UClass* Class; const TCHAR* Icon; };
		const FKind Kinds[] = {
			{ UOnyxClipEvent_Message::StaticClass(), TEXT("Onyx2D.Event.Message.Small") },
			{ UOnyxClipEvent_Sound::StaticClass(), TEXT("Onyx2D.Event.Sound.Small") },
			{ UOnyxClipEvent_Effect::StaticClass(), TEXT("Onyx2D.Event.Effect.Small") },
			{ UOnyxClipEvent_CameraShake::StaticClass(), TEXT("Onyx2D.Event.Shake.Small") },
			{ UOnyxClipEvent_LightFlash::StaticClass(), TEXT("Onyx2D.Event.Light.Small") },
			{ UOnyxClipEvent_SceneFlash::StaticClass(), TEXT("Onyx2D.Event.Flash.Small") },
			{ UOnyxClipWindow_Hitbox::StaticClass(), TEXT("Onyx2D.Window.Hitbox.Small") },
			{ UOnyxClipWindow_Invulnerable::StaticClass(), TEXT("Onyx2D.Window.Invulnerable.Small") },
			{ UOnyxClipWindow_LoopSound::StaticClass(), TEXT("Onyx2D.Window.LoopSound.Small") },
		};
		for (const FKind& Kind : Kinds)
		{
			if (Class->IsChildOf(Kind.Class))
			{
				return Kind.Icon;
			}
		}
		return Class->IsChildOf(UOnyxClipWindow::StaticClass()) ? TEXT("Onyx2D.Window.Custom.Small") : TEXT("Onyx2D.Event.Custom.Small");
	}

	const FSlateBrush* EventIcon(const UOnyxClipEvent* Event)
	{
		return FOnyxEditorStyle::Get().GetBrush(EventIconName(Event ? Event->GetClass() : nullptr));
	}

	const TArray<FNamedColor>& LaneColors()
	{
		static const TArray<FNamedColor> Colors = {
			{ INVTEXT("Purple"), UOnyxClip::GetLanePaletteColor(0) },
			{ INVTEXT("Amber"), UOnyxClip::GetLanePaletteColor(1) },
			{ INVTEXT("Teal"), UOnyxClip::GetLanePaletteColor(2) },
			{ INVTEXT("Coral"), UOnyxClip::GetLanePaletteColor(3) },
			{ INVTEXT("Blue"), UOnyxClip::GetLanePaletteColor(4) },
			{ INVTEXT("Green"), UOnyxClip::GetLanePaletteColor(5) },
			{ INVTEXT("Pink"), UOnyxClip::GetLanePaletteColor(6) },
			{ INVTEXT("Gray"), UOnyxClip::GetLanePaletteColor(7) },
		};
		return Colors;
	}

	void PromptForName(const TSharedRef<SWidget>& Parent, const FText& Title, const FText& Initial, TFunction<void(FName)> OnCommit)
	{
		TSharedRef<SWindow> Window = SNew(SWindow).Title(Title).ClientSize(FVector2D(320.f, 64.f)).SupportsMinimize(false).SupportsMaximize(false);
		TWeakPtr<SWindow> WeakWindow = Window;
		Window->SetContent(SNew(SBorder).BorderImage(FAppStyle::GetBrush("Brushes.Panel")).Padding(14.f)
			[
				SNew(SEditableTextBox)
				.Text(Initial)
				.SelectAllTextWhenFocused(true)
				.OnTextCommitted_Lambda([WeakWindow, OnCommit](const FText& Text, ETextCommit::Type Commit)
				{
					const FString Name = Text.ToString().TrimStartAndEnd().Replace(TEXT(" "), TEXT(""));
					if (Commit == ETextCommit::OnEnter && !Name.IsEmpty())
					{
						OnCommit(FName(*Name));
					}
					if (TSharedPtr<SWindow> Pinned = WeakWindow.Pin())
					{
						Pinned->RequestDestroyWindow();
					}
				})
			]);
		FSlateApplication::Get().AddModalWindow(Window, Parent);
	}
}
