// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OnyxBrushLibrary.generated.h"

class UTexture2D;
struct FSlateBrush;

/** Object brushes the user added to the Level Painter (saved per user, per project). */
UCLASS(Config = EditorPerProjectUserSettings)
class UOnyxPainterSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Blueprints, flipbooks, sprites or static meshes that can be painted onto the map. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> CustomBrushes;
};

/** How the map canvas draws an area object: a top-to-bottom gradient with a line along its top edge. */
struct FOnyxAreaLook
{
	FLinearColor Top = FLinearColor(0.2f, 0.55f, 0.9f, 0.5f);
	FLinearColor Bottom = FLinearColor(0.05f, 0.15f, 0.4f, 0.8f);
	FLinearColor Edge = FLinearColor::White;
	/** The top edge ripples (water). */
	bool bWavyEdge = false;
};

/** Something that can be painted onto a cell as an actor: a gameplay piece or one of your own assets. */
struct FOnyxPainterBrush
{
	/** Built-in name ("Coin") or the asset path of a custom brush. */
	FName Id;
	FText Label;
	/** Actor class spawned for this brush. */
	UClass* Class = nullptr;
	/** Custom brushes: the Blueprint / flipbook / sprite / mesh asset. */
	FSoftObjectPath Asset;
	bool bCustom = false;
	/** One of the project's characters, listed by itself (not removable from the palette). */
	bool bProject = false;
	/** Stand on the bottom of the cell (characters, player starts, your own objects) instead of sitting in its centre. */
	bool bStandOnFloor = false;
	/** Object layer it goes to by default ("Entities" or "Decor"). */
	FName DefaultLayer;
	FLinearColor Color = FLinearColor::White;
	/** Style brush used instead of the class icon (empty = class icon). */
	FName Icon;
	/** Optional setup after spawning (e.g. turn a pickup into a health pickup). */
	TFunction<void(AActor*)> Configure;
	/** Built-in brushes: recognises actors placed before brush tags existed. */
	TFunction<bool(const AActor*)> Matches;

	/**
	 * Area brushes (water): the actor fills a block of cells you drag out, and can be moved and resized on the canvas.
	 * GetArea reads the world rectangle it covers (X and Z of Min / Max); SetArea makes it cover one.
	 */
	TFunction<FBox(const AActor*)> GetArea;
	TFunction<void(AActor*, const FBox&)> SetArea;
	TFunction<FOnyxAreaLook(const AActor*)> GetAreaLook;
	/** Cells a click (rather than a drag) covers. */
	FIntPoint DefaultAreaCells = FIntPoint(8, 4);

	bool IsArea() const { return GetArea && SetArea; }
};

/**
 * All paintable objects: the built-in Onyx actors plus any asset the user drops in.
 * Knows how to spawn them and how to draw a small preview of them.
 */
class FOnyxBrushLibrary
{
public:
	static FOnyxBrushLibrary& Get();

	static const FName EntitiesLayer;
	static const FName DecorLayer;
	/** Object layer drawn behind the tile map (scenery). */
	static const FName BackgroundLayer;

	const TArray<TSharedRef<FOnyxPainterBrush>>& GetBrushes() const { return Brushes; }
	TSharedPtr<FOnyxPainterBrush> FindById(FName Id) const;
	/** Brush that made this actor (from its tag, or by matching built-in classes). */
	TSharedPtr<FOnyxPainterBrush> FindFor(const AActor* Actor) const;

	/** Can this asset become a brush? (actor Blueprint, flipbook, sprite, static mesh) */
	static bool CanBeBrush(const FAssetData& Asset);
	/** Adds assets as custom brushes; returns how many were added. */
	int32 AddAssets(const TArray<FAssetData>& Assets);
	void Remove(FName Id);

	/** Spawns the brush's actor (not yet aligned to a cell). */
	AActor* Spawn(const FOnyxPainterBrush& Brush, UWorld* World, const FVector& Location) const;

	/** Small icon for lists and canvas markers. */
	const FSlateBrush* GetIcon(const FOnyxPainterBrush& Brush) const;
	/**
	 * What the brush looks like at Time (seconds; animations loop): the texture and pixel rectangle of its sprite.
	 * Sprites, flipbooks, and actor classes that show one (characters: their Idle clip).
	 */
	bool GetPreviewSprite(const FOnyxPainterBrush& Brush, UTexture2D*& OutTexture, FIntRect& OutRect, double Time = 0.0) const;

	DECLARE_MULTICAST_DELEGATE(FOnChanged);
	FOnChanged OnChanged;

private:
	FOnyxBrushLibrary();
	~FOnyxBrushLibrary();
	void Rebuild();
	/** Brushes for the project's character Blueprints. */
	void AddProjectCharacters();
	/** A Blueprint was added, removed or renamed: rebuild on the next tick. */
	void OnAssetsChanged(const FAssetData& Asset);
	TSharedPtr<FOnyxPainterBrush> MakeCustomBrush(const FSoftObjectPath& Path) const;

	TArray<TSharedRef<FOnyxPainterBrush>> Brushes;
	bool bRebuildQueued = false;
};
