// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/OnyxToolOptions.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class SOnyxFlipbookPreview;
class UTexture2D;
struct FPropertyChangedEvent;

/** Character Wizard: sprite sheet -> sprites, flipbooks and a playable character Blueprint. */
class SOnyxCharacterTool : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxCharacterTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Loads a texture as the sprite sheet, names the character after it and guesses the frame size. */
	void SetSpriteSheet(UTexture2D* Texture);

private:
	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakePreviewPanel();
	TSharedRef<SWidget> MakeTransportButton(FName Icon, const FText& Tooltip, TFunction<void()> OnClicked, TFunction<FName()> DynamicIcon = nullptr);

	void HandlePropertyChanged(const FPropertyChangedEvent& Event);
	void RefreshPreview();
	void RebuildAnimationChoices();
	void SelectAnimation(TSharedPtr<FName> Name);
	int32 FindSelectedIndex() const;

	void CreateCharacter();
	void DetectFrameSize();
	void UseContentBrowserSelection();
	void ResetAnimations();

	FText GetSheetInfo() const;
	FText GetStatus() const;
	TArray<FIntRect> GetSelectedFrameRects() const;

	TStrongObjectPtr<UOnyxCharacterWizardOptions> Options;
	TSharedPtr<IDetailsView> Details;
	TSharedPtr<SOnyxFlipbookPreview> Preview;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> AnimationCombo;
	TArray<TSharedPtr<FName>> AnimationChoices;
	FName SelectedAnimation = TEXT("Idle");
	FText LastResult;
};
