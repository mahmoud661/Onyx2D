// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "OnyxExamplesCommandlet.generated.h"

/**
 * Headless builds for Onyx2D content, no scripting needed:
 *   UnrealEditor-Cmd.exe <Project>.uproject -run=OnyxExamples                 (plugin materials + Sunny Land)
 *   UnrealEditor-Cmd.exe <Project>.uproject -run=OnyxExamples -materials      (rebuild the plugin materials only)
 */
UCLASS()
class UOnyxExamplesCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UOnyxExamplesCommandlet();
	virtual int32 Main(const FString& Params) override;
};
