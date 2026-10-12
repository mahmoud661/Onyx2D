// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsWindow/OnyxTileOptions.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class UPaperTileMap;
class UTexture2D;

DECLARE_DELEGATE_OneParam(FOnOnyxTileMapCreated, UPaperTileMap*);

/** Tile sheet -> Paper2D tile set (with collision) -> tile map placed in the level. */
class SOnyxTileTool : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxTileTool) {}
		/** Fired after Create Tile Map (the toolkit opens the Level Painter on it). */
		SLATE_EVENT(FOnOnyxTileMapCreated, OnTileMapCreated)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetTileSheet(UTexture2D* Texture);

private:
	TSharedRef<SWidget> MakeToolbar();
	bool CreateTileSet();
	bool CreateTileMap();
	void UseContentBrowserSelection();
	void OpenAsset(UObject* Asset) const;

	FText GetSheetInfo() const;
	FText GetStatus() const;
	TArray<FIntRect> GetGroundTileRect() const;

	TStrongObjectPtr<UOnyxTileOptions> Options;
	TSharedPtr<IDetailsView> Details;
	FText LastResult;
	FOnOnyxTileMapCreated OnTileMapCreated;
};
