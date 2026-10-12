// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Builders/OnyxNormalMaps.h"

class AOnyxCharacter;
class UBlueprint;
class UTexture2D;

/** One clip cut from a sprite sheet: its role, a row, a start column and a frame count. */
struct FOnyxClipSpec
{
	FName Name;
	bool bEnabled = true;
	/** Optional separate image for this clip (strip sheets: one PNG per clip). */
	TWeakObjectPtr<UTexture2D> TextureOverride;
	int32 Row = 0;
	int32 StartColumn = 0;
	/** 0 = use every frame to the end of the row. */
	int32 FrameCount = 0;
	float FramesPerSecond = 10.f;
	bool bLoop = true;
};

struct FOnyxCharacterSpec
{
	FString Name = TEXT("Hero");
	FString Folder = TEXT("/Game/Characters");
	/** What the character does: a mask of EOnyxBehaviour (each one is a component). Default: the player's set. */
	int32 Behaviours = 1 | 2 | 4; // Player Control, Camera, Respawn
	/** Who it fights for (OnyxTeams: 0 neutral, 1 player, 2 enemy): attacks and contact damage only hit other teams. */
	uint8 Team = 1;
	TWeakObjectPtr<UTexture2D> SpriteSheet;
	FIntPoint FrameSize = FIntPoint(64, 64);
	/** World size of one texture pixel in cm, the same as the tiles (4 = a 64 px frame is 256 cm tall). */
	float CentimetersPerPixel = 4.f;
	/** Transparent pixels under the feet in each frame, so the character stands on the ground. */
	int32 PixelsBelowFeet = 0;
	/** Size the capsule to the visible pixels of Idle (or the first clip) and derive PixelsBelowFeet from them. */
	bool bAutoFitCapsule = false;
	/** Give every frame a normal map made from the art (shaded in relief by 2D lights). */
	bool bGenerateNormalMap = true;
	FOnyxNormalMapSettings NormalMap;
	TArray<FOnyxClipSpec> Clips;

	/** Idle, Run, Jump, Fall, Attack1-3, Hurt, Death, Dash, WallSlide - one row each. */
	static TArray<FOnyxClipSpec> DefaultClips();

	/**
	 * One clip per strip texture (role -> a row of frames); standard roles take their speed and loop from the presets,
	 * FramesPerSecond overrides per role. The capsule is fitted to Idle.
	 */
	static FOnyxCharacterSpec FromStrips(const TMap<FName, UTexture2D*>& Strips, const FString& Name, const FString& Folder, FIntPoint FrameSize,
		int32 Behaviours, uint8 Team, float CentimetersPerPixel, const TMap<FName, float>& FramesPerSecond);
};

struct FOnyxCharacterBuildResult
{
	UBlueprint* Blueprint = nullptr;
	class UOnyxMoveset* Moveset = nullptr;
	int32 SpriteCount = 0;
	int32 FlipbookCount = 0;
	/** Radius / half height when the capsule was auto fitted, zero otherwise. */
	FVector2D CapsuleSize = FVector2D::ZeroVector;
	TArray<FString> Warnings;
	FText Error;

	bool Succeeded() const { return Blueprint != nullptr; }
};

/** Turns a sprite sheet description into sprites, flipbooks, Clips, a Moveset and a character Blueprint. */
class FOnyxCharacterBuilder
{
public:
	static FOnyxCharacterBuildResult Build(const FOnyxCharacterSpec& Spec);

	/**
	 * A character Blueprint (Folder/Name/BP_Name) that plays Moveset: capsule fitted to the visible pixels of Idle, the
	 * given behaviours (mask of EOnyxBehaviour) and team. Null with OutError set on failure.
	 */
	static UBlueprint* CreateFromMoveset(const FString& Name, const FString& Folder, class UOnyxMoveset* Moveset, int32 Behaviours, uint8 Team,
		float CentimetersPerPixel, FText& OutError);

	/** Texture used by a clip (its override, or the main sheet). */
	static UTexture2D* ResolveTexture(const FOnyxCharacterSpec& Spec, const FOnyxClipSpec& ClipSpec);

	/** Frame count after resolving "0 = to the end of the row". */
	static int32 ResolveFrameCount(const FOnyxCharacterSpec& Spec, const FOnyxClipSpec& ClipSpec);

	/** Pixel rectangle of each frame (for previews). */
	static TArray<FIntRect> GetFrameRects(const FOnyxCharacterSpec& Spec, const FOnyxClipSpec& ClipSpec);

private:
	static void FitCapsule(const FOnyxCharacterSpec& Spec, AOnyxCharacter* Defaults, int32& OutPixelsBelowFeet, FOnyxCharacterBuildResult& Result);
};
