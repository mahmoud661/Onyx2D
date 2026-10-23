// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

/**
 * Draws a selected Onyx Light 2D in the level viewport, on the 2D plane: how far it reaches, its cone, the line or
 * rectangle it shines from with its soft border, and where its Light Animation sweeps or swings it.
 */
class FOnyxLightVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
};
