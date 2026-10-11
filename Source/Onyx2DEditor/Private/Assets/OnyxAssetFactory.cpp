// Copyright Onyx2D. All Rights Reserved.

#include "Assets/OnyxAssetFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "UObject/Package.h"

void FOnyxAssetFactory::ConfigurePixelArtTexture(UTexture2D* Texture)
{
	if (!Texture)
	{
		return;
	}
	if (Texture->Filter == TF_Nearest && Texture->MipGenSettings == TMGS_NoMipmaps && Texture->LODGroup == TEXTUREGROUP_Pixels2D)
	{
		return;
	}
	Texture->Modify();
	Texture->Filter = TF_Nearest;
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->LODGroup = TEXTUREGROUP_Pixels2D;
	Texture->CompressionSettings = TC_EditorIcon;
	Texture->NeverStream = true;
	Texture->PostEditChange();
	Texture->MarkPackageDirty();
}

UTexture2D* FOnyxAssetFactory::ImportTexture(const FString& File, const FString& Folder, const FString& Name)
{
	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = File;
	Task->DestinationPath = Folder;
	Task->DestinationName = SanitizeName(Name);
	Task->bAutomated = true;
	Task->bReplaceExisting = true;
	Task->bSave = false;
	FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().ImportAssetTasks({ Task });
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *(Folder / Task->DestinationName + TEXT(".") + Task->DestinationName));
	ConfigurePixelArtTexture(Texture);
	return Texture;
}

UTexture2D* FOnyxAssetFactory::CreateTexture(const FString& Folder, const FString& Name, const TArray<FColor>& Pixels, FIntPoint Size)
{
	UTexture2D* Texture = Size.X > 0 && Size.Y > 0 && Pixels.Num() == Size.X * Size.Y ? CreateOrReuse<UTexture2D>(Folder, SanitizeName(Name)) : nullptr;
	if (!Texture)
	{
		return nullptr;
	}
	Texture->Modify();
	Texture->PreEditChange(nullptr);
	Texture->Source.Init(Size.X, Size.Y, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
	ConfigurePixelArtTexture(Texture);
	Texture->PostEditChange();
	Texture->MarkPackageDirty();
	return Texture;
}

bool FOnyxAssetFactory::IsValidContentFolder(const FString& Folder)
{
	return Folder.StartsWith(TEXT("/Game"));
}

FString FOnyxAssetFactory::SanitizeName(const FString& Name)
{
	FString Result;
	for (const TCHAR C : Name)
	{
		Result.AppendChar(FChar::IsAlnum(C) || C == TEXT('_') ? C : TEXT('_'));
	}
	return Result.IsEmpty() ? TEXT("Unnamed") : Result;
}

UObject* FOnyxAssetFactory::CreateOrReuseAsset(UClass* Class, const FString& Folder, const FString& Name)
{
	const FString PackageName = Folder / Name;
	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();

	if (UObject* Existing = StaticFindObject(UObject::StaticClass(), Package, *Name))
	{
		return Existing->IsA(Class) ? Existing : nullptr;
	}

	UObject* Asset = NewObject<UObject>(Package, Class, *Name, RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Asset);
	Package->MarkPackageDirty();
	return Asset;
}

UPaperSprite* FOnyxAssetFactory::CreateSprite(const FString& Folder, const FString& Name, UTexture2D* Texture, FIntPoint Offset, FIntPoint Size,
	float PixelsPerUnit, ESpritePivotMode::Type Pivot)
{
	UPaperSprite* Sprite = CreateOrReuse<UPaperSprite>(Folder, Name);
	if (!Sprite || !Texture)
	{
		return nullptr;
	}

	Sprite->Modify();
	FSpriteAssetInitParameters Params;
	Params.SetTextureAndFill(Texture);
	Params.Offset = Offset;
	Params.Dimension = Size;
	Params.SetPixelsPerUnrealUnit(PixelsPerUnit);
	Sprite->InitializeSprite(Params, false);
	Sprite->SetPivotMode(Pivot, FVector2D::ZeroVector, false);

	// Character frames should not carry their own collision; the capsule handles that.
	if (FByteProperty* CollisionProperty = FindFProperty<FByteProperty>(UPaperSprite::StaticClass(), TEXT("SpriteCollisionDomain")))
	{
		CollisionProperty->SetPropertyValue_InContainer(Sprite, uint8(ESpriteCollisionMode::None));
	}
	Sprite->PostEditChange();
	Sprite->MarkPackageDirty();
	return Sprite;
}

UPaperFlipbook* FOnyxAssetFactory::CreateFlipbook(const FString& Folder, const FString& Name, const TArray<UPaperSprite*>& Frames, float FramesPerSecond)
{
	UPaperFlipbook* Flipbook = CreateOrReuse<UPaperFlipbook>(Folder, Name);
	if (!Flipbook)
	{
		return nullptr;
	}

	Flipbook->Modify();
	{
		FScopedFlipbookMutator Editor(Flipbook);
		Editor.FramesPerSecond = FMath::Max(FramesPerSecond, 0.1f);
		Editor.KeyFrames.Reset();
		for (UPaperSprite* Sprite : Frames)
		{
			FPaperFlipbookKeyFrame KeyFrame;
			KeyFrame.Sprite = Sprite;
			KeyFrame.FrameRun = 1;
			Editor.KeyFrames.Add(KeyFrame);
		}
	}
	Flipbook->PostEditChange();
	Flipbook->MarkPackageDirty();
	return Flipbook;
}

UBlueprint* FOnyxAssetFactory::CreateBlueprint(const FString& Folder, const FString& Name, UClass* ParentClass)
{
	const FString PackageName = Folder / Name;
	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();

	if (UObject* Existing = StaticFindObject(UObject::StaticClass(), Package, *Name))
	{
		UBlueprint* ExistingBlueprint = Cast<UBlueprint>(Existing);
		return ExistingBlueprint && ExistingBlueprint->ParentClass && ExistingBlueprint->ParentClass->IsChildOf(ParentClass) ? ExistingBlueprint : nullptr;
	}

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, *Name, BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	if (Blueprint)
	{
		FAssetRegistryModule::AssetCreated(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		Package->MarkPackageDirty();
	}
	return Blueprint;
}

bool FOnyxAssetFactory::SavePackages(const TArray<UPackage*>& Packages)
{
	TArray<UPackage*> Unique;
	for (UPackage* Package : Packages)
	{
		if (Package)
		{
			Unique.AddUnique(Package);
		}
	}
	return Unique.IsEmpty() || UEditorLoadingAndSavingUtils::SavePackages(Unique, true);
}
