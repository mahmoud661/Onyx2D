// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

/**
 * Feeds the active level viewport's camera to the editor world's Onyx lighting every frame, so 2D lights,
 * shadows and moods show while editing exactly as they will in game.
 */
class FOnyxEditorLightingView
{
public:
	void Start();
	void Stop();

private:
	bool Tick(float DeltaTime);

	FTSTicker::FDelegateHandle TickerHandle;
};
