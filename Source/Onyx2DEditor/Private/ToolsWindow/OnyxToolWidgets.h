// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

class IDetailsView;

/** Small building blocks that give every Onyx2D tool the standard Unreal editor look. */
namespace OnyxTools
{
	FSlateFontInfo Font(int32 Size, bool bBold = false);
	FLinearColor Accent();
	FLinearColor Muted();

	/** Standard Details panel for a UObject, without the name area. */
	TSharedRef<IDetailsView> MakeDetailsView(UObject* Object, bool bAllowSearch = true);

	/** Limits a details view to categories containing one of the given names, for objects of Class (hides Rendering, Physics...). */
	void ShowOnlyCategories(const TSharedRef<IDetailsView>& View, UClass* Class, const TArray<FName>& Categories);

	/** Toolbar strip under the tab well: toolbar on the left, optional extra content on the right. */
	TSharedRef<SWidget> ToolbarStrip(const TSharedRef<SWidget>& Toolbar, const TSharedPtr<SWidget>& Trailing = nullptr);

	/** Panel with a category-style header, as used by Details / Outliner panels. */
	TSharedRef<SWidget> Section(const TAttribute<FText>& Title, const TSharedRef<SWidget>& Content, const TSharedPtr<SWidget>& HeaderExtra = nullptr);

	/** Status bar at the bottom of a tool. */
	TSharedRef<SWidget> StatusBar(const TAttribute<FText>& Text);

	/** Muted, wrapping helper text. */
	TSharedRef<SWidget> Hint(const FText& Text);

	/** Toast in the bottom-right corner of the editor. */
	void Notify(const FText& Message, bool bSuccess);

	/** Opens a documentation page in the browser ("levels/level-painter", optionally with "#section"). */
	void OpenDocs(const FString& Page);

	/** A small "?" button that opens a documentation page. */
	TSharedRef<SWidget> HelpButton(const FString& Page, const FText& Tooltip = FText::GetEmpty());
}
