// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "OnyxMaterialsCommandlet.generated.h"

/**
 * Rebuilds Onyx2D's materials from C++, headless (after a plugin update changes one, or when one is missing):
 *   UnrealEditor-Cmd.exe <Project>.uproject -run=OnyxMaterials          (rebuild every plugin material)
 *   UnrealEditor-Cmd.exe <Project>.uproject -run=OnyxMaterials -missing (only make the ones that are missing or outdated)
 */
UCLASS()
class UOnyxMaterialsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UOnyxMaterialsCommandlet();
	virtual int32 Main(const FString& Params) override;
};
