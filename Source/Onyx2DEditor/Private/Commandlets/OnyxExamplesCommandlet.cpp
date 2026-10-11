// Copyright Onyx2D. All Rights Reserved.

#include "Commandlets/OnyxExamplesCommandlet.h"
#include "Assets/OnyxMaterialBuilder.h"
#include "Examples/OnyxSunnyLandExample.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxExamples, Log, All);

UOnyxExamplesCommandlet::UOnyxExamplesCommandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UOnyxExamplesCommandlet::Main(const FString& Params)
{
	const bool bMaterialsOnly = FParse::Param(*Params, TEXT("materials"));
	for (const FString& Built : FOnyxMaterialBuilder::EnsureMaterials(bMaterialsOnly))
	{
		UE_LOG(LogOnyxExamples, Display, TEXT("Built %s"), *Built);
	}
	if (bMaterialsOnly)
	{
		return 0;
	}
	FText Error;
	if (!FOnyxSunnyLandExample::Build(Error))
	{
		UE_LOG(LogOnyxExamples, Error, TEXT("Sunny Land: %s"), *Error.ToString());
		return 1;
	}
	UE_LOG(LogOnyxExamples, Display, TEXT("Sunny Land built: %s"), FOnyxSunnyLandExample::LevelPath);
	return 0;
}
