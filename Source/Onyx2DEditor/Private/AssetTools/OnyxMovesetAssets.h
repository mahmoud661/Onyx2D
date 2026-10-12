// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "Factories/Factory.h"
#include "OnyxMovesetAssets.generated.h"

/** Content Browser > Add > Onyx2D > Moveset. */
UCLASS()
class UOnyxMovesetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UOnyxMovesetFactory();
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};

/** Content Browser > Add > Onyx2D > Clip. */
UCLASS()
class UOnyxClipFactory : public UFactory
{
	GENERATED_BODY()

public:
	UOnyxClipFactory();
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};

/** Movesets in the Content Browser: Onyx2D category; double-click opens the Moveset Editor. */
UCLASS()
class UAssetDefinition_OnyxMoveset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override { return INVTEXT("Moveset"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(1.f, 0.76f, 0.2f); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};

/** Clips in the Content Browser: Onyx2D category; double-click opens their Moveset's editor on that clip. */
UCLASS()
class UAssetDefinition_OnyxClip : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override { return INVTEXT("Clip"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(0.22f, 0.71f, 1.f); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
