// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UPaperSprite;
class UTexture2D;

/** One sprite as an editor panel should draw it: the texture area and where it sits in the world (X right, Z up). */
struct FOnyxSpriteDraw
{
	UTexture2D* Texture = nullptr;
	/** The sprite's area of the texture, in UVs. */
	FBox2f UV = FBox2f(FVector2f::ZeroVector, FVector2f::UnitVector);
	/** Its pixel size in the texture. */
	FIntPoint Pixels = FIntPoint::ZeroValue;
	/** World rectangle (X, Z) it covers. */
	FVector2D WorldMin = FVector2D::ZeroVector;
	FVector2D WorldMax = FVector2D::ZeroVector;
	/** Mirrored left-right (the actor faces left). */
	bool bFlipX = false;
	FLinearColor Color = FLinearColor::White;
	/** World Y, to draw back to front. */
	double Depth = 0.0;
};

/**
 * What actors look like in 2D, for editor panels (the Level Painter canvas and palette): their sprites and flipbooks,
 * animated, placed exactly as in the level.
 */
namespace OnyxActorPreview
{
	/** The sprites an actor shows at Time (seconds; flipbooks loop), in world space. */
	void Gather(const AActor* Actor, double Time, TArray<FOnyxSpriteDraw>& OutDraws);

	/**
	 * The sprite an actor of Class shows when placed (its Moveset's Idle clip, or its default flipbook / sprite, also from
	 * Blueprint components) at Time, relative to the actor's origin. False if it has none.
	 */
	bool GetClassSprite(const UClass* Class, double Time, FOnyxSpriteDraw& OutDraw);

	/** A sprite drawn with a component transform. */
	bool MakeDraw(const UPaperSprite* Sprite, const FTransform& Transform, FOnyxSpriteDraw& OutDraw);
}
