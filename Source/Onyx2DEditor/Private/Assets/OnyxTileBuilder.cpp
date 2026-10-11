// Copyright Onyx2D. All Rights Reserved.

#include "Assets/OnyxTileBuilder.h"
#include "Assets/OnyxTilePadding.h"
#include "Core/Onyx2DTypes.h"
#include "Assets/OnyxAssetFactory.h"
#include "Assets/OnyxImageAnalysis.h"
#include "World/OnyxTileCollision.h"
#include "World/OnyxTileCollisionComponent.h"
#include "Core/Onyx2DSettings.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "PaperTileLayer.h"
#include "PaperTileMap.h"
#include "PaperTileMapActor.h"
#include "PaperTileMapComponent.h"
#include "PaperTileSet.h"
#include "ScopedTransaction.h"

UPaperTileSet* FOnyxTileBuilder::CreateTileSet(const FOnyxTileSetSpec& Spec, FText& OutError)
{
	UTexture2D* Texture = Spec.Texture.Get();
	if (!Texture)
	{
		OutError = INVTEXT("Pick a tileset image first.");
		return nullptr;
	}
	if (Spec.TileSize.X <= 0 || Spec.TileSize.Y <= 0)
	{
		OutError = INVTEXT("Tile width and height must be greater than zero.");
		return nullptr;
	}
	if (!FOnyxAssetFactory::IsValidContentFolder(Spec.Folder))
	{
		OutError = INVTEXT("The folder must start with /Game.");
		return nullptr;
	}

	FOnyxAssetFactory::ConfigurePixelArtTexture(Texture);
	UPaperTileSet* TileSet = FOnyxAssetFactory::CreateOrReuse<UPaperTileSet>(Spec.Folder, FOnyxAssetFactory::SanitizeName(Spec.Name));
	if (!TileSet)
	{
		OutError = INVTEXT("That name is already used by a different kind of asset.");
		return nullptr;
	}

	TileSet->Modify();
	TileSet->SetTileSheetTexture(Texture);
	TileSet->SetTileSize(Spec.TileSize);
	TileSet->SetMargin(FIntMargin(Spec.Margin));
	TileSet->SetPerTileSpacing(FIntPoint(Spec.Spacing));

	if (Spec.bSolidCollision)
	{
		AutoAssignCollision(TileSet);
	}
	TileSet->PostEditChange();
	TileSet->MarkPackageDirty();
	TArray<UPackage*> Packages = { TileSet->GetPackage(), Texture->GetPackage() };
	if (Spec.bGenerateNormalMap)
	{
		if (UTexture2D* NormalMap = FOnyxNormalMaps::CreateFor(Texture, Spec.NormalMap, FOnyxNormalMaps::GridOf(TileSet)))
		{
			FOnyxNormalMaps::Assign(TileSet, NormalMap);
			Packages.Add(NormalMap->GetPackage());
		}
	}
	FOnyxAssetFactory::SavePackages(Packages);
	if (Spec.bPadTiles)
	{
		FOnyxTilePadding::PadTileSet(TileSet); // after the normal map, so it gets padded too
	}
	return TileSet;
}

UPaperTileMap* FOnyxTileBuilder::CreateTileMap(const FOnyxTileMapSpec& Spec, FText& OutError)
{
	UPaperTileSet* TileSet = Spec.TileSet.Get();
	if (!TileSet)
	{
		OutError = INVTEXT("Pick or create a tile set first.");
		return nullptr;
	}
	if (!FOnyxAssetFactory::IsValidContentFolder(Spec.Folder))
	{
		OutError = INVTEXT("The folder must start with /Game.");
		return nullptr;
	}

	UPaperTileMap* TileMap = FOnyxAssetFactory::CreateOrReuse<UPaperTileMap>(Spec.Folder, FOnyxAssetFactory::SanitizeName(Spec.Name));
	if (!TileMap)
	{
		OutError = INVTEXT("That name is already used by a different kind of asset.");
		return nullptr;
	}

	TileMap->Modify();
	TileMap->MapWidth = FMath::Max(Spec.Width, 1);
	TileMap->MapHeight = FMath::Max(Spec.Height, 1);
	TileMap->TileWidth = TileSet->GetTileSize().X;
	TileMap->TileHeight = TileSet->GetTileSize().Y;
	TileMap->PixelsPerUnrealUnit = 1.f / FMath::Max(Spec.CentimetersPerPixel, 0.01f);
	TileMap->SelectedTileSet = TileSet;
	if (FByteProperty* CollisionProperty = FindFProperty<FByteProperty>(UPaperTileMap::StaticClass(), TEXT("SpriteCollisionDomain")))
	{
		CollisionProperty->SetPropertyValue_InContainer(TileMap, uint8(ESpriteCollisionMode::Use3DPhysics));
	}
	TileMap->SetCollisionThickness(200.f);
	if (UMaterialInterface* LitMaterial = UOnyx2DSettings::Get()->LitSpriteMaterial.LoadSynchronous())
	{
		TileMap->Material = LitMaterial;
	}

	if (TileMap->TileLayers.IsEmpty())
	{
		UPaperTileLayer* Ground = TileMap->AddNewLayer();
		Ground->LayerName = INVTEXT("Ground");
		UPaperTileLayer* Decoration = TileMap->AddNewLayer();
		Decoration->LayerName = INVTEXT("Decoration");
		Decoration->SetLayerCollides(false);
	}
	else
	{
		TileMap->ResizeMap(TileMap->MapWidth, TileMap->MapHeight);
	}

	if (Spec.bFillGround && TileMap->TileLayers.Num() > 0)
	{
		// Paint the colliding "Ground" layer (index 0 for maps made here), not the decoration layer.
		const TObjectPtr<UPaperTileLayer>* Found = TileMap->TileLayers.FindByPredicate([](const TObjectPtr<UPaperTileLayer>& Layer) { return Layer && Layer->LayerName.ToString() == TEXT("Ground"); });
		UPaperTileLayer* Ground = Found ? Found->Get() : TileMap->TileLayers[0].Get();
		FPaperTileInfo Tile;
		Tile.TileSet = TileSet;
		Tile.PackedTileIndex = FMath::Clamp(Spec.GroundTile, 0, FMath::Max(TileSet->GetTileCount() - 1, 0));
		for (int32 Row = FMath::Max(TileMap->MapHeight - Spec.GroundRows, 0); Row < TileMap->MapHeight; ++Row)
		{
			for (int32 Column = 0; Column < TileMap->MapWidth; ++Column)
			{
				Ground->SetCell(Column, Row, Tile);
			}
		}
	}
	TileMap->PostEditChange();
	TileMap->MarkPackageDirty();
	FOnyxAssetFactory::SavePackages({ TileMap->GetPackage() });

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (Spec.bPlaceInLevel && World)
	{
		const FScopedTransaction Transaction(INVTEXT("Place Onyx Tile Map"));
		// Paper2D tile maps grow down and right from their origin; lift it so the ground sits at Z = 0.
		const float TileHeightWorld = TileMap->TileHeight * Spec.CentimetersPerPixel;
		// Paper2D centres tile (0, 0) on the actor: shift by half a tile so the map's corner is at X = 0 and the ground at Z = 0.
		const float TileWidthWorld = TileMap->TileWidth * Spec.CentimetersPerPixel;
		const FVector Origin(TileWidthWorld * 0.5f, Onyx2D::TileMapDepth, TileHeightWorld * (TileMap->MapHeight - Spec.GroundRows - 0.5f));
		if (APaperTileMapActor* Actor = Cast<APaperTileMapActor>(GEditor->AddActor(World->GetCurrentLevel(), APaperTileMapActor::StaticClass(), FTransform(Origin))))
		{
			Actor->SetActorLocation(Origin); // AddActor snaps to the editor grid, which would undo the depth
			Actor->GetRenderComponent()->SetTileMap(TileMap);
			Actor->SetActorLabel(TileMap->GetName());
			EnsureTileCollision(Actor);
		}
	}
	return TileMap;
}

int32 FOnyxTileBuilder::AutoAssignCollision(UPaperTileSet* TileSet, float SolidCoverage)
{
	UTexture2D* Texture = TileSet ? TileSet->GetTileSheetTexture() : nullptr;
	if (!Texture)
	{
		return 0;
	}
	TileSet->Modify();
	const FIntPoint TileSize = TileSet->GetTileSize();
	int32 Solid = 0;
	for (int32 Index = 0; Index < TileSet->GetTileCount(); ++Index)
	{
		FVector2D UV;
		if (!TileSet->GetTileUV(Index, UV))
		{
			continue;
		}
		const FIntPoint Min(FMath::RoundToInt(UV.X), FMath::RoundToInt(UV.Y));
		const float Coverage = FOnyxImageAnalysis::GetCoverage(Texture, FIntRect(Min, Min + TileSize));
		// Unreadable source: fall back to solid, like a classic "every tile collides" tile set.
		const bool bSolid = Coverage < 0.f || Coverage >= SolidCoverage;
		FOnyxTileCollision::Set(TileSet, Index, bSolid ? EOnyxTileCollision::Solid : EOnyxTileCollision::None);
		Solid += bSolid ? 1 : 0;
	}
	TileSet->PostEditChange();
	TileSet->MarkPackageDirty();
	return Solid;
}

void FOnyxTileBuilder::EnsureTileCollision(APaperTileMapActor* Actor)
{
	if (!Actor || Actor->FindComponentByClass<UOnyxTileCollisionComponent>())
	{
		return;
	}
	Actor->Modify();
	UOnyxTileCollisionComponent* Collision = NewObject<UOnyxTileCollisionComponent>(Actor, TEXT("OnyxTileCollision"), RF_Transactional);
	Collision->SetupAttachment(Actor->GetRootComponent());
	Actor->AddInstanceComponent(Collision);
	Collision->RegisterComponent();
}
