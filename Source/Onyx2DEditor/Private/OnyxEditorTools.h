// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Entry points into the Onyx2D tools window for code outside it (asset editors, menus). */
namespace OnyxEditorTools
{
	/** Opens the Moveset Editor on a Moveset, a Clip or a character Blueprint. */
	void OpenMovesetEditor(UObject* Asset);
}
