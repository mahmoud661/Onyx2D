// Copyright Onyx2D. All Rights Reserved.

#include "Commandlets/OnyxExamplesCommandlet.h"
#include "Builders/OnyxMaterialBuilder.h"
#include "Examples/OnyxStarterGame.h"
#include "Examples/OnyxSunnyLandExample.h"
#include "Examples/OnyxWaterExample.h"
#include "Examples/OnyxLearnExample.h"

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
	// -starter: only the starter game (-makestart also makes it the project's start).
	const bool bMakeStart = FParse::Param(*Params, TEXT("makestart"));
	if (FParse::Param(*Params, TEXT("starter")))
	{
		if (!FOnyxStarterGame::Build(bMakeStart, Error))
		{
			UE_LOG(LogOnyxExamples, Error, TEXT("Starter game: %s"), *Error.ToString());
			return 1;
		}
		UE_LOG(LogOnyxExamples, Display, TEXT("Starter game built: %s"), FOnyxStarterGame::LevelPath);
		return 0;
	}
	// -learn: only what the docs' Learn Unreal pages build (needs Sunny Land).
	if (FParse::Param(*Params, TEXT("learn")))
	{
		if (!FOnyxLearnExample::Build(Error))
		{
			UE_LOG(LogOnyxExamples, Error, TEXT("Learn Unreal: %s"), *Error.ToString());
			return 1;
		}
		UE_LOG(LogOnyxExamples, Display, TEXT("Learn Unreal built: %s"), FOnyxLearnExample::LevelPath);
		return 0;
	}
	if (!FOnyxSunnyLandExample::Build(Error))
	{
		UE_LOG(LogOnyxExamples, Error, TEXT("Sunny Land: %s"), *Error.ToString());
		return 1;
	}
	UE_LOG(LogOnyxExamples, Display, TEXT("Sunny Land built: %s"), FOnyxSunnyLandExample::LevelPath);
	if (!FOnyxWaterExample::Build(Error))
	{
		UE_LOG(LogOnyxExamples, Error, TEXT("Water test: %s"), *Error.ToString());
		return 1;
	}
	UE_LOG(LogOnyxExamples, Display, TEXT("Water test built: %s"), FOnyxWaterExample::LevelPath);
	if (!FOnyxStarterGame::Build(bMakeStart, Error))
	{
		UE_LOG(LogOnyxExamples, Error, TEXT("Starter game: %s"), *Error.ToString());
		return 1;
	}
	UE_LOG(LogOnyxExamples, Display, TEXT("Starter game built: %s"), FOnyxStarterGame::LevelPath);
	if (!FOnyxLearnExample::Build(Error))
	{
		UE_LOG(LogOnyxExamples, Error, TEXT("Learn Unreal: %s"), *Error.ToString());
		return 1;
	}
	UE_LOG(LogOnyxExamples, Display, TEXT("Learn Unreal built: %s"), FOnyxLearnExample::LevelPath);
	return 0;
}
