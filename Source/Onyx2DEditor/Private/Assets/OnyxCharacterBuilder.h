// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Assets/OnyxNormalMaps.h"

class AOnyxCharacter;
class UBlueprint;
class UTexture2D;

/** One animation cut from a sprite sheet: a row, a start column and a frame count. */
struct FOnyxAnimationSpec
{
	FName Name;
	bool bEnabled = true;
	/** Optional separate image for this animation (strip sheets: one PNG per animation). */
	TWeakObjectPtr<UTexture2D> TextureOverride;
	int32 Row = 0;
	int32 StartColumn = 0;
	/** 0 = use every frame to the end of the row. */
	int32 FrameCount = 0;
	float FramesPerSecond = 10.f;
	bool bLoop = true;
};

enum class EOnyxCharacterKind : uint8
{
	Player,
	Enemy
};

struct FOnyxCharacterSpec
{
	FString Name = TEXT("Hero");
	FString Folder = TEXT("/Game/Characters");
	EOnyxCharacterKind Kind = EOnyxCharacterKind::Player;
	TWeakObjectPtr<UTexture2D> SpriteSheet;
	FIntPoint FrameSize = FIntPoint(64, 64);
	/** World size of one texture pixel in cm, the same as the tiles (4 = a 64 px frame is 256 cm tall). */
	float CentimetersPerPixel = 4.f;
	/** Transparent pixels under the feet in each frame, so the character stands on the ground. */
	int32 PixelsBelowFeet = 0;
	/** Size the capsule to the visible pixels of Idle (or the first animation) and derive PixelsBelowFeet from them. */
	bool bAutoFitCapsule = false;
	/** Give every frame a normal map made from the art (shaded in relief by 2D lights). */
	bool bGenerateNormalMap = true;
	FOnyxNormalMapSettings NormalMap;
	TArray<FOnyxAnimationSpec> Animations;

	/** Idle, Run, Jump, Fall, Attack1-3, Hurt, Death, Dash, WallSlide - one row each. */
	static TArray<FOnyxAnimationSpec> DefaultAnimations();
};

struct FOnyxCharacterBuildResult
{
	UBlueprint* Blueprint = nullptr;
	int32 SpriteCount = 0;
	int32 FlipbookCount = 0;
	/** Radius / half height when the capsule was auto fitted, zero otherwise. */
	FVector2D CapsuleSize = FVector2D::ZeroVector;
	TArray<FString> Warnings;
	FText Error;

	bool Succeeded() const { return Blueprint != nullptr; }
};

/** Turns a sprite sheet description into sprites, flipbooks and a ready-to-play character Blueprint. */
class FOnyxCharacterBuilder
{
public:
	static FOnyxCharacterBuildResult Build(const FOnyxCharacterSpec& Spec);

	/** Texture used by an animation (its override, or the main sheet). */
	static UTexture2D* ResolveTexture(const FOnyxCharacterSpec& Spec, const FOnyxAnimationSpec& Animation);

	/** Frame count after resolving "0 = to the end of the row". */
	static int32 ResolveFrameCount(const FOnyxCharacterSpec& Spec, const FOnyxAnimationSpec& Animation);

	/** Pixel rectangle of each frame (for previews). */
	static TArray<FIntRect> GetFrameRects(const FOnyxCharacterSpec& Spec, const FOnyxAnimationSpec& Animation);

private:
	static void FitCapsule(const FOnyxCharacterSpec& Spec, AOnyxCharacter* Defaults, int32& OutPixelsBelowFeet, FOnyxCharacterBuildResult& Result);
};
