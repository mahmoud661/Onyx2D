// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"

class SOnyxSlicerCanvas;
class UOnyxClip;
class UTexture2D;

DECLARE_DELEGATE_TwoParams(FOnOnyxClipSliced, UOnyxClip* /*Clip*/, FName /*Role*/);

/**
 * Cuts animations out of a sprite sheet (like Unity's Sprite Editor): pick a texture or import a PNG, the frame grid is
 * detected, click cells (in play order) or drag across a row to select frames, name the animation and create it. Makes
 * the sprites (with normal maps for 2D lighting), the flipbook and the Clip.
 */
class SOnyxSpriteSlicer : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SOnyxSpriteSlicer) {}
		SLATE_ARGUMENT(UTexture2D*, Texture)
		/** Content folder for the new assets. */
		SLATE_ARGUMENT(FString, Folder)
		SLATE_ARGUMENT(FString, NamePrefix)
		SLATE_ARGUMENT(FName, Role)
		SLATE_EVENT(FOnOnyxClipSliced, OnCreated)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Opens the slicer in its own window. */
	static void Open(UTexture2D* Texture, const FString& Folder, const FString& NamePrefix, FName Role, FOnOnyxClipSliced OnCreated);

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SOnyxSpriteSlicer"); }

private:
	void SetTexture(UTexture2D* NewTexture);
	void DetectGrid();
	void ImportPng();
	FReply Create();
	FText GetStatus() const;

	TObjectPtr<UTexture2D> Texture = nullptr;
	FIntPoint CellSize = FIntPoint(32, 32);
	int32 Margin = 0;
	int32 Spacing = 0;
	FString Role = TEXT("Idle");
	float FramesPerSecond = 10.f;
	bool bLoop = true;
	float CentimetersPerPixel = 4.f;
	FString Folder;
	FString NamePrefix;
	FOnOnyxClipSliced OnCreated;
	TSharedPtr<SOnyxSlicerCanvas> Canvas;
};
