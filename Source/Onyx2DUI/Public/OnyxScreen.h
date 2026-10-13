// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "OnyxScreen.generated.h"

class UBorder;
class UImage;
class UOnyxButton;
class UOnyxSettingsMenu;
class USoundBase;
class USpacer;
class UTextBlock;
class UTexture2D;
class UVerticalBox;

/** One entry of a screen's menu, as built by the screen (its button and label). */
USTRUCT()
struct FOnyxMenuEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UOnyxButton> Button = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> Label = nullptr;

	UPROPERTY()
	TObjectPtr<UImage> Marker = nullptr;

	UPROPERTY()
	TObjectPtr<UImage> Trail = nullptr;

	float Focus = 0.f;
	bool bDanger = false;
};

/** Outlined text button used by dialogs and footers. */
USTRUCT()
struct FOnyxFramedButton
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UOnyxButton> Button = nullptr;

	UPROPERTY()
	TObjectPtr<UBorder> Frame = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> Label = nullptr;

	float Focus = 0.f;
	bool bDanger = false;
};

/**
 * Base for Onyx UI screens. The widget tree is built in code (BuildUI) so screens work with no
 * assets; subclass in C++ or Blueprint to change content. Provides animated menu entries,
 * keyboard/gamepad navigation, focus recovery, a confirm dialog and the settings overlay.
 *
 * A Widget Blueprint child with a layout of its own (the Designer) skips BuildUI: it has no built-in menu entries and no
 * confirm box, so its buttons call the screen's functions (Open Settings, Start Game, Resume...) and actions that would
 * ask "Are you sure?" happen straight away. Name a panel MenuContent to have it hidden while settings are open.
 */
UCLASS(Abstract)
class ONYX2DUI_API UOnyxScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	UOnyxScreen(const FObjectInitializer& ObjectInitializer);

	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	virtual void BuildUI() {}
	virtual void OnBackPressed() {}
	virtual void FocusDefault();

	// --- factory helpers ---
	template <class T>
	T* Make()
	{
		return WidgetTree->ConstructWidget<T>(T::StaticClass());
	}

	UTextBlock* MakeText(const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color);
	UImage* MakeImage(const FLinearColor& Color, const FVector2D& Size);
	UImage* MakeTexImage(UTexture2D* Texture, const FLinearColor& Color, const FVector2D& Size = FVector2D(32.f, 32.f));
	USpacer* MakeSpacer(const FVector2D& Size);
	UOnyxButton* MakeButton(bool bFocusable);
	UWidget* MakeKeyHint(const FText& Key, const FText& Action);
	FOnyxFramedButton MakeFramedButton(const FText& Text, bool bDanger, bool bFocusable, TFunction<void()> OnClicked);
	void TickFramedButton(FOnyxFramedButton& Button, float DeltaTime);

	// --- menu entries ---
	int32 AddMenuEntry(UVerticalBox* Box, const FText& Label, TFunction<void()> Action, bool bDanger = false);
	void FocusEntry(int32 Index);
	void TickEntries(float DeltaTime);

	// --- confirm dialog ---
	UWidget* BuildDialogLayer();
	void ShowConfirm(const FText& Title, const FText& Message, const FText& ConfirmText, TFunction<void()> OnConfirm, bool bDanger = true);
	void CloseConfirm();
	bool IsDialogOpen() const { return bDialogOpen; }

	// --- settings overlay ---
	/** Opens the settings screen over this one (for a Settings button of a layout of your own). */
	UFUNCTION(BlueprintCallable, Category = "Onyx2D|UI")
	void OpenSettings();
	bool IsSettingsOpen() const { return ActiveSettings != nullptr; }
	virtual void OnSettingsClosed();

	bool CanReclaimFocus() const;
	void PlayUISound(bool bConfirm) const;

	UTexture2D* GetFadeTexture();
	UTexture2D* GetVignetteTexture();

	UPROPERTY(Transient)
	TArray<FOnyxMenuEntry> Entries;

	/** Hidden while settings are open, disabled while a dialog is open. */
	UPROPERTY(BlueprintReadOnly, Category = "Screen", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MenuContent;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> DialogLayer;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> DialogPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DialogTitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DialogMessage;

	UPROPERTY(Transient)
	FOnyxFramedButton DialogConfirm;

	UPROPERTY(Transient)
	FOnyxFramedButton DialogCancel;

	UPROPERTY(Transient)
	TObjectPtr<UOnyxSettingsMenu> ActiveSettings;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FadeTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> VignetteTexture;

	TFunction<void()> PendingConfirm;
	int32 LastFocusedEntry = 0;
	float Age = 0.f;
	float DialogAlpha = 0.f;
	bool bDialogOpen = false;
	bool bInputLocked = false;
	float EntryIntroDelay = 0.25f;
	float EntryStagger = 0.07f;
};
