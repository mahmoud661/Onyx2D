// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpriteEditorOnlyTypes.h"

class UBlueprint;
class UPackage;
class UPaperFlipbook;
class UPaperSprite;
class UTexture2D;

/** Low-level editor services for creating and saving Paper2D / Onyx2D assets. No UI here. */
class FOnyxAssetFactory
{
public:
	/** Nearest filtering, no mips, uncompressed: keeps pixel art crisp. */
	static void ConfigurePixelArtTexture(UTexture2D* Texture);

	/** Creates (or reuses) an asset of type T at Folder/Name. Returns null if another type already uses that name. */
	template <class T>
	static T* CreateOrReuse(const FString& Folder, const FString& Name)
	{
		return Cast<T>(CreateOrReuseAsset(T::StaticClass(), Folder, Name));
	}

	/** Imports an image file (PNG...) as a pixel-art texture at Folder/Name, replacing an existing one. */
	static UTexture2D* ImportTexture(const FString& File, const FString& Folder, const FString& Name);

	/** Creates (or overwrites) a pixel-art texture at Folder/Name from BGRA pixels, rows top to bottom. */
	static UTexture2D* CreateTexture(const FString& Folder, const FString& Name, const TArray<FColor>& Pixels, FIntPoint Size);

	static UPaperSprite* CreateSprite(const FString& Folder, const FString& Name, UTexture2D* Texture, FIntPoint Offset, FIntPoint Size,
		float PixelsPerUnit, ESpritePivotMode::Type Pivot);

	static UPaperFlipbook* CreateFlipbook(const FString& Folder, const FString& Name, const TArray<UPaperSprite*>& Frames, float FramesPerSecond);

	/** Creates (or reuses) a Blueprint subclass of ParentClass. */
	static UBlueprint* CreateBlueprint(const FString& Folder, const FString& Name, UClass* ParentClass);

	/** Saves every dirty package in the list. */
	static bool SavePackages(const TArray<UPackage*>& Packages);

	/** Asset folders must live under /Game. */
	static bool IsValidContentFolder(const FString& Folder);

	/** Makes a string safe for an asset name. */
	static FString SanitizeName(const FString& Name);

private:
	static UObject* CreateOrReuseAsset(UClass* Class, const FString& Folder, const FString& Name);
};
