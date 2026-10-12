// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Builds the plugin's own materials in C++ (no scripts): lit sprite with normal maps, the 2D lighting composite,
 * parallax background, glow, light halo, the lighting parameter collection and the light-buffer placeholder.
 * Missing ones are created when the editor starts, so the plugin is self-contained.
 */
class FOnyxMaterialBuilder
{
public:
	/** Content folder of the plugin's materials. */
	static const TCHAR* Folder;

	/** Creates the materials that are missing (or all of them when bRebuild). Returns the names it built. */
	static TArray<FString> EnsureMaterials(bool bRebuild = false);

	/** True when every plugin material exists. */
	static bool AreMaterialsPresent();
};
