// Copyright Onyx2D. All Rights Reserved.

#include "Tools/OnyxToolOptions.h"
#include "Assets/OnyxCharacterBuilder.h"
#include "Assets/OnyxTileBuilder.h"
#include "Engine/Texture2D.h"
#include "PaperTileSet.h"

UOnyxCharacterWizardOptions::UOnyxCharacterWizardOptions()
{
	Folder.Path = TEXT("/Game/Characters");
	ResetAnimations();
}

void UOnyxCharacterWizardOptions::ResetAnimations()
{
	Animations.Reset();
	for (const FOnyxAnimationSpec& Spec : FOnyxCharacterSpec::DefaultAnimations())
	{
		FOnyxWizardAnimation& Row = Animations.AddDefaulted_GetRef();
		Row.Name = Spec.Name;
		Row.bEnabled = Spec.bEnabled;
		Row.Row = Spec.Row;
		Row.FramesPerSecond = Spec.FramesPerSecond;
		Row.bLoop = Spec.bLoop;
	}
}

FOnyxCharacterSpec UOnyxCharacterWizardOptions::ToSpec() const
{
	FOnyxCharacterSpec Spec;
	Spec.Name = Name;
	Spec.Folder = Folder.Path;
	Spec.Kind = Type == EOnyxCharacterType::Player ? EOnyxCharacterKind::Player : EOnyxCharacterKind::Enemy;
	Spec.SpriteSheet = SpriteSheet;
	Spec.FrameSize = FrameSize;
	Spec.CentimetersPerPixel = CentimetersPerPixel;
	Spec.PixelsBelowFeet = PixelsBelowFeet;
	Spec.bAutoFitCapsule = bAutoFitCapsule;
	Spec.bGenerateNormalMap = bGenerateNormalMap;
	Spec.NormalMap = NormalMap;
	for (const FOnyxWizardAnimation& Row : Animations)
	{
		FOnyxAnimationSpec& Animation = Spec.Animations.AddDefaulted_GetRef();
		Animation.Name = Row.Name;
		Animation.bEnabled = Row.bEnabled;
		Animation.TextureOverride = Row.TextureOverride;
		Animation.Row = Row.Row;
		Animation.StartColumn = Row.StartColumn;
		Animation.FrameCount = Row.FrameCount;
		Animation.FramesPerSecond = Row.FramesPerSecond;
		Animation.bLoop = Row.bLoop;
	}
	return Spec;
}

UOnyxTileOptions::UOnyxTileOptions()
{
	Folder.Path = TEXT("/Game/Tiles");
}

FOnyxTileSetSpec UOnyxTileOptions::ToTileSetSpec() const
{
	FOnyxTileSetSpec Spec;
	Spec.Texture = TileSheet;
	Spec.TileSize = TileSize;
	Spec.Margin = Margin;
	Spec.Spacing = Spacing;
	Spec.bSolidCollision = bSolidCollision;
	Spec.bGenerateNormalMap = bGenerateNormalMap;
	Spec.NormalMap = NormalMap;
	Spec.bPadTiles = bPadTiles;
	Spec.Name = TileSetName;
	Spec.Folder = Folder.Path;
	return Spec;
}

FOnyxTileMapSpec UOnyxTileOptions::ToTileMapSpec() const
{
	FOnyxTileMapSpec Spec;
	Spec.TileSet = TileSet;
	Spec.Width = MapSize.X;
	Spec.Height = MapSize.Y;
	Spec.CentimetersPerPixel = CentimetersPerPixel;
	Spec.bFillGround = bFillGround;
	Spec.GroundRows = GroundRows;
	Spec.GroundTile = GroundTile;
	Spec.bPlaceInLevel = bPlaceInLevel;
	Spec.Name = TileMapName;
	Spec.Folder = Folder.Path;
	return Spec;
}
