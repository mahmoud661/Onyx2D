// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxArtImporter.h"
#include "Algo/Unique.h"
#include "Builders/OnyxAssetUtils.h"
#include "Builders/OnyxCharacterBuilder.h"
#include "Builders/OnyxImageAnalysis.h"
#include "Builders/OnyxTileBuilder.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Internationalization/Regex.h"
#include "Misc/Paths.h"
#include "Character/OnyxBehaviourLibrary.h"
#include "Combat/OnyxCombatTypes.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PaperTileSet.h"

namespace
{
	constexpr uint8 ArtVisibleAlpha = 8;

	struct FArtFrame
	{
		TArray<FColor> Pixels;
		FIntPoint Size = FIntPoint::ZeroValue;
	};

	/** Lower-case words of a file name: "PlayerRun_02" -> player, run, 02. */
	TArray<FString> Tokenize(const FString& Name)
	{
		TArray<FString> Tokens;
		FString Current;
		auto Flush = [&]()
		{
			if (!Current.IsEmpty())
			{
				Tokens.Add(Current.ToLower());
				Current.Reset();
			}
		};
		for (const TCHAR C : Name)
		{
			if (!FChar::IsAlnum(C))
			{
				Flush();
				continue;
			}
			if (!Current.IsEmpty())
			{
				const TCHAR Previous = Current[Current.Len() - 1];
				if ((FChar::IsLower(Previous) && FChar::IsUpper(C)) || FChar::IsDigit(Previous) != FChar::IsDigit(C))
				{
					Flush();
				}
			}
			Current.AppendChar(C);
		}
		Flush();
		return Tokens;
	}

	bool IsNumber(const FString& Token)
	{
		return !Token.IsEmpty() && Token.IsNumeric();
	}

	/** Words in file names that say "animation" and which Onyx2D animation they mean. */
	const TPair<const TCHAR*, const TCHAR*> AnimationWords[] = {
		{ TEXT("idle"), TEXT("Idle") }, { TEXT("stand"), TEXT("Idle") }, { TEXT("breathing"), TEXT("Idle") },
		{ TEXT("run"), TEXT("Run") }, { TEXT("running"), TEXT("Run") }, { TEXT("walk"), TEXT("Run") }, { TEXT("walking"), TEXT("Run") }, { TEXT("move"), TEXT("Run") },
		{ TEXT("jump"), TEXT("Jump") }, { TEXT("jumping"), TEXT("Jump") }, { TEXT("rise"), TEXT("Jump") },
		{ TEXT("fall"), TEXT("Fall") }, { TEXT("falling"), TEXT("Fall") },
		{ TEXT("attack"), TEXT("Attack1") }, { TEXT("atk"), TEXT("Attack1") }, { TEXT("slash"), TEXT("Attack1") }, { TEXT("shoot"), TEXT("Attack1") }, { TEXT("punch"), TEXT("Attack1") },
		{ TEXT("hurt"), TEXT("Hurt") }, { TEXT("hit"), TEXT("Hurt") }, { TEXT("damage"), TEXT("Hurt") }, { TEXT("damaged"), TEXT("Hurt") },
		{ TEXT("death"), TEXT("Death") }, { TEXT("die"), TEXT("Death") }, { TEXT("dead"), TEXT("Death") }, { TEXT("dying"), TEXT("Death") },
		{ TEXT("dash"), TEXT("Dash") }, { TEXT("roll"), TEXT("Dash") },
		{ TEXT("crouch"), TEXT("Crouch") }, { TEXT("duck"), TEXT("Crouch") },
		{ TEXT("climb"), TEXT("Climb") },
		{ TEXT("wallslide"), TEXT("WallSlide") }, { TEXT("slide"), TEXT("WallSlide") },
	};

	/** Words that describe the file rather than the character ("hero_sheet.png"). */
	const TCHAR* NoiseWords[] = { TEXT("sheet"), TEXT("spritesheet"), TEXT("sprites"), TEXT("sprite"), TEXT("strip"), TEXT("anim"), TEXT("animation"),
		TEXT("animations"), TEXT("frames"), TEXT("frame"), TEXT("png"), TEXT("x"), TEXT("px") };

	const TCHAR* TileWords[] = { TEXT("tile"), TEXT("tiles"), TEXT("tileset"), TEXT("tilemap"), TEXT("terrain"), TEXT("ground"), TEXT("platform"),
		TEXT("platforms"), TEXT("blocks"), TEXT("world"), TEXT("level") };
	const TCHAR* BackgroundWords[] = { TEXT("background"), TEXT("bg"), TEXT("back"), TEXT("sky"), TEXT("parallax"), TEXT("middle"), TEXT("mid"),
		TEXT("far"), TEXT("clouds"), TEXT("mountains"), TEXT("layer") };
	const TCHAR* PlayerWords[] = { TEXT("player"), TEXT("hero"), TEXT("main"), TEXT("knight"), TEXT("character") };

	bool HasWord(const TArray<FString>& Tokens, const TCHAR* const* Words, int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Tokens.Contains(Words[Index]))
			{
				return true;
			}
		}
		return false;
	}
	template <int32 N> bool HasWord(const TArray<FString>& Tokens, const TCHAR* const (&Words)[N]) { return HasWord(Tokens, Words, N); }

	bool IsAnimationToken(const FString& Token)
	{
		for (const TPair<const TCHAR*, const TCHAR*>& Word : AnimationWords)
		{
			if (Token == Word.Key)
			{
				return true;
			}
		}
		return false;
	}

	/** "dark_knight_run" -> DarkKnight: the words that are neither animation, number nor noise. */
	FString CharacterName(const FString& Name)
	{
		FString Result;
		for (const FString& Token : Tokenize(Name))
		{
			bool bNoise = IsNumber(Token) || IsAnimationToken(Token);
			for (const TCHAR* Word : NoiseWords)
			{
				bNoise |= Token == Word;
			}
			if (!bNoise)
			{
				Result += Token.Left(1).ToUpper() + Token.Mid(1);
			}
		}
		return Result;
	}

	/** Splits "player-run-12" into ("player-run", 12); Number is -1 without a trailing number. */
	TPair<FString, int32> SplitFrameNumber(const FString& Name)
	{
		int32 Index = Name.Len();
		while (Index > 0 && FChar::IsDigit(Name[Index - 1]))
		{
			--Index;
		}
		const int32 Number = Index < Name.Len() ? FCString::Atoi(*Name.Mid(Index)) : -1;
		FString Prefix = Name.Left(Index);
		while (!Prefix.IsEmpty() && !FChar::IsAlnum(Prefix[Prefix.Len() - 1]))
		{
			Prefix.LeftChopInline(1);
		}
		return { Prefix, Number };
	}

	float OpaqueShare(const TArray<FColor>& Pixels)
	{
		int32 Opaque = 0;
		for (const FColor& Pixel : Pixels)
		{
			Opaque += Pixel.A > ArtVisibleAlpha ? 1 : 0;
		}
		return Pixels.IsEmpty() ? 0.f : float(Opaque) / Pixels.Num();
	}

	TArray<uint8> AlphaOf(const TArray<FColor>& Pixels)
	{
		TArray<uint8> Alpha;
		Alpha.SetNumUninitialized(Pixels.Num());
		for (int32 Index = 0; Index < Pixels.Num(); ++Index)
		{
			Alpha[Index] = Pixels[Index].A;
		}
		return Alpha;
	}

	bool CellHasContent(const TArray<FColor>& Pixels, FIntPoint Size, const FIntRect& Cell)
	{
		for (int32 Y = Cell.Min.Y; Y < Cell.Max.Y; ++Y)
		{
			for (int32 X = Cell.Min.X; X < Cell.Max.X; ++X)
			{
				if (Pixels[Y * Size.X + X].A > ArtVisibleAlpha)
				{
					return true;
				}
			}
		}
		return false;
	}

	int32 CountCells(const TArray<FColor>& Pixels, FIntPoint Size, FIntPoint Cell, int32 Row)
	{
		int32 Count = 0;
		for (int32 Y = 0; Y + Cell.Y <= Size.Y; Y += Cell.Y)
		{
			if (Row != INDEX_NONE && Y / Cell.Y != Row)
			{
				continue;
			}
			for (int32 X = 0; X + Cell.X <= Size.X; X += Cell.X)
			{
				Count += CellHasContent(Pixels, Size, FIntRect(X, Y, X + Cell.X, Y + Cell.Y)) ? 1 : 0;
			}
		}
		return Count;
	}

	/** Frame size of an animation strip: square frames when the width is a multiple of the height, else the detected grid. */
	FIntPoint StripCellSize(const TArray<FColor>& Pixels, FIntPoint Size)
	{
		if (Size.Y > 0 && Size.X % Size.Y == 0)
		{
			return FIntPoint(Size.Y, Size.Y);
		}
		const TOptional<FIntPoint> Detected = FOnyxImageAnalysis::DetectFrameSize(AlphaOf(Pixels), Size);
		return Detected.IsSet() ? FIntPoint(Detected->X, Size.Y) : Size;
	}

	bool GatherFrames(const FOnyxArtAnimation& Animation, TArray<FArtFrame>& OutFrames, FText& OutError)
	{
		for (const FString& File : Animation.Files)
		{
			FArtFrame Image;
			if (!FOnyxArtImporter::LoadPixels(File, Image.Pixels, Image.Size))
			{
				OutError = FText::Format(INVTEXT("Can't read {0}."), FText::FromString(FPaths::GetCleanFilename(File)));
				return false;
			}
			if (Animation.CellSize.X <= 0 || Animation.CellSize.Y <= 0)
			{
				OutFrames.Add(MoveTemp(Image));
				continue;
			}
			const FIntPoint Cell = Animation.CellSize;
			for (int32 Y = 0; Y + Cell.Y <= Image.Size.Y; Y += Cell.Y)
			{
				if (Animation.Row != INDEX_NONE && Y / Cell.Y != Animation.Row)
				{
					continue;
				}
				for (int32 X = 0; X + Cell.X <= Image.Size.X; X += Cell.X)
				{
					if (!CellHasContent(Image.Pixels, Image.Size, FIntRect(X, Y, X + Cell.X, Y + Cell.Y)))
					{
						continue;
					}
					FArtFrame& Frame = OutFrames.AddDefaulted_GetRef();
					Frame.Size = Cell;
					Frame.Pixels.SetNumUninitialized(Cell.X * Cell.Y);
					for (int32 Row = 0; Row < Cell.Y; ++Row)
					{
						FMemory::Memcpy(&Frame.Pixels[Row * Cell.X], &Image.Pixels[(Y + Row) * Image.Size.X + X], Cell.X * sizeof(FColor));
					}
				}
			}
		}
		if (OutFrames.IsEmpty())
		{
			OutError = FText::Format(INVTEXT("{0} has no visible frames."), FText::FromName(Animation.Name));
			return false;
		}
		return true;
	}

	/** Frames left to right in one strip, each centred on the bottom of a Cell (feet stay on one line). */
	TArray<FColor> PackStrip(const TArray<FArtFrame>& Frames, FIntPoint Cell, FIntPoint& OutSize)
	{
		OutSize = FIntPoint(Cell.X * Frames.Num(), Cell.Y);
		TArray<FColor> Strip;
		Strip.Init(FColor(0, 0, 0, 0), OutSize.X * OutSize.Y);
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			const FArtFrame& Frame = Frames[Index];
			const FIntPoint Offset(Index * Cell.X + (Cell.X - Frame.Size.X) / 2, Cell.Y - Frame.Size.Y);
			for (int32 Y = 0; Y < Frame.Size.Y; ++Y)
			{
				FMemory::Memcpy(&Strip[(Offset.Y + Y) * OutSize.X + Offset.X], &Frame.Pixels[Y * Frame.Size.X], Frame.Size.X * sizeof(FColor));
			}
		}
		return Strip;
	}

	/** Sum of colour + alpha differences between two pixels. */
	int32 Difference(const FColor& A, const FColor& B)
	{
		return FMath::Abs(A.R - B.R) + FMath::Abs(A.G - B.G) + FMath::Abs(A.B - B.B) + FMath::Abs(A.A - B.A);
	}

	bool IsClearColumn(const TArray<FColor>& Pixels, FIntPoint Size, int32 X)
	{
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			if (Pixels[Y * Size.X + X].A > ArtVisibleAlpha)
			{
				return false;
			}
		}
		return true;
	}

	bool IsClearRow(const TArray<FColor>& Pixels, FIntPoint Size, int32 Y)
	{
		for (int32 X = 0; X < Size.X; ++X)
		{
			if (Pixels[Y * Size.X + X].A > ArtVisibleAlpha)
			{
				return false;
			}
		}
		return true;
	}

	/** True when a grid of Tile cells with Spacing-wide transparent gaps (and Margin border) fits the image exactly. */
	bool FitsGappedGrid(const TArray<FColor>& Pixels, FIntPoint Size, FIntPoint Tile, int32 Margin, int32 Spacing)
	{
		const FIntPoint Period = Tile + FIntPoint(Spacing, Spacing);
		const FIntPoint Inner = Size - FIntPoint(Margin * 2, Margin * 2) + FIntPoint(Spacing, Spacing);
		if (Inner.X <= 0 || Inner.Y <= 0 || Inner.X % Period.X != 0 || Inner.Y % Period.Y != 0)
		{
			return false;
		}
		const FIntPoint Cells(Inner.X / Period.X, Inner.Y / Period.Y);
		if (Cells.X < 2 && Cells.Y < 2)
		{
			return false;
		}
		for (int32 Column = 1; Column < Cells.X; ++Column)
		{
			for (int32 Gap = 0; Gap < Spacing; ++Gap)
			{
				if (!IsClearColumn(Pixels, Size, Margin + Column * Period.X - Spacing + Gap))
				{
					return false;
				}
			}
		}
		for (int32 Row = 1; Row < Cells.Y; ++Row)
		{
			for (int32 Gap = 0; Gap < Spacing; ++Gap)
			{
				if (!IsClearRow(Pixels, Size, Margin + Row * Period.Y - Spacing + Gap))
				{
					return false;
				}
			}
		}
		return true;
	}

	/** How much sharper colour changes are on the grid lines of a Tile-sized grid than anywhere else (tile edges show up). */
	double EdgeScore(const TArray<FColor>& Pixels, FIntPoint Size, int32 Tile, double AverageDifference)
	{
		double Sum = 0.0;
		int64 Count = 0;
		for (int32 X = Tile; X < Size.X; X += Tile)
		{
			for (int32 Y = 0; Y < Size.Y; ++Y)
			{
				Sum += Difference(Pixels[Y * Size.X + X - 1], Pixels[Y * Size.X + X]);
				++Count;
			}
		}
		for (int32 Y = Tile; Y < Size.Y; Y += Tile)
		{
			for (int32 X = 0; X < Size.X; ++X)
			{
				Sum += Difference(Pixels[(Y - 1) * Size.X + X], Pixels[Y * Size.X + X]);
				++Count;
			}
		}
		return Count > 0 ? (Sum / Count) / (AverageDifference + 1e-3) : 0.0;
	}

	/** Of the art touching the grid lines (every Cell pixels), the share that continues across a line (is cut). */
	float CutShare(const TArray<FColor>& Pixels, FIntPoint Size, int32 Cell)
	{
		int64 Cuts = 0;
		int64 Total = 0;
		for (int32 X = Cell; X < Size.X; X += Cell)
		{
			for (int32 Y = 0; Y < Size.Y; ++Y)
			{
				const bool bBefore = Pixels[Y * Size.X + X - 1].A > ArtVisibleAlpha;
				const bool bAfter = Pixels[Y * Size.X + X].A > ArtVisibleAlpha;
				Cuts += bBefore && bAfter ? 1 : 0;
				Total += bBefore || bAfter ? 1 : 0;
			}
		}
		for (int32 Y = Cell; Y < Size.Y; Y += Cell)
		{
			for (int32 X = 0; X < Size.X; ++X)
			{
				const bool bBefore = Pixels[(Y - 1) * Size.X + X].A > ArtVisibleAlpha;
				const bool bAfter = Pixels[Y * Size.X + X].A > ArtVisibleAlpha;
				Cuts += bBefore && bAfter ? 1 : 0;
				Total += bBefore || bAfter ? 1 : 0;
			}
		}
		return Total > 0 ? float(Cuts) / Total : 0.f;
	}

	/** Starting guess, changed in the list: the player's behaviours for a hero, a patrolling foe otherwise. */
	void SetRole(FOnyxArtItem& Item, bool bHero)
	{
		Item.Behaviours = int32(bHero ? UOnyxBehaviourLibrary::PlayerPreset : UOnyxBehaviourLibrary::PatrolFoePreset);
		Item.Team = bHero ? OnyxTeams::Player : OnyxTeams::Enemy;
	}

	const int32 TileCandidates[] = { 8, 10, 12, 16, 18, 20, 24, 32, 48, 64 };

}

FText FOnyxArtImporter::GetKindName(EOnyxArtKind Kind)
{
	switch (Kind)
	{
	case EOnyxArtKind::TileSet: return INVTEXT("Tile Set");
	case EOnyxArtKind::Character: return INVTEXT("Character");
	case EOnyxArtKind::Sprite: return INVTEXT("Sprite");
	case EOnyxArtKind::Background: return INVTEXT("Background");
	default: return INVTEXT("Skip");
	}
}

FText FOnyxArtItem::Describe() const
{
	switch (Kind)
	{
	case EOnyxArtKind::TileSet:
	{
		const FIntPoint Cells((ImageSize.X - Margin * 2 + Spacing) / FMath::Max(TileSize.X + Spacing, 1), (ImageSize.Y - Margin * 2 + Spacing) / FMath::Max(TileSize.Y + Spacing, 1));
		FString Text = FString::Printf(TEXT("%d x %d px tiles, %d x %d grid"), TileSize.X, TileSize.Y, Cells.X, Cells.Y);
		if (Spacing > 0 || Margin > 0)
		{
			Text += FString::Printf(TEXT(", %d px gaps"), Spacing);
		}
		return FText::FromString(Text + TEXT(" - padded, lit, collision from pixels"));
	}
	case EOnyxArtKind::Character:
	{
		TArray<FString> Parts;
		for (const FOnyxArtAnimation& Animation : Animations)
		{
			Parts.Add(FString::Printf(TEXT("%s %d"), *Animation.Name.ToString(), Animation.FrameCount));
		}
		return FText::FromString(FString::Join(Parts, TEXT(", ")));
	}
	case EOnyxArtKind::Sprite:
		if (!Regions.IsEmpty())
		{
			return FText::FromString(FString::Printf(TEXT("Sprite sheet: %d separate sprites"), Regions.Num()));
		}
		return Animations.IsEmpty()
			? FText::FromString(FString::Printf(TEXT("%d x %d px sprite"), ImageSize.X, ImageSize.Y))
			: FText::FromString(FString::Printf(TEXT("Animated sprite, %d frames"), Animations[0].FrameCount));
	case EOnyxArtKind::Background:
		return FText::FromString(FString::Printf(TEXT("%d x %d px texture for the Parallax tab"), ImageSize.X, ImageSize.Y));
	default:
		return FOnyxArtImporter::GetKindName(Kind);
	}
}

bool FOnyxArtImporter::LoadPixels(const FString& File, TArray<FColor>& OutPixels, FIntPoint& OutSize)
{
	FImage Image;
	if (!FImageUtils::LoadImage(*File, Image))
	{
		return false;
	}
	FImage Converted;
	Image.CopyTo(Converted, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	OutSize = FIntPoint(Converted.SizeX, Converted.SizeY);
	const TArrayView64<FColor> Colors = Converted.AsBGRA8();
	OutPixels = TArray<FColor>(Colors.GetData(), int32(Colors.Num()));
	return OutPixels.Num() == OutSize.X * OutSize.Y && OutPixels.Num() > 0;
}

TArray<FIntRect> FOnyxArtImporter::FindRegions(const TArray<FColor>& Pixels, FIntPoint Size)
{
	constexpr int32 MergeGap = 2;   // pieces this close belong together (a sign and its post)
	constexpr int32 MinPixels = 12; // smaller specks are dropped

	TArray<FIntRect> Regions;
	TBitArray<> Seen(false, Pixels.Num());
	TArray<int32> Stack;
	for (int32 Start = 0; Start < Pixels.Num(); ++Start)
	{
		if (Seen[Start] || Pixels[Start].A <= ArtVisibleAlpha)
		{
			continue;
		}
		FIntRect Box(Start % Size.X, Start / Size.X, Start % Size.X + 1, Start / Size.X + 1);
		int32 Count = 0;
		Stack.Reset();
		Stack.Add(Start);
		Seen[Start] = true;
		while (!Stack.IsEmpty())
		{
			const int32 Index = Stack.Pop(EAllowShrinking::No);
			const int32 X = Index % Size.X;
			const int32 Y = Index / Size.X;
			Box.Include(FIntPoint(X, Y));
			Box.Max = Box.Max.ComponentMax(FIntPoint(X + 1, Y + 1));
			++Count;
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					const int32 NX = X + DX;
					const int32 NY = Y + DY;
					if (NX < 0 || NY < 0 || NX >= Size.X || NY >= Size.Y)
					{
						continue;
					}
					const int32 Next = NY * Size.X + NX;
					if (!Seen[Next] && Pixels[Next].A > ArtVisibleAlpha)
					{
						Seen[Next] = true;
						Stack.Add(Next);
					}
				}
			}
		}
		if (Count >= MinPixels)
		{
			Regions.Add(Box);
		}
	}

	// Merge pieces that touch or nearly touch.
	for (bool bMerged = true; bMerged;)
	{
		bMerged = false;
		for (int32 A = 0; A < Regions.Num() && !bMerged; ++A)
		{
			for (int32 B = A + 1; B < Regions.Num(); ++B)
			{
				const FIntRect Grown(Regions[A].Min - FIntPoint(MergeGap), Regions[A].Max + FIntPoint(MergeGap));
				if (Grown.Intersect(Regions[B]))
				{
					Regions[A].Union(Regions[B]);
					Regions.RemoveAtSwap(B);
					bMerged = true;
					break;
				}
			}
		}
	}
	// Reading order: top to bottom, left to right.
	Regions.Sort([](const FIntRect& A, const FIntRect& B) { return A.Min.Y != B.Min.Y ? A.Min.Y < B.Min.Y : A.Min.X < B.Min.X; });
	return Regions;
}

FName FOnyxArtImporter::MatchAnimation(const FString& Name)
{
	const TArray<FString> Tokens = Tokenize(Name);
	for (int32 Index = 0; Index < Tokens.Num(); ++Index)
	{
		for (const TPair<const TCHAR*, const TCHAR*>& Word : AnimationWords)
		{
			if (Tokens[Index] != Word.Key)
			{
				continue;
			}
			// "attack-2" / "attack2" -> Attack2.
			if (FCString::Strcmp(Word.Value, TEXT("Attack1")) == 0 && Tokens.IsValidIndex(Index + 1) && IsNumber(Tokens[Index + 1]))
			{
				const int32 Number = FCString::Atoi(*Tokens[Index + 1]);
				if (Number >= 1 && Number <= 3)
				{
					return *FString::Printf(TEXT("Attack%d"), Number);
				}
			}
			return Word.Value;
		}
	}
	return NAME_None;
}

void FOnyxArtImporter::DetectTileGrid(const FString& File, const TArray<FColor>& Pixels, FIntPoint Size, FIntPoint& OutTileSize, int32& OutMargin, int32& OutSpacing)
{
	OutMargin = 0;
	OutSpacing = 0;

	// 1. The file name says it: "tiles_16x16.png", "tileset-32px.png".
	TOptional<FIntPoint> Named;
	FRegexMatcher SizeMatcher(FRegexPattern(TEXT("(\\d+)\\s*[xX]\\s*(\\d+)")), FPaths::GetBaseFilename(File));
	if (SizeMatcher.FindNext())
	{
		Named = FIntPoint(FCString::Atoi(*SizeMatcher.GetCaptureGroup(1)), FCString::Atoi(*SizeMatcher.GetCaptureGroup(2)));
	}
	else
	{
		FRegexMatcher PxMatcher(FRegexPattern(TEXT("(\\d+)\\s*px")), FPaths::GetBaseFilename(File));
		if (PxMatcher.FindNext())
		{
			const int32 Px = FCString::Atoi(*PxMatcher.GetCaptureGroup(1));
			Named = FIntPoint(Px, Px);
		}
	}
	if (Named.IsSet() && (Named->X < 4 || Named->Y < 4 || Named->X > Size.X || Named->Y > Size.Y))
	{
		Named.Reset();
	}

	// 2. Transparent gaps between tiles (spaced sheets): the smallest grid whose gaps are all clear.
	for (int32 Spacing = 1; Spacing <= 4; ++Spacing)
	{
		for (const int32 Candidate : TileCandidates)
		{
			const FIntPoint Tile = Named.Get(FIntPoint(Candidate, Candidate));
			for (const int32 Margin : { 0, Spacing })
			{
				if (FitsGappedGrid(Pixels, Size, Tile, Margin, Spacing))
				{
					OutTileSize = Tile;
					OutMargin = Margin;
					OutSpacing = Spacing;
					return;
				}
			}
			if (Named.IsSet())
			{
				break;
			}
		}
	}
	if (Named.IsSet())
	{
		OutTileSize = *Named;
		return;
	}

	// 3. Transparent background: the grid whose lines cut through the least art (item / character sheets). Bigger
	//    multiples cut even less, so take the smallest size that is about as good as the best.
	if (OpaqueShare(Pixels) < 0.9f)
	{
		TArray<TPair<int32, float>> Cuts;
		float Least = 1.f;
		for (const int32 Candidate : TileCandidates)
		{
			if (Size.X % Candidate == 0 && Size.Y % Candidate == 0 && (Size.X / Candidate >= 2 || Size.Y / Candidate >= 2))
			{
				Cuts.Emplace(Candidate, CutShare(Pixels, Size, Candidate));
				Least = FMath::Min(Least, Cuts.Last().Value);
			}
		}
		for (const TPair<int32, float>& Cut : Cuts)
		{
			if (Cut.Value <= 0.35f && Cut.Value <= Least + 0.1f)
			{
				OutTileSize = FIntPoint(Cut.Key, Cut.Key);
				return;
			}
		}
	}

	// 4. Packed sheets: tile edges are where colours change most sharply. Smallest grid that scores near the best.
	double Average = 0.0;
	int64 Count = 0;
	for (int32 Y = 0; Y < Size.Y; ++Y)
	{
		for (int32 X = 1; X < Size.X; ++X)
		{
			Average += Difference(Pixels[Y * Size.X + X - 1], Pixels[Y * Size.X + X]);
			++Count;
		}
	}
	Average = Count > 0 ? Average / Count : 0.0;

	TArray<TPair<int32, double>> Scores;
	double Best = 0.0;
	for (const int32 Candidate : TileCandidates)
	{
		if (Size.X % Candidate == 0 && Size.Y % Candidate == 0 && (Size.X / Candidate >= 2 || Size.Y / Candidate >= 2))
		{
			const double Score = EdgeScore(Pixels, Size, Candidate, Average);
			Scores.Emplace(Candidate, Score);
			Best = FMath::Max(Best, Score);
		}
	}
	for (const TPair<int32, double>& Score : Scores)
	{
		if (Best > 1.2 && Score.Value >= Best * 0.85)
		{
			OutTileSize = FIntPoint(Score.Key, Score.Key);
			return;
		}
	}
	// No clear signal: the most common pixel-art tile size that fits.
	for (const int32 Fallback : { 16, 32, 8, 24, 18, 48, 64 })
	{
		if (Size.X % Fallback == 0 && Size.Y % Fallback == 0)
		{
			OutTileSize = FIntPoint(Fallback, Fallback);
			return;
		}
	}
	OutTileSize = FIntPoint(16, 16);
}

TArray<TSharedPtr<FOnyxArtItem>> FOnyxArtImporter::Analyze(const TArray<FString>& Paths)
{
	// --- Collect PNGs -------------------------------------------------------------------------------
	TArray<FString> Files;
	for (const FString& Path : Paths)
	{
		if (IFileManager::Get().DirectoryExists(*Path))
		{
			TArray<FString> Found;
			IFileManager::Get().FindFilesRecursive(Found, *Path, TEXT("*.png"), true, false);
			Files.Append(Found);
		}
		else if (FPaths::GetExtension(Path).Equals(TEXT("png"), ESearchCase::IgnoreCase))
		{
			Files.Add(Path);
		}
	}
	for (FString& File : Files)
	{
		FPaths::NormalizeFilename(File);
	}
	Files.Sort();
	Files.SetNum(Algo::Unique(Files));

	// --- Group numbered frames (run-1.png, run-2.png...) per folder ------------------------------------
	struct FEntry
	{
		FString Name;
		FString Directory;
		TArray<TPair<int32, FString>> Files;
	};
	TMap<FString, FEntry> Groups;
	for (const FString& File : Files)
	{
		const FString Directory = FPaths::GetPath(File);
		const TPair<FString, int32> Split = SplitFrameNumber(FPaths::GetBaseFilename(File));
		const FString Key = Split.Value >= 0 ? Directory / Split.Key + TEXT("#") : File;
		FEntry& Entry = Groups.FindOrAdd(Key);
		Entry.Name = Split.Value >= 0 ? Split.Key : FPaths::GetBaseFilename(File);
		Entry.Directory = Directory;
		Entry.Files.Emplace(Split.Value, File);
	}

	TArray<TSharedPtr<FOnyxArtItem>> Items;
	TMap<FString, TSharedPtr<FOnyxArtItem>> Characters;
	auto FolderName = [](const FString& Directory, int32 Up)
	{
		FString Result = Directory;
		for (int32 Step = 0; Step < Up; ++Step)
		{
			Result = FPaths::GetPath(Result);
		}
		return FPaths::GetCleanFilename(Result);
	};

	for (TPair<FString, FEntry>& Pair : Groups)
	{
		FEntry& Entry = Pair.Value;
		Entry.Files.Sort([](const TPair<int32, FString>& A, const TPair<int32, FString>& B) { return A.Key < B.Key; });
		const bool bSequence = Entry.Files.Num() > 1;
		TArray<FString> EntryFiles;
		for (const TPair<int32, FString>& File : Entry.Files)
		{
			EntryFiles.Add(File.Value);
		}
		const FString FirstFile = EntryFiles[0];

		// Animation: from the file name, else from the folder ("idle/1.png").
		FName Animation = MatchAnimation(Entry.Name);
		if (Animation.IsNone() && (bSequence || Entry.Name.IsEmpty()))
		{
			Animation = MatchAnimation(FolderName(Entry.Directory, 0));
		}

		if (!Animation.IsNone())
		{
			FString Character = CharacterName(Entry.Name);
			for (int32 Up = 0; Character.IsEmpty() && Up < 3; ++Up)
			{
				Character = CharacterName(FolderName(Entry.Directory, Up));
			}
			Character = Character.IsEmpty() ? TEXT("Character") : Character;

			TSharedPtr<FOnyxArtItem>& Item = Characters.FindOrAdd(Character.ToLower());
			if (!Item.IsValid())
			{
				Item = MakeShared<FOnyxArtItem>();
				Item->Kind = EOnyxArtKind::Character;
				Item->Name = Character;
				Item->Source = Entry.Directory;
				SetRole(*Item, HasWord(Tokenize(Entry.Name + TEXT(" ") + Entry.Directory), PlayerWords));
				Items.Add(Item);
			}
			// Two files for the same animation (e.g. "attack" twice): the second becomes Attack2 / Idle2...
			FName Unique = Animation;
			for (int32 Suffix = 2; Item->Animations.ContainsByPredicate([&](const FOnyxArtAnimation& A) { return A.Name == Unique; }); ++Suffix)
			{
				const FString Base = Animation.ToString().LeftChop(Animation.ToString().EndsWith(TEXT("1")) ? 1 : 0);
				Unique = *FString::Printf(TEXT("%s%d"), *Base, Suffix);
			}

			FOnyxArtAnimation& Anim = Item->Animations.AddDefaulted_GetRef();
			Anim.Name = Unique;
			Anim.Files = EntryFiles;
			if (bSequence)
			{
				Anim.FrameCount = EntryFiles.Num();
			}
			else
			{
				TArray<FColor> Pixels;
				FIntPoint Size;
				if (LoadPixels(FirstFile, Pixels, Size))
				{
					Anim.CellSize = StripCellSize(Pixels, Size);
					Anim.FrameCount = CountCells(Pixels, Size, Anim.CellSize, INDEX_NONE);
				}
			}
			continue;
		}

		// Not an animation.
		TSharedPtr<FOnyxArtItem> Item = MakeShared<FOnyxArtItem>();
		Item->Name = CharacterName(Entry.Name);
		Item->Name = Item->Name.IsEmpty() ? CharacterName(FolderName(Entry.Directory, 0)) : Item->Name;
		Item->Name = Item->Name.IsEmpty() ? TEXT("Art") : Item->Name;
		Item->ImageFile = FirstFile;
		Item->Source = bSequence ? FString::Printf(TEXT("%s (%d frames)"), *Entry.Name, EntryFiles.Num()) : FPaths::GetCleanFilename(FirstFile);
		Items.Add(Item);

		TArray<FColor> Pixels;
		if (!LoadPixels(FirstFile, Pixels, Item->ImageSize))
		{
			Item->Kind = EOnyxArtKind::Skip;
			continue;
		}
		if (bSequence)
		{
			// Coins, gems, torches... : an animated sprite.
			Item->Kind = EOnyxArtKind::Sprite;
			FOnyxArtAnimation& Anim = Item->Animations.AddDefaulted_GetRef();
			Anim.Name = TEXT("Loop");
			Anim.Files = EntryFiles;
			Anim.FrameCount = EntryFiles.Num();
			continue;
		}

		const TArray<FString> Tokens = Tokenize(Entry.Name);
		const float Opaque = OpaqueShare(Pixels);
		if (HasWord(Tokens, BackgroundWords))
		{
			Item->Kind = EOnyxArtKind::Background;
			continue;
		}
		const bool bTileHint = HasWord(Tokens, TileWords);
		if (bTileHint || Opaque >= 0.5f)
		{
			Item->Kind = EOnyxArtKind::TileSet;
			DetectTileGrid(FirstFile, Pixels, Item->ImageSize, Item->TileSize, Item->Margin, Item->Spacing);
			continue;
		}
		int32 Margin = 0;
		int32 Spacing = 0;
		FIntPoint GappedTile;
		DetectTileGrid(FirstFile, Pixels, Item->ImageSize, GappedTile, Margin, Spacing);
		if (Spacing > 0)
		{
			Item->Kind = EOnyxArtKind::TileSet;
			Item->TileSize = GappedTile;
			Item->Margin = Margin;
			Item->Spacing = Spacing;
			continue;
		}

		// A sheet with several rows of frames: a character, one animation per row (Idle, Run, Jump, Fall, Attack, Hurt, Death...).
		const TOptional<FIntPoint> Cell = FOnyxImageAnalysis::DetectFrameSize(AlphaOf(Pixels), Item->ImageSize);
		const FIntPoint Grid = Cell.IsSet() ? FIntPoint(Item->ImageSize.X / Cell->X, Item->ImageSize.Y / Cell->Y) : FIntPoint(1, 1);
		if (Grid.X >= 2 && Grid.Y >= 2)
		{
			Item->Kind = EOnyxArtKind::Character;
			SetRole(*Item, HasWord(Tokens, PlayerWords));
			const TArray<FOnyxClipSpec> Presets = FOnyxCharacterSpec::DefaultClips();
			int32 PresetIndex = 0;
			for (int32 Row = 0; Row < Grid.Y && PresetIndex < Presets.Num(); ++Row)
			{
				const int32 Frames = CountCells(Pixels, Item->ImageSize, *Cell, Row);
				if (Frames == 0)
				{
					continue;
				}
				FOnyxArtAnimation& Anim = Item->Animations.AddDefaulted_GetRef();
				Anim.Name = Presets[PresetIndex++].Name;
				Anim.Files = { FirstFile };
				Anim.CellSize = *Cell;
				Anim.Row = Row;
				Anim.FrameCount = Frames;
			}
			continue;
		}
		if (Grid.X >= 2)
		{
			// One row of frames without an animation name: an animated sprite.
			Item->Kind = EOnyxArtKind::Sprite;
			FOnyxArtAnimation& Anim = Item->Animations.AddDefaulted_GetRef();
			Anim.Name = TEXT("Loop");
			Anim.Files = { FirstFile };
			Anim.CellSize = *Cell;
			Anim.FrameCount = CountCells(Pixels, Item->ImageSize, *Cell, INDEX_NONE);
			continue;
		}
		// Props packed on one transparent sheet: one sprite each.
		Item->Kind = EOnyxArtKind::Sprite;
		Item->Regions = FindRegions(Pixels, Item->ImageSize);
		if (Item->Regions.Num() < 2)
		{
			Item->Regions.Reset();
		}
	}

	// A "character" with no way to stand or move is an effect (enemy-death, item-feedback): an animated sprite.
	for (const TSharedPtr<FOnyxArtItem>& Item : Items)
	{
		if (Item->Kind == EOnyxArtKind::Character && !Item->Animations.ContainsByPredicate([](const FOnyxArtAnimation& A) { return A.Name == TEXT("Idle") || A.Name == TEXT("Run"); }))
		{
			// "eagle-attack" keeps its character's name; "enemy-death" keeps the effect in its name.
			Item->Kind = EOnyxArtKind::Sprite;
			Item->Animations.SetNum(1);
			const FString Effect = Item->Animations[0].Name.ToString();
			Item->Name += Effect.StartsWith(TEXT("Attack")) ? FString() : Effect;
			Item->Source = FString::Printf(TEXT("%s (%d frames)"), *FPaths::GetCleanFilename(Item->Source), Item->Animations[0].FrameCount);
			Item->Animations[0].Name = TEXT("Loop");
		}
		else if (Item->Kind == EOnyxArtKind::Character)
		{
			Item->Source = FString::Printf(TEXT("%s (%d animations)"), *FPaths::GetCleanFilename(Item->Source), Item->Animations.Num());
		}
	}

	// Exactly one character and nobody called "player": that one is the player.
	TArray<TSharedPtr<FOnyxArtItem>> CharacterItems = Items.FilterByPredicate([](const TSharedPtr<FOnyxArtItem>& Item) { return Item->Kind == EOnyxArtKind::Character; });
	if (CharacterItems.Num() == 1)
	{
		SetRole(*CharacterItems[0], true);
	}

	// Unique names per kind.
	TSet<FString> Used;
	for (const TSharedPtr<FOnyxArtItem>& Item : Items)
	{
		const FString Base = FOnyxAssetUtils::SanitizeName(Item->Name);
		FString Name = Base;
		for (int32 Suffix = 2; Used.Contains(Name.ToLower()); ++Suffix)
		{
			Name = FString::Printf(TEXT("%s%d"), *Base, Suffix);
		}
		Used.Add(Name.ToLower());
		Item->Name = Name;
	}

	// Characters and tile sets first: the things a level needs.
	Items.StableSort([](const TSharedPtr<FOnyxArtItem>& A, const TSharedPtr<FOnyxArtItem>& B) { return uint8(A->Kind) < uint8(B->Kind); });
	return Items;
}

bool FOnyxArtImporter::Build(FOnyxArtItem& Item, const FOnyxArtImportSettings& Settings, FText& OutError)
{
	if (!FOnyxAssetUtils::IsValidContentFolder(Settings.Folder))
	{
		OutError = INVTEXT("The folder must start with /Game.");
		return false;
	}
	const FString Name = FOnyxAssetUtils::SanitizeName(Item.Name);
	const FString Folder = Settings.Folder / Name;
	const float PixelsPerUnit = 1.f / FMath::Max(Settings.CentimetersPerPixel, 0.01f);

	switch (Item.Kind)
	{
	case EOnyxArtKind::TileSet:
	{
		UTexture2D* Texture = FOnyxAssetUtils::ImportTexture(Item.ImageFile, Folder, TEXT("T_") + Name);
		if (!Texture)
		{
			OutError = FText::Format(INVTEXT("Could not import {0}."), FText::FromString(FPaths::GetCleanFilename(Item.ImageFile)));
			return false;
		}
		FOnyxAssetUtils::SavePackages({ Texture->GetPackage() });
		FOnyxTileSetSpec Spec;
		Spec.Texture = Texture;
		Spec.TileSize = Item.TileSize;
		Spec.Margin = Item.Margin;
		Spec.Spacing = Item.Spacing;
		Spec.Name = TEXT("TS_") + Name;
		Spec.Folder = Folder;
		UPaperTileSet* TileSet = FOnyxTileBuilder::CreateTileSet(Spec, OutError);
		Item.Result = TileSet;
		return TileSet != nullptr;
	}
	case EOnyxArtKind::Background:
	{
		UTexture2D* Texture = FOnyxAssetUtils::ImportTexture(Item.ImageFile, Folder, TEXT("T_") + Name);
		if (!Texture)
		{
			OutError = FText::Format(INVTEXT("Could not import {0}."), FText::FromString(FPaths::GetCleanFilename(Item.ImageFile)));
			return false;
		}
		FOnyxAssetUtils::SavePackages({ Texture->GetPackage() });
		Item.Result = Texture;
		return true;
	}
	case EOnyxArtKind::Sprite:
	{
		if (Item.Animations.IsEmpty() && !Item.Regions.IsEmpty())
		{
			UTexture2D* Texture = FOnyxAssetUtils::ImportTexture(Item.ImageFile, Folder, TEXT("T_") + Name);
			if (!Texture)
			{
				OutError = FText::Format(INVTEXT("Could not import {0}."), FText::FromString(FPaths::GetCleanFilename(Item.ImageFile)));
				return false;
			}
			TArray<UPackage*> Packages = { Texture->GetPackage() };
			UPaperSprite* First = nullptr;
			for (int32 Index = 0; Index < Item.Regions.Num(); ++Index)
			{
				const FIntRect& Region = Item.Regions[Index];
				if (UPaperSprite* Sprite = FOnyxAssetUtils::CreateSprite(Folder, FString::Printf(TEXT("S_%s_%d"), *Name, Index + 1), Texture, Region.Min,
					Region.Size(), PixelsPerUnit, ESpritePivotMode::Bottom_Center))
				{
					First = First ? First : Sprite;
					Packages.Add(Sprite->GetPackage());
				}
			}
			FOnyxAssetUtils::SavePackages(Packages);
			Item.Result = First;
			return First != nullptr;
		}
		if (Item.Animations.IsEmpty())
		{
			UTexture2D* Texture = FOnyxAssetUtils::ImportTexture(Item.ImageFile, Folder, TEXT("T_") + Name);
			UPaperSprite* Sprite = Texture ? FOnyxAssetUtils::CreateSprite(Folder, TEXT("S_") + Name, Texture, FIntPoint::ZeroValue,
				FIntPoint(Texture->Source.GetSizeX(), Texture->Source.GetSizeY()), PixelsPerUnit, ESpritePivotMode::Bottom_Center) : nullptr;
			if (!Sprite)
			{
				OutError = FText::Format(INVTEXT("Could not make a sprite from {0}."), FText::FromString(FPaths::GetCleanFilename(Item.ImageFile)));
				return false;
			}
			FOnyxAssetUtils::SavePackages({ Texture->GetPackage(), Sprite->GetPackage() });
			Item.Result = Sprite;
			return true;
		}
		TArray<FArtFrame> Frames;
		if (!GatherFrames(Item.Animations[0], Frames, OutError))
		{
			return false;
		}
		FIntPoint Cell(0, 0);
		for (const FArtFrame& Frame : Frames)
		{
			Cell = Cell.ComponentMax(Frame.Size);
		}
		FIntPoint StripSize;
		const TArray<FColor> StripPixels = PackStrip(Frames, Cell, StripSize);
		UTexture2D* Strip = FOnyxAssetUtils::CreateTexture(Folder, TEXT("T_") + Name + TEXT("_Strip"), StripPixels, StripSize);
		if (!Strip)
		{
			OutError = INVTEXT("Could not create the animation texture (the name may be used by another asset).");
			return false;
		}
		TArray<UPaperSprite*> Sprites;
		TArray<UPackage*> Packages = { Strip->GetPackage() };
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			if (UPaperSprite* Sprite = FOnyxAssetUtils::CreateSprite(Folder / TEXT("Frames"), FString::Printf(TEXT("S_%s_%d"), *Name, Index + 1), Strip,
				FIntPoint(Index * Cell.X, 0), Cell, PixelsPerUnit, ESpritePivotMode::Bottom_Center))
			{
				Sprites.Add(Sprite);
				Packages.Add(Sprite->GetPackage());
			}
		}
		UPaperFlipbook* Flipbook = FOnyxAssetUtils::CreateFlipbook(Folder, TEXT("FB_") + Name, Sprites, 10.f);
		if (!Flipbook)
		{
			OutError = INVTEXT("Could not create the flipbook.");
			return false;
		}
		Packages.Add(Flipbook->GetPackage());
		FOnyxAssetUtils::SavePackages(Packages);
		Item.Result = Flipbook;
		return true;
	}
	case EOnyxArtKind::Character:
	{
		// Every animation in strips of one common frame size, feet on the same line.
		TArray<TPair<FName, TArray<FArtFrame>>> AllFrames;
		FIntPoint Cell(0, 0);
		for (const FOnyxArtAnimation& Animation : Item.Animations)
		{
			TArray<FArtFrame> Frames;
			if (!GatherFrames(Animation, Frames, OutError))
			{
				return false;
			}
			for (const FArtFrame& Frame : Frames)
			{
				Cell = Cell.ComponentMax(Frame.Size);
			}
			AllFrames.Emplace(Animation.Name, MoveTemp(Frames));
		}
		TMap<FName, UTexture2D*> Strips;
		TArray<UPackage*> Packages;
		for (const TPair<FName, TArray<FArtFrame>>& Animation : AllFrames)
		{
			FIntPoint StripSize;
			const TArray<FColor> Pixels = PackStrip(Animation.Value, Cell, StripSize);
			UTexture2D* Strip = FOnyxAssetUtils::CreateTexture(Folder / TEXT("Strips"), FString::Printf(TEXT("T_%s_%s"), *Name, *Animation.Key.ToString()), Pixels, StripSize);
			if (!Strip)
			{
				OutError = INVTEXT("Could not create an animation texture (the name may be used by another asset).");
				return false;
			}
			Strips.Add(Animation.Key, Strip);
			Packages.Add(Strip->GetPackage());
		}
		FOnyxAssetUtils::SavePackages(Packages);
		// The character builder makes its own <Name> sub-folder, next to the strips.
		const FOnyxCharacterBuildResult Built = FOnyxCharacterBuilder::Build(
			FOnyxCharacterSpec::FromStrips(Strips, Name, Settings.Folder, Cell, Item.Behaviours, Item.Team, Settings.CentimetersPerPixel, {}));
		if (!Built.Succeeded())
		{
			OutError = Built.Error;
			return false;
		}
		Item.Result = Built.Blueprint;
		return true;
	}
	default:
		return true;
	}
}
