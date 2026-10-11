// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** What a dropped image (or group of images) becomes. */
enum class EOnyxArtKind : uint8
{
	TileSet,
	Character,
	Sprite,
	Background,
	Skip
};

/** Where one animation's frames come from: frame files, or cells of one strip / sheet image. */
struct FOnyxArtAnimation
{
	FName Name;
	/** Frame images in order, or a single strip / sheet image. */
	TArray<FString> Files;
	/** Cell size inside a single image; zero = each file is one frame. */
	FIntPoint CellSize = FIntPoint::ZeroValue;
	/** Sheet row to read (-1 = every row, left to right then top to bottom). */
	int32 Row = INDEX_NONE;
	int32 FrameCount = 0;
};

/** One thing the importer will build, as guessed from the dropped files. Everything can be changed before building. */
struct FOnyxArtItem
{
	EOnyxArtKind Kind = EOnyxArtKind::Skip;
	FString Name;
	/** Short "where it came from" text: file or folder name and file count. */
	FString Source;

	/** Tile set, sprite and background: the image file. */
	FString ImageFile;
	FIntPoint ImageSize = FIntPoint::ZeroValue;

	/** Tile set grid. */
	FIntPoint TileSize = FIntPoint(16, 16);
	int32 Margin = 0;
	int32 Spacing = 0;

	/** Sprite sheet of separate props: one sprite per region (empty = one sprite for the whole image). */
	TArray<FIntRect> Regions;

	/** Character. */
	TArray<FOnyxArtAnimation> Animations;
	bool bPlayer = false;

	/** Filled after building: the main asset made. */
	TWeakObjectPtr<UObject> Result;

	FText Describe() const;
};

struct FOnyxArtImportSettings
{
	/** Content folder; each item gets a sub-folder named after it. */
	FString Folder = TEXT("/Game/Art");
	/** World size of one art pixel in cm, shared by tiles, characters and sprites so pixels match. */
	float CentimetersPerPixel = 4.f;
};

/**
 * Turns art dropped from disk into ready-to-use Onyx2D assets with no manual setup:
 * - tile sheets  -> padded, lit tile set with collision (tile size, margin and spacing detected);
 * - animations   -> a playable character: frame sequences (run-1.png, run-2.png...), one strip per animation
 *                   (idle.png, run.png) or a sheet with one animation per row; names like idle / run / jump /
 *                   attack / hurt / death pick the animation, the rest of the name groups files per character;
 * - other images -> a sprite (props) or a pixel-art texture (backgrounds, for the parallax tool).
 */
class FOnyxArtImporter
{
public:
	/** Looks at files and folders (searched recursively for PNGs) and guesses what to build. */
	static TArray<TSharedPtr<FOnyxArtItem>> Analyze(const TArray<FString>& Paths);

	/** Builds one item. Returns false with OutError set on failure. */
	static bool Build(FOnyxArtItem& Item, const FOnyxArtImportSettings& Settings, FText& OutError);

	/** Detects a tile sheet's grid: tile size from the file name ("16x16"), gaps between tiles, or tile edges. */
	static void DetectTileGrid(const FString& File, const TArray<FColor>& Pixels, FIntPoint Size, FIntPoint& OutTileSize, int32& OutMargin, int32& OutSpacing);

	/** Animation an image name stands for (Idle, Run, Jump, Fall, Attack1..3, Hurt, Death, Dash, Crouch...), or NAME_None. */
	static FName MatchAnimation(const FString& Name);

	static FText GetKindName(EOnyxArtKind Kind);

	/** Separate pieces of art in an image (props on a transparent sheet), close bits merged, specks dropped. */
	static TArray<FIntRect> FindRegions(const TArray<FColor>& Pixels, FIntPoint Size);

	/** Reads a PNG / image file as BGRA pixels. */
	static bool LoadPixels(const FString& File, TArray<FColor>& OutPixels, FIntPoint& OutSize);
};
