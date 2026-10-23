// Copyright Onyx2D. All Rights Reserved.

#include "Commandlets/OnyxMaterialsCommandlet.h"
#include "Builders/OnyxMaterialBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxMaterialsCommandlet, Log, All);

UOnyxMaterialsCommandlet::UOnyxMaterialsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UOnyxMaterialsCommandlet::Main(const FString& Params)
{
	const TArray<FString> Built = FOnyxMaterialBuilder::EnsureMaterials(!FParse::Param(*Params, TEXT("missing")));
	for (const FString& Material : Built)
	{
		UE_LOG(LogOnyxMaterialsCommandlet, Display, TEXT("Built %s"), *Material);
	}
	UE_LOG(LogOnyxMaterialsCommandlet, Display, TEXT("Onyx2D materials: %d built"), Built.Num());
	return 0;
}
