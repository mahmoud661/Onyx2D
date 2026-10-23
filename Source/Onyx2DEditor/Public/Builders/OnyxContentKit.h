// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Builders/OnyxPixelCanvas.h"
#include "Engine/World.h"
#include "Lighting/OnyxLight2D.h"

class AOnyxCharacter;
class AOnyxPickup;
class APaperSpriteActor;
class UActorComponent;
class UBlueprint;
class UOnyxAutotileSet;
class UOnyxEnemyAIComponent;
class UOnyxPatrolComponent;
class UPackage;
class UPaperFlipbook;
class UPaperSprite;
class UPaperTileSet;
class USoundBase;
class UTexture2D;
class UWidgetBlueprint;
struct FOnyxColorGrade;

/**
 * What a game's content builder needs to turn art into a level from C++: importing pictures and frames into a content
 * folder, flipbooks (also drawn in code), sprites and tile sets, foes with Enemy AI, the shared effects (hit spark, dust,
 * bursts), coins, a HUD with its game mode, and placing sprites, lights and coins on a map's pixel grid. The Onyx2D
 * examples are built with it, and so can your game. Failures are reported through Error.
 */
class ONYX2DEDITOR_API FOnyxContentKit
{
public:
	/** ContentFolder: where everything goes (/Game/...). PixelSize: world size of one art pixel, cm. */
	FOnyxContentKit(const FString& ContentFolder, float PixelSize);
	virtual ~FOnyxContentKit() = default;

	// --- Art --------------------------------------------------------------------------------------------------------

	UTexture2D* ImportPng(const FString& File, const FString& Folder, const FString& Name);
	/** Imports a picture and re-colours it with Grade (none: as painted). */
	UTexture2D* Import(const FString& File, const FString& Folder, const FString& Name, const FOnyxColorGrade* Grade);
	/** Every numbered frame in a folder (name-1.png, name2.png...), in number order. */
	TArray<UTexture2D*> ImportFrames(const FString& Directory, const FString& Prefix);
	/** A canvas saved as a pixel-art texture in Folder (under the content folder). */
	UTexture2D* CanvasTexture(const FOnyxPixelCanvas& Canvas, const FString& Folder, const FString& Name);
	UTexture2D* Strip(const TArray<UTexture2D*>& Frames, const FString& Name, FIntPoint& OutFrameSize);
	UPaperFlipbook* Flipbook(const TArray<UTexture2D*>& Frames, const FString& Name, float FramesPerSecond);
	/** bEffect: whole frames centred on the spot, see-through and unlit (they can fade), for effects played where something happens. */
	UPaperFlipbook* DrawnFlipbook(const TArray<FOnyxPixelCanvas>& Frames, const FString& Name, float FramesPerSecond, bool bEffect = false);
	/** A whole picture as a lit sprite standing on its lowest pixel; Background: a flat colour painted behind it, made see-through. */
	UPaperSprite* Prop(const FString& File, const FString& Name, const FOnyxColorGrade* Grade, const FColor* Background = nullptr);
	/** Makes every pixel of exactly this colour see-through (a flat background the artist painted behind a picture). */
	static int32 ClearColor(UTexture2D* Texture, const FColor& Color);
	/** Part of a picture as a lit sprite standing on its bottom edge. */
	UPaperSprite* SpriteFrom(UTexture2D* Texture, FIntPoint Offset, FIntPoint Size, const FString& Name);
	/** A terrain's autotile set (Onyx Autotile) from its drawn block and fills: one atlas texture, a sprite per tile, the set asset. */
	UOnyxAutotileSet* MakeAutotileSet(const FOnyxPixelCanvas& Block, const TArray<FOnyxPixelCanvas>& Fills, const FString& Name);
	/** A tile set of TileSize tiles: every tile scenery, except the Solid ones and the One-Way platforms. */
	UPaperTileSet* MakeTileSet(const FString& File, const FString& Name, const FOnyxColorGrade* Grade, const TArray<int32>& Solid, const TArray<int32>& OneWay,
		FIntPoint TileSize = FIntPoint(16, 16));

	// --- Characters -------------------------------------------------------------------------------------------------

	/**
	 * A foe with the fighting-foe behaviours (Patrol, Contact Damage, Enemy AI) from strips of frames by role, rebuilt over
	 * an older version of itself. Setup tunes it, its patrol and its AI (it drops 2 coins unless told otherwise; call
	 * MakeCoins first). FeetRowsFromBottom, bFitToBody: see UOnyxEditorLibrary::CreateCharacterFromStrips.
	 */
	UBlueprint* MakeFoe(const FString& Name, const TMap<FName, UTexture2D*>& Strips, const TMap<FName, float>& Speeds, FIntPoint FrameSize, bool bFacesLeft,
		UPaperFlipbook* DeathEffect, TFunctionRef<void(AOnyxCharacter&, UOnyxPatrolComponent&, UOnyxEnemyAIComponent&)> Setup, int32 FeetRowsFromBottom = -1,
		bool bFitToBody = false);

	/** A light that goes where the character's sprite goes (it turns with it). */
	void AddGlow(UBlueprint* Blueprint, const FLinearColor& Color, float Intensity, float Radius, const FVector2D& Offset, float Flicker);

	/** A Blueprint's component of a class, added to its construction script (named Name) if it has none yet. */
	static UActorComponent* AddComponent(UBlueprint* Blueprint, UClass* Class, FName Name);
	template <class T>
	static T* AddComponent(UBlueprint* Blueprint, FName Name)
	{
		return Cast<T>(AddComponent(Blueprint, T::StaticClass(), Name));
	}
	/** Takes out a component a Blueprint no longer needs (rebuilding what an older version of a builder made). */
	static void RemoveComponent(UBlueprint* Blueprint, UClass* Class);

	// --- Effects, coins, HUD, game mode -----------------------------------------------------------------------------

	struct FEffects
	{
		/** Where a blow lands: a white flash, rays and a ring cooling to blue. */
		UPaperFlipbook* HitSpark = nullptr;
		/** Kicked up at the feet (jumping, landing, dashing). */
		UPaperFlipbook* Dust = nullptr;
		/** A burst of fire (a fireball hitting something). */
		UPaperFlipbook* FireBurst = nullptr;
		/** A wisp of ghost-fire, flying right (a projectile). */
		UPaperFlipbook* Wisp = nullptr;
		UPaperFlipbook* WispBurst = nullptr;
		/** Motes of light drawn in to the middle, about a second long (mending: Onyx Energy's Gather Effect). */
		UPaperFlipbook* Gather = nullptr;
		/** A ring of light going out (a mend that healed: Mended Effect). */
		UPaperFlipbook* Mended = nullptr;
		/** Blades meeting: a white star (a clash). */
		UPaperFlipbook* Clash = nullptr;
	};
	/** Draws the shared effects (once). */
	bool MakeEffects();
	FEffects Effects;

	struct FCoins
	{
		UPaperFlipbook* Coin = nullptr;
		UPaperFlipbook* Sparkle = nullptr;
		/** Picked up (set by MakeSounds; coins placed after it chime). */
		USoundBase* Sound = nullptr;
	};
	/** A spinning gold coin and the sparkle it leaves, drawn in code (with the HUD's icons). */
	bool MakeCoins(FCoins& Out);
	FCoins Coins;

	struct FHudLook
	{
		/** A health point (none: a drop of pale blue light). */
		UTexture2D* Pip = nullptr;
		FVector2D PipSize = FVector2D(36.f, 48.f);
		FLinearColor EmptyPipColor = FLinearColor(0.08f, 0.11f, 0.2f, 0.75f);
		FLinearColor TextColor = FLinearColor(0.82f, 0.92f, 1.f);
		/** The energy cells under the health (shown when the hero has Onyx Energy). */
		FLinearColor EnergyColor = FLinearColor(0.72f, 0.95f, 1.f);
	};
	/**
	 * Sound effects synthesised in code (FOnyxSoundSynth), into Sounds/: what an action game's hero, foes and pickups need.
	 * A game can put its own in their place (recordings, cues: FOnyxSoundCueBuilder) before it builds what uses them.
	 */
	struct FSounds
	{
		/** A blade's swish, its impact, blades meeting. */
		USoundBase* Swing = nullptr;
		USoundBase* Hit = nullptr;
		USoundBase* Clash = nullptr;
		USoundBase* Jump = nullptr;
		USoundBase* Land = nullptr;
		USoundBase* Dash = nullptr;
		USoundBase* Step = nullptr;
		USoundBase* Hurt = nullptr;
		USoundBase* Death = nullptr;
		USoundBase* FoeDeath = nullptr;
		USoundBase* Coin = nullptr;
		/** Mending: energy gathering in, a wound mended. */
		USoundBase* Gather = nullptr;
		USoundBase* Mended = nullptr;
		/** A resting place reached. */
		USoundBase* Checkpoint = nullptr;
		/** A projectile thrown, a projectile bursting. */
		USoundBase* Shot = nullptr;
		USoundBase* Burst = nullptr;
	};
	/** Makes the sounds (once); the coins made by MakeCoins chime with Coin from then on. */
	bool MakeSounds();
	FSounds Sounds;

	/** BP_<Name>GameMode: spawns Hero and shows WBP_<Name>HUD (health pips, energy cells, the coins under them). Needs MakeCoins first. */
	UBlueprint* MakeGameMode(const FString& Name, UBlueprint* Hero, const FHudLook& Look = FHudLook());

	// --- Placing, on a map's pixel grid (y down from its top-left corner) -------------------------------------------

	/** Reads where the level's tile map starts (Paper2D centres tile 0, 0 on the actor, so it's measured, never assumed). */
	void FindMapCorner(UWorld* World);
	FVector WorldAt(float PixelX, float PixelY, float Depth = 0.f) const;

	template <class T>
	T* Spawn(UWorld* World, const FVector& Location, const FString& Label)
	{
		T* Actor = World->SpawnActor<T>(Location, FRotator::ZeroRotator);
		if (Actor)
		{
			Actor->SetActorLabel(Label);
		}
		return Actor;
	}

	/** A sprite (bottom-centre pivot) with its middle at PixelX, standing at BottomPixelY; Layer: Background, Decor or none (props). */
	APaperSpriteActor* PlaceSprite(UWorld* World, UPaperSprite* Sprite, float PixelX, float BottomPixelY, const FString& Label, FName Layer = NAME_None,
		float Depth = 0.f, const FLinearColor& Color = FLinearColor::White);

	struct FLightLook
	{
		EOnyxLightPreset Preset;
		FLinearColor Color;
		float Intensity;
		float Radius;
		/** 0 = no glow in the air. */
		float HaloSize;
		float HaloIntensity;
		bool bShadows;
	};
	static FLightLook Torch();
	static FLightLook Candles();
	static FLightLook Moonlight();
	AOnyxLight2D* AddLight(UWorld* World, const FVector2D& Pixel, const FLightLook& Look, const FString& Label);
	AOnyxPickup* PlaceCoin(UWorld* World, const FVector2D& Pixel, const FCoins& CoinArt, const FString& Label);

	/** Deletes an asset and moves what is left of it in memory out of the way, so a new asset can take its name. */
	static void ClearAssetPath(const FString& PackageName);

	bool Fail(const FText& Reason)
	{
		Error = Reason;
		return false;
	}

	const FString Root;
	/** World size of one art pixel, cm. */
	const float PixelSize;
	FText Error;
	TArray<UPackage*> Dirty;

protected:
	UWidgetBlueprint* MakeHUD(const FString& Name, const FHudLook& Look);

	FVector MapCorner = FVector::ZeroVector;
	UTexture2D* DropIcon = nullptr;
	UTexture2D* CoinIcon = nullptr;
};
