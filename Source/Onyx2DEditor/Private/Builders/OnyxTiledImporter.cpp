// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxTiledImporter.h"
#include "Builders/OnyxAssetUtils.h"
#include "Builders/OnyxTileBuilder.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Misc/FileHelper.h"
#include "PaperTileLayer.h"
#include "PaperTileMap.h"
#include "PaperTileMapActor.h"
#include "PaperTileMapComponent.h"
#include "PaperTileSet.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "World/OnyxTileCollisionComponent.h"

namespace
{
	/** Tiled stores flips in the top three bits of a tile id, exactly like Paper2D's EPaperTileFlags. */
	constexpr uint32 TiledFlagMask = 0xE0000000u;

	struct FTiledLayer
	{
		FString Name;
		bool bVisible = true;
		TArray<uint32> Data;
	};

	bool ReadLayers(const TSharedPtr<FJsonObject>& Root, int32 Width, int32 Height, TArray<FTiledLayer>& OutLayers, FText& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Layers = nullptr;
		if (!Root->TryGetArrayField(TEXT("layers"), Layers))
		{
			OutError = INVTEXT("The file has no layers.");
			return false;
		}
		// Groups are flattened in order.
		TArray<TSharedPtr<FJsonValue>> Pending = *Layers;
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> Layer = Pending[Index]->AsObject();
			const FString Type = Layer->GetStringField(TEXT("type"));
			if (Type == TEXT("group"))
			{
				const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
				if (Layer->TryGetArrayField(TEXT("layers"), Children))
				{
					Pending.Insert(*Children, Index + 1);
				}
				continue;
			}
			const TArray<TSharedPtr<FJsonValue>>* Data = nullptr;
			if (Type != TEXT("tilelayer") || !Layer->TryGetArrayField(TEXT("data"), Data))
			{
				continue; // object / image layers and compressed data are not tiles we can read
			}
			if (Data->Num() != Width * Height)
			{
				OutError = FText::Format(INVTEXT("Layer \"{0}\" is not {1} x {2} (infinite maps and base64 data aren't supported: save as CSV/JSON)."),
					FText::FromString(Layer->GetStringField(TEXT("name"))), Width, Height);
				return false;
			}
			FTiledLayer& Out = OutLayers.AddDefaulted_GetRef();
			Out.Name = Layer->GetStringField(TEXT("name"));
			Layer->TryGetBoolField(TEXT("visible"), Out.bVisible);
			Out.Data.Reserve(Data->Num());
			for (const TSharedPtr<FJsonValue>& Value : *Data)
			{
				Out.Data.Add(uint32(Value->AsNumber()));
			}
		}
		if (OutLayers.IsEmpty())
		{
			OutError = INVTEXT("The file has no tile layers.");
			return false;
		}
		return true;
	}
}

UPaperTileMap* FOnyxTiledImporter::Import(const FOnyxTiledImportSpec& Spec, FText& OutError)
{
	UPaperTileSet* TileSet = Spec.TileSet.Get();
	FString Text;
	if (!TileSet)
	{
		OutError = INVTEXT("Pick the tile set made from the map's tile sheet.");
		return nullptr;
	}
	if (!FFileHelper::LoadFileToString(Text, *Spec.FilePath))
	{
		OutError = FText::Format(INVTEXT("Could not read {0}."), FText::FromString(Spec.FilePath));
		return nullptr;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		OutError = INVTEXT("Not a Tiled JSON map (export as .json / .tmj).");
		return nullptr;
	}
	if (Root->GetStringField(TEXT("orientation")) != TEXT("orthogonal"))
	{
		OutError = INVTEXT("Only orthogonal Tiled maps can be imported.");
		return nullptr;
	}
	const int32 Width = Root->GetIntegerField(TEXT("width"));
	const int32 Height = Root->GetIntegerField(TEXT("height"));
	const FIntPoint TiledTileSize(Root->GetIntegerField(TEXT("tilewidth")), Root->GetIntegerField(TEXT("tileheight")));
	if (TiledTileSize != TileSet->GetTileSize())
	{
		OutError = FText::Format(INVTEXT("The map uses {0} x {1} tiles but the tile set uses {2} x {3}."),
			TiledTileSize.X, TiledTileSize.Y, TileSet->GetTileSize().X, TileSet->GetTileSize().Y);
		return nullptr;
	}
	int32 FirstGid = 1;
	const TArray<TSharedPtr<FJsonValue>>* TileSets = nullptr;
	if (Root->TryGetArrayField(TEXT("tilesets"), TileSets) && TileSets->Num() > 0)
	{
		FirstGid = (*TileSets)[0]->AsObject()->GetIntegerField(TEXT("firstgid"));
	}

	TArray<FTiledLayer> Layers;
	if (!ReadLayers(Root, Width, Height, Layers, OutError))
	{
		return nullptr;
	}

	FOnyxTileMapSpec MapSpec;
	MapSpec.TileSet = TileSet;
	MapSpec.Width = Width;
	MapSpec.Height = Height;
	MapSpec.CentimetersPerPixel = Spec.CentimetersPerPixel;
	MapSpec.bFillGround = false;
	MapSpec.GroundRows = 0;
	MapSpec.bPlaceInLevel = Spec.bPlaceInLevel;
	MapSpec.Name = Spec.Name;
	MapSpec.Folder = Spec.Folder;
	UPaperTileMap* TileMap = FOnyxTileBuilder::CreateTileMap(MapSpec, OutError);
	if (!TileMap)
	{
		return nullptr;
	}

	// Tiled draws its first layer at the back; Paper2D layer 0 is the front.
	TileMap->Modify();
	TileMap->TileLayers.Reset();
	const int32 TileCount = TileSet->GetTileCount();
	for (int32 LayerIndex = Layers.Num() - 1; LayerIndex >= 0; --LayerIndex)
	{
		const FTiledLayer& Source = Layers[LayerIndex];
		UPaperTileLayer* Layer = TileMap->AddNewLayer();
		Layer->LayerName = FText::FromString(Source.Name);
		Layer->SetShouldRenderInEditor(Source.bVisible);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const uint32 Gid = Source.Data[Y * Width + X];
				const int32 Index = int32(Gid & ~TiledFlagMask) - FirstGid;
				if ((Gid & ~TiledFlagMask) == 0 || Index < 0 || Index >= TileCount)
				{
					continue; // empty, or from another Tiled tile set
				}
				FPaperTileInfo Tile;
				Tile.TileSet = TileSet;
				Tile.PackedTileIndex = int32(uint32(Index) | (Gid & TiledFlagMask));
				Layer->SetCell(X, Y, Tile);
			}
		}
	}
	TileMap->PostEditChange();
	TileMap->MarkPackageDirty();
	FOnyxAssetUtils::SavePackages({ TileMap->GetPackage() });

	// The placed actor's collision was built from the empty map: rebuild it from the imported tiles.
	if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
	{
		for (TActorIterator<APaperTileMapActor> It(World); It; ++It)
		{
			if (It->GetRenderComponent()->TileMap == TileMap)
			{
				if (UOnyxTileCollisionComponent* Collision = It->FindComponentByClass<UOnyxTileCollisionComponent>())
				{
					Collision->Rebuild();
				}
			}
		}
	}
	return TileMap;
}
