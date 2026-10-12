// Copyright Onyx2D. All Rights Reserved.

#include "AssetTools/OnyxMovesetAssets.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxMoveset.h"
#include "OnyxEditorTools.h"

namespace
{
	const FAssetCategoryPath OnyxMovesetCategories[] = { FAssetCategoryPath(INVTEXT("Onyx2D")) };
}

UOnyxMovesetFactory::UOnyxMovesetFactory()
{
	SupportedClass = UOnyxMoveset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UOnyxMovesetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UOnyxMoveset>(InParent, Class, Name, Flags | RF_Transactional);
}

UOnyxClipFactory::UOnyxClipFactory()
{
	SupportedClass = UOnyxClip::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UOnyxClipFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UOnyxClip>(InParent, Class, Name, Flags | RF_Transactional);
}

TSoftClassPtr<UObject> UAssetDefinition_OnyxMoveset::GetAssetClass() const
{
	return UOnyxMoveset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_OnyxMoveset::GetAssetCategories() const
{
	return OnyxMovesetCategories;
}

EAssetCommandResult UAssetDefinition_OnyxMoveset::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UOnyxMoveset* Moveset : OpenArgs.LoadObjects<UOnyxMoveset>())
	{
		OnyxEditorTools::OpenMovesetEditor(Moveset);
	}
	return EAssetCommandResult::Handled;
}

TSoftClassPtr<UObject> UAssetDefinition_OnyxClip::GetAssetClass() const
{
	return UOnyxClip::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_OnyxClip::GetAssetCategories() const
{
	return OnyxMovesetCategories;
}

EAssetCommandResult UAssetDefinition_OnyxClip::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UOnyxClip* Clip : OpenArgs.LoadObjects<UOnyxClip>())
	{
		OnyxEditorTools::OpenMovesetEditor(Clip);
	}
	return EAssetCommandResult::Handled;
}
