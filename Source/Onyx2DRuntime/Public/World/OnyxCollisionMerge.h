// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Grid -> rectangle merging (the idea behind Unity's Composite Collider 2D):
 * adjacent cells become one shape, so there are few bodies and no seams for characters to snag on.
 * Rectangles are in cell coordinates, Min inclusive, Max exclusive.
 */
struct ONYX2DRUNTIME_API FOnyxCollisionMerge
{
	/** Greedy merge into as few rectangles as practical (rows first, then grown downwards). */
	static TArray<FIntRect> MergeRectangles(const TArray<bool>& Cells, int32 Width, int32 Height);

	/** Horizontal runs only (one-way platforms must stay one cell thick so their top is well defined). */
	static TArray<FIntRect> MergeRows(const TArray<bool>& Cells, int32 Width, int32 Height);
};
