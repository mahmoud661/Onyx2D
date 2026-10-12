// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STileView.h"

class FAssetThumbnail;
class FAssetThumbnailPool;
class ITableRow;
class SBox;
class SOnyxFlipbookPreview;
class STableViewBase;
class SWrapBox;
class UBlueprint;
class UPaperTileMap;
class UPaperTileSet;

DECLARE_DELEGATE_OneParam(FOnOnyxEditAnimations, UObject* /*Moveset, Clip or character Blueprint*/);
DECLARE_DELEGATE_OneParam(FOnOnyxPaintTileMap, UPaperTileMap*);
DECLARE_DELEGATE_OneParam(FOnOnyxPaintTileSet, UPaperTileSet*);

/** What a Library entry is. */
enum class EOnyxLibraryCategory : uint8
{
	All,
	Character,
	Object,
	TileSet,
	TileMap,
	Animation
};

/** One previewable thing: an asset, or a C++ class from the game module deriving from an Onyx class. */
struct FOnyxLibraryEntry
{
	FAssetData Asset;
	TWeakObjectPtr<UClass> NativeClass;
	EOnyxLibraryCategory Category = EOnyxLibraryCategory::Object;
	FString Name;
	TSharedPtr<FAssetThumbnail> Thumbnail;
};

/**
 * Everything Onyx-ready in the project in one place: characters, other Onyx Blueprints and C++ classes, tile sets,
 * tile maps and animations (Anim Sequences and flipbooks). Pick one to see it play (characters: every animation), then place it in the level,
 * open it, edit its animations or paint with it. Updates by itself when assets are added or removed.
 */
class SOnyxLibraryTool : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxLibraryTool) {}
		SLATE_EVENT(FOnOnyxEditAnimations, OnEditAnimations)
		SLATE_EVENT(FOnOnyxPaintTileMap, OnPaintTileMap)
		SLATE_EVENT(FOnOnyxPaintTileSet, OnPaintTileSet)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SOnyxLibraryTool() override;

private:
	using FEntryPtr = TSharedPtr<FOnyxLibraryEntry>;

	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeCategoryBar();
	TSharedRef<SWidget> MakePreviewPanel();
	TSharedRef<ITableRow> MakeTile(FEntryPtr Entry, const TSharedRef<STableViewBase>& Owner);

	void Refresh();
	void ApplyFilter();
	void Select(FEntryPtr Entry);
	void ShowAnimation(FName Animation);
	/** Adds a moveset's clips (by role) to the preview's animations. */
	void AddMovesetClips(const class UOnyxMoveset* Moveset);
	/** One button per animation in the preview; shows Idle first. */
	void ShowRoleButtons();
	void MarkDirty() { bDirty = true; }
	EActiveTimerReturnType CheckDirty(double CurrentTime, float DeltaTime);

	UClass* GetEntryClass(const FOnyxLibraryEntry& Entry) const;
	UObject* GetEntryObject(const FOnyxLibraryEntry& Entry) const;
	void PlaceSelected();
	void OpenSelected();

	static FText CategoryName(EOnyxLibraryCategory Category);

	TArray<FEntryPtr> Entries;
	TArray<FEntryPtr> Filtered;
	FEntryPtr Selected;
	EOnyxLibraryCategory Category = EOnyxLibraryCategory::All;
	FString Search;
	bool bDirty = false;

	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TSharedPtr<STileView<FEntryPtr>> TileView;
	TSharedPtr<SOnyxFlipbookPreview> Preview;
	TSharedPtr<SBox> StillPreview;
	TSharedPtr<SWrapBox> AnimationButtons;
	TMap<FName, TPair<TWeakObjectPtr<class UPaperFlipbook>, float>> Animations;
	FName CurrentAnimation;

	FOnOnyxEditAnimations OnEditAnimations;
	FOnOnyxPaintTileMap OnPaintTileMap;
	FOnOnyxPaintTileSet OnPaintTileSet;
	FDelegateHandle AddedHandle;
	FDelegateHandle RemovedHandle;
	FDelegateHandle RenamedHandle;
};
