// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AOnyxParallaxBackground;
class IDetailsView;

/** Builds and edits the level's parallax background (layers of tiling textures). */
class SOnyxParallaxTool : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxParallaxTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SOnyxParallaxTool() override;

private:
	TSharedRef<SWidget> MakeToolbar();
	void SetTarget(AOnyxParallaxBackground* Background);
	AOnyxParallaxBackground* FindInLevel() const;
	void HandleSelectionChanged(UObject* Selection);

	void AddBackground();
	void AddLayersFromSelection();
	void FocusBackground();
	FText GetStatus() const;

	TWeakObjectPtr<AOnyxParallaxBackground> Target;
	TSharedPtr<IDetailsView> Details;
	FDelegateHandle SelectionHandle;
};
