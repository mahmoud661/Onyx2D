// Copyright Onyx2D. All Rights Reserved.

#include "Style/OnyxEditorStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Textures/SlateIcon.h"

TSharedPtr<FSlateStyleSet> FOnyxEditorStyle::StyleSet;

namespace
{
	const FVector2D Icon16(16.f, 16.f);
	const FVector2D Icon20(20.f, 20.f);
	const FVector2D Icon64(64.f, 64.f);

	/** Onyx actor classes and the SVG used for their class icon / thumbnail. */
	const TPair<const TCHAR*, const TCHAR*> ClassIcons[] = {
		{ TEXT("OnyxCharacter"), TEXT("Character") },
		{ TEXT("OnyxLevelAudio"), TEXT("Audio") },
		{ TEXT("OnyxLightSwitch"), TEXT("Switch") },
		{ TEXT("OnyxPlayerControlComponent"), TEXT("Character") },
		{ TEXT("OnyxPatrolComponent"), TEXT("Enemy") },
		{ TEXT("OnyxStompableComponent"), TEXT("Enemy") },
		{ TEXT("OnyxContactDamageComponent"), TEXT("Hazard") },
		{ TEXT("OnyxRespawnComponent"), TEXT("Checkpoint") },
		{ TEXT("OnyxPickup"), TEXT("Pickup") },
		{ TEXT("OnyxMovingPlatform"), TEXT("MovingPlatform") },
		{ TEXT("OnyxLevelExit"), TEXT("LevelExit") },
		{ TEXT("OnyxCheckpoint"), TEXT("Checkpoint") },
		{ TEXT("OnyxOneWayPlatform"), TEXT("OneWayPlatform") },
		{ TEXT("OnyxHazardVolume"), TEXT("Hazard") },
		{ TEXT("OnyxWater"), TEXT("Water") },
		{ TEXT("OnyxLight2D"), TEXT("Light2D") },
		{ TEXT("OnyxLightingEnvironment"), TEXT("Lighting") },
		{ TEXT("OnyxCameraBounds"), TEXT("CameraBounds") },
		{ TEXT("OnyxLevelBuilder"), TEXT("LevelBuilder") },
		{ TEXT("OnyxParallaxBackground"), TEXT("ParallaxBackground") },
	};

	/** Tool icons: style name suffix -> SVG. */
	const TPair<const TCHAR*, const TCHAR*> ToolIcons[] = {
		{ TEXT("Onyx2D.Icon"), TEXT("Onyx2D") },
		{ TEXT("Onyx2D.Tab.Home"), TEXT("Home") },
		{ TEXT("Onyx2D.Tab.Import"), TEXT("Import") },
		{ TEXT("Onyx2D.Tab.Library"), TEXT("Library") },
		{ TEXT("Onyx2D.Tab.Character"), TEXT("Character") },
		{ TEXT("Onyx2D.Tab.Animations"), TEXT("Animation") },
		{ TEXT("Onyx2D.Tab.Tiles"), TEXT("Tiles") },
		{ TEXT("Onyx2D.Tab.Parallax"), TEXT("Parallax") },
		{ TEXT("Onyx2D.Tab.Level"), TEXT("Level") },
		{ TEXT("Onyx2D.Validate"), TEXT("Validate") },
		{ TEXT("Onyx2D.Light"), TEXT("Light2D") },
		{ TEXT("Onyx2D.Lighting"), TEXT("Lighting") },
		{ TEXT("Onyx2D.Project"), TEXT("Project") },
		{ TEXT("Onyx2D.DetectGrid"), TEXT("DetectGrid") },
		{ TEXT("Onyx2D.Tab.Painter"), TEXT("Painter") },
		{ TEXT("Onyx2D.Tool.Brush"), TEXT("ToolBrush") },
		{ TEXT("Onyx2D.Tool.Rect"), TEXT("ToolRect") },
		{ TEXT("Onyx2D.Tool.Fill"), TEXT("ToolFill") },
		{ TEXT("Onyx2D.Tool.Erase"), TEXT("ToolErase") },
		{ TEXT("Onyx2D.Tool.Pick"), TEXT("ToolPick") },
		{ TEXT("Onyx2D.Tool.Select"), TEXT("ToolSelect") },
		{ TEXT("Onyx2D.Tool.Grid"), TEXT("ToolGrid") },
		{ TEXT("Onyx2D.Tool.Fit"), TEXT("ToolFit") },
		{ TEXT("Onyx2D.Eye"), TEXT("Eye") },
		{ TEXT("Onyx2D.Collision"), TEXT("Collision") },
		{ TEXT("Onyx2D.Entity.Health"), TEXT("Heart") },
		{ TEXT("Onyx2D.EyeOff"), TEXT("EyeOff") },
		{ TEXT("Onyx2D.Layer.Tiles"), TEXT("LayerTiles") },
		{ TEXT("Onyx2D.Layer.Entities"), TEXT("LayerEntities") },
		{ TEXT("Onyx2D.Timeline.Frames"), TEXT("Frames") },
		{ TEXT("Onyx2D.Event.Message"), TEXT("EventMessage") },
		{ TEXT("Onyx2D.Event.Sound"), TEXT("EventSound") },
		{ TEXT("Onyx2D.Event.Effect"), TEXT("EventEffect") },
		{ TEXT("Onyx2D.Event.Shake"), TEXT("EventShake") },
		{ TEXT("Onyx2D.Event.Light"), TEXT("EventLight") },
		{ TEXT("Onyx2D.Event.Flash"), TEXT("EventFlash") },
		{ TEXT("Onyx2D.Event.Custom"), TEXT("EventCustom") },
		{ TEXT("Onyx2D.Window.Hitbox"), TEXT("WindowHitbox") },
		{ TEXT("Onyx2D.Window.Invulnerable"), TEXT("WindowInvulnerable") },
		{ TEXT("Onyx2D.Window.LoopSound"), TEXT("WindowLoopSound") },
		{ TEXT("Onyx2D.Window.Custom"), TEXT("WindowCustom") },
	};
}

void FOnyxEditorStyle::Register()
{
	if (StyleSet.IsValid())
	{
		return;
	}
	StyleSet = MakeShared<FSlateStyleSet>(GetStyleSetName());
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Onyx2D"));
	StyleSet->SetContentRoot(Plugin.IsValid() ? Plugin->GetBaseDir() / TEXT("Resources/Icons") : FString());

	for (const TPair<const TCHAR*, const TCHAR*>& Entry : ToolIcons)
	{
		StyleSet->Set(Entry.Key, new FSlateVectorImageBrush(StyleSet->RootToContentDir(Entry.Value, TEXT(".svg")), Icon20));
		StyleSet->Set(*(FString(Entry.Key) + TEXT(".Small")), new FSlateVectorImageBrush(StyleSet->RootToContentDir(Entry.Value, TEXT(".svg")), Icon16));
	}
	for (const TPair<const TCHAR*, const TCHAR*>& Entry : ClassIcons)
	{
		const FString Path = StyleSet->RootToContentDir(Entry.Value, TEXT(".svg"));
		StyleSet->Set(*(FString(TEXT("ClassIcon.")) + Entry.Key), new FSlateVectorImageBrush(Path, Icon16));
		StyleSet->Set(*(FString(TEXT("ClassThumbnail.")) + Entry.Key), new FSlateVectorImageBrush(Path, Icon64));
	}
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FOnyxEditorStyle::Unregister()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}
}

FName FOnyxEditorStyle::GetStyleSetName()
{
	static const FName Name(TEXT("Onyx2DEditorStyle"));
	return Name;
}

const ISlateStyle& FOnyxEditorStyle::Get()
{
	check(StyleSet.IsValid());
	return *StyleSet;
}

FSlateIcon FOnyxEditorStyle::Icon(FName Name)
{
	return FSlateIcon(GetStyleSetName(), Name, *(Name.ToString() + TEXT(".Small")));
}
