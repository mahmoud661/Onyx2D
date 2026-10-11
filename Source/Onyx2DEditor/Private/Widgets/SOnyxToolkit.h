// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FTabManager;
class SDockTab;
class SOnyxCharacterTool;
class SOnyxPainterTool;
class UPaperTileMap;
class SOnyxTileTool;
class UTexture2D;

/**
 * The Onyx2D tools window: its own tab manager hosting Import, Character Wizard, Animations,
 * Tiles, Level Painter, Parallax and Level tabs, which can be rearranged or torn off like any editor tab.
 */
class SOnyxToolkit : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxToolkit) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& OwnerTab);
	virtual ~SOnyxToolkit() override;

	void ShowCharacterWizard(UTexture2D* SpriteSheet = nullptr);
	void ShowTiles(UTexture2D* TileSheet = nullptr);
	void FocusTab(FName TabId);
	void ShowPainter(UPaperTileMap* TileMap = nullptr);

	static const FName ImportTabId;
	static const FName CharacterTabId;
	static const FName AnimationsTabId;
	static const FName TilesTabId;
	static const FName PainterTabId;
	static const FName ParallaxTabId;
	static const FName LevelTabId;

private:
	void RegisterTabs();
	void SaveLayout();

	TSharedPtr<FTabManager> TabManager;
	TSharedPtr<SOnyxCharacterTool> CharacterTool;
	TSharedPtr<SOnyxTileTool> TileTool;
	TSharedPtr<SOnyxPainterTool> PainterTool;
};
