// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxAssetUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Character/OnyxBehaviourLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "UObject/Package.h"

void FOnyxAssetUtils::ConfigurePixelArtTexture(UTexture2D* Texture)
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

UTexture2D* FOnyxAssetUtils::ImportTexture(const FString& File, const FString& Folder, const FString& Name)
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

UTexture2D* FOnyxAssetUtils::CreateTexture(const FString& Folder, const FString& Name, const TArray<FColor>& Pixels, FIntPoint Size)
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

bool FOnyxAssetUtils::IsValidContentFolder(const FString& Folder)
{
	return Folder.StartsWith(TEXT("/Game"));
}

FString FOnyxAssetUtils::SanitizeName(const FString& Name)
{
	FString Result;
	for (const TCHAR C : Name)
	{
		Result.AppendChar(FChar::IsAlnum(C) || C == TEXT('_') ? C : TEXT('_'));
	}
	return Result.IsEmpty() ? TEXT("Unnamed") : Result;
}

UObject* FOnyxAssetUtils::CreateOrReuseAsset(UClass* Class, const FString& Folder, const FString& Name)
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

UPaperSprite* FOnyxAssetUtils::CreateSprite(const FString& Folder, const FString& Name, UTexture2D* Texture, FIntPoint Offset, FIntPoint Size,
	float PixelsPerUnit, ESpritePivotMode::Type Pivot, UMaterialInterface* Material, const FVector2D& CustomPivot)
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
	Params.DefaultMaterialOverride = Material;
	Sprite->InitializeSprite(Params, false);
	Sprite->SetPivotMode(Pivot, CustomPivot, false);

	// Character frames should not carry their own collision; the capsule handles that.
	if (FByteProperty* CollisionProperty = FindFProperty<FByteProperty>(UPaperSprite::StaticClass(), TEXT("SpriteCollisionDomain")))
	{
		CollisionProperty->SetPropertyValue_InContainer(Sprite, uint8(ESpriteCollisionMode::None));
	}
	Sprite->PostEditChange();
	Sprite->MarkPackageDirty();
	return Sprite;
}

bool FOnyxAssetUtils::SetSpriteSocket(UPaperSprite* Sprite, FName Name, const FVector2D& Pixel)
{
	// Paper2D keeps a sprite's sockets to its own editor: reach them through their property.
	FArrayProperty* Property = Sprite && !Name.IsNone() ? FindFProperty<FArrayProperty>(UPaperSprite::StaticClass(), TEXT("Sockets")) : nullptr;
	if (!Property)
	{
		return false;
	}
	TArray<FPaperSpriteSocket>& Sockets = *Property->ContainerPtrToValuePtr<TArray<FPaperSpriteSocket>>(Sprite);
	// Sockets live in pivot space: pixels from the pivot, X right and up the second axis (Paper2D's X and Z).
	const FVector2D Pivoted = Sprite->ConvertTextureSpaceToPivotSpace(Sprite->GetSourceUV() + Pixel);
	Sprite->Modify();
	FPaperSpriteSocket* Socket = Sockets.FindByPredicate([Name](const FPaperSpriteSocket& Each) { return Each.SocketName == Name; });
	if (!Socket)
	{
		Socket = &Sockets.AddDefaulted_GetRef();
		Socket->SocketName = Name;
	}
	Socket->LocalTransform = FTransform(FVector(Pivoted.X, 0.f, Pivoted.Y));
	Sprite->MarkPackageDirty();
	return true;
}

UPaperFlipbook* FOnyxAssetUtils::CreateFlipbook(const FString& Folder, const FString& Name, const TArray<UPaperSprite*>& Frames, float FramesPerSecond)
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

UBlueprint* FOnyxAssetUtils::CreateBlueprint(const FString& Folder, const FString& Name, UClass* ParentClass)
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

UActorComponent* FOnyxAssetUtils::FindComponentTemplate(UBlueprint* Blueprint, UClass* Class)
{
	if (!Blueprint || !Class)
	{
		return nullptr;
	}
	if (Blueprint->SimpleConstructionScript)
	{
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA(Class))
			{
				return Node->ComponentTemplate;
			}
		}
	}
	const AActor* Defaults = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject<AActor>() : nullptr;
	return Defaults ? Defaults->FindComponentByClass(Class) : nullptr;
}

TArray<UActorComponent*> FOnyxAssetUtils::AddBehaviourComponents(UBlueprint* Blueprint, int32 Behaviours)
{
	TArray<UActorComponent*> Templates;
	USimpleConstructionScript* Construction = Blueprint ? Blueprint->SimpleConstructionScript.Get() : nullptr;
	const AActor* Defaults = Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject<AActor>() : nullptr;
	if (!Construction || !Defaults)
	{
		return Templates;
	}
	bool bAdded = false;
	for (const EOnyxBehaviour Behaviour : UOnyxBehaviourLibrary::GetAll())
	{
		UClass* Class = UOnyxBehaviourLibrary::GetComponentClass(Behaviour);
		if (!(Behaviours & int32(Behaviour)) || !Class)
		{
			continue;
		}
		if (UActorComponent* Existing = FindComponentTemplate(Blueprint, Class))
		{
			Templates.Add(Existing);
			continue;
		}
		USCS_Node* Node = Construction->CreateNode(Class, Class->GetFName());
		if (Class->IsChildOf(USceneComponent::StaticClass()) && Defaults->GetRootComponent())
		{
			Node->SetParent(Defaults->GetRootComponent());
		}
		Construction->AddNode(Node);
		Templates.Add(Node->ComponentTemplate);
		bAdded = true;
	}
	if (bAdded)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}
	return Templates;
}

bool FOnyxAssetUtils::SavePackages(const TArray<UPackage*>& Packages)
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
