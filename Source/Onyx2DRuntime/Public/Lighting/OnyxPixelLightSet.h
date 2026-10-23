// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/ObjectKey.h"
#include "OnyxPixelLightSet.generated.h"

class UPaperFlipbook;
class UPaperSprite;
class UTexture2D;

/** Which pixels of a character's art give light, and how that light spreads: how a Pixel Light Set is made. */
USTRUCT(BlueprintType)
struct ONYX2DRUNTIME_API FOnyxPixelLightRecipe
{
	GENERATED_BODY()

	/** Pixels of these colours give light (the blade's colours, a lantern's glass, an eye). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Light")
	TArray<FColor> Colors;

	/** How far a pixel's colour may be from one of the Colors and still count (0: exactly; 0.1: close shades too). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Light", meta = (ClampMin = "0", ClampMax = "1"))
	float Tolerance = 0.02f;

	/** Also every pixel brighter than this share of full brightness (0: off), for art without set colours. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Light", meta = (ClampMin = "0", ClampMax = "1"))
	float MinBrightness = 0.f;

	/**
	 * Take the pixels from a glow mask painted for the art instead: a picture the size of the art's texture, white where it gives
	 * light, imported beside it as "<texture>_GlowMask". For a part whose colours others share (a spear painted with the hair's
	 * colours): colours alone would light both.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Light")
	bool bFromPaintedMask = false;

	/** How far the light spreads from those pixels, in art pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Light", meta = (ClampMin = "1", ClampMax = "64"))
	int32 Spread = 10;

	/** The light takes the colour of the pixels it comes from; off, Tint alone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Light")
	bool bPixelColours = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Light")
	FLinearColor Tint = FLinearColor::White;

	/** Size of the light sheet against the art (light is soft: half is plenty). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Light", meta = (ClampMin = "0.125", ClampMax = "1"))
	float Resolution = 0.5f;

	/** True when the pixel is one that gives light (by colour or brightness). */
	bool Matches(const FColor& Pixel) const;

	/** True when it names some pixels: colours, a brightness or a painted mask. */
	bool NamesPixels() const { return bFromPaintedMask || !Colors.IsEmpty() || MinBrightness > 0.f; }
};

/** One frame of art's light: where it is in the sheet, and where it sits around the sprite's pivot (cm, X right, Z up). */
USTRUCT()
struct ONYX2DRUNTIME_API FOnyxPixelLightFrame
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Pixel Light")
	TObjectPtr<UPaperSprite> Sprite;

	UPROPERTY(VisibleAnywhere, Category = "Pixel Light")
	FVector2f UVMin = FVector2f::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Pixel Light")
	FVector2f UVMax = FVector2f::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Pixel Light")
	FVector2f LocalMin = FVector2f::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Pixel Light")
	FVector2f LocalMax = FVector2f::ZeroVector;
};

/**
 * Light that comes from chosen pixels of a character's art: for every frame where they show (a blade mid-swing, a lantern),
 * the light they give, shaped like them and spread softly round them, in one sheet. An Onyx Pixel Light component plays it
 * with the character's animation. Made by the editor (the Moveset Editor's Pixel Light, or FOnyxPixelLightBuilder in C++).
 */
UCLASS(BlueprintType, meta = (DisplayName = "Onyx Pixel Light Set"))
class ONYX2DRUNTIME_API UOnyxPixelLightSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The light of every frame, side by side. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pixel Light")
	TObjectPtr<UTexture2D> Sheet;

	/** The glow round the pixels in the air (the Pixel Light's Halo): the same frames, laid out the same, spread much less. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pixel Light")
	TObjectPtr<UTexture2D> HaloSheet;

	UPROPERTY(VisibleAnywhere, Category = "Pixel Light")
	TArray<FOnyxPixelLightFrame> Frames;

	/** How it was made (rebuilding it uses this). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pixel Light")
	FOnyxPixelLightRecipe Recipe;

	/** The flipbooks it was made from (rebuilding uses them). */
	UPROPERTY(VisibleAnywhere, Category = "Pixel Light")
	TArray<TSoftObjectPtr<UPaperFlipbook>> Sources;

	/** The light of a frame of art (null: that frame gives none). */
	const FOnyxPixelLightFrame* FindFrame(const UPaperSprite* Sprite) const;

private:
	mutable TMap<TObjectKey<UPaperSprite>, int32> Lookup;
	/** How many frames the lookup was made for (made again when they change). */
	mutable int32 LookupFrames = INDEX_NONE;
};
