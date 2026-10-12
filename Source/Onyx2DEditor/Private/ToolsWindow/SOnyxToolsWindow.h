// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/SCompoundWidget.h"

class FTabManager;
class SBox;
class SDockTab;
class SOnyxAnimationEditor;
class SOnyxPainterTool;
class SOnyxTileTool;
class UBlueprint;
class UPaperTileMap;
class UPaperTileSet;
class UTexture2D;

/**
 * The Onyx2D tools window: a sidebar that is always there (every tool, grouped; open / focus one, reset the layout) next to
 * its own tab manager hosting the tools you opened: Home, Import, Library, Animations, Tiles, Level Painter, Parallax and Level tabs,
 * which can be rearranged or torn off like any editor tab.
 */
class SOnyxToolsWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxToolsWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& OwnerTab);
	virtual ~SOnyxToolsWindow() override;

	/** Animation Editor: a new animation cut from this sprite sheet. */
	void ShowNewAnimation(UTexture2D* SpriteSheet);
	void ShowTiles(UTexture2D* TileSheet = nullptr);
	/** Animation Editor on an Animation Set, Anim Sequence or character Blueprint. */
	void ShowAnimations(UObject* Asset);
	void ShowPainter(UPaperTileMap* TileMap = nullptr);
	void ShowPainterWithTileSet(UPaperTileSet* TileSet);

	/** Opens (or brings to front) a tool tab, even after every tab was closed. */
	void FocusTab(FName TabId);

	/** Closes every tool tab and restores the default layout. */
	void ResetLayout();

	static const FName HomeTabId;
	static const FName ImportTabId;
	static const FName LibraryTabId;
	static const FName AnimationsTabId;
	static const FName TilesTabId;
	static const FName PainterTabId;
	static const FName ParallaxTabId;
	static const FName LevelTabId;

private:
	void RegisterTabs();
	void SaveLayout();
	TSharedRef<SWidget> MakeSidebar();
	void RestoreLayout(const TSharedRef<FTabManager::FLayout>& Layout);
	bool IsTabOpen(FName TabId) const;
	bool IsTabInFront(FName TabId) const;

	TSharedPtr<FTabManager> TabManager;
	TWeakPtr<SDockTab> OwnerTabWeak;
	TSharedPtr<SBox> TabArea;
	TSharedPtr<SOnyxAnimationEditor> AnimationEditor;
	TSharedPtr<SOnyxTileTool> TileTool;
	TSharedPtr<SOnyxPainterTool> PainterTool;
	bool bSidebarCollapsed = false;
};
