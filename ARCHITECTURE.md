# Onyx2D architecture

This file says what the main classes are, what each one is called and why, and where its source lives.
Read it before adding a class.

## The main classes

| What | Class | Name in the editor |
|---|---|---|
| A character | `AOnyxCharacter` | Onyx Character |
| How it moves | `UOnyxMovementComponent` | Onyx Movement |
| Plays its animations on the sprite | `UOnyxMovesetComponent` | Onyx Moveset |
| Everything a character can play: clips by role + messages | `UOnyxMoveset` (asset) | Moveset |
| One animation: frames, speed, loop, timed events | `UOnyxClip` (asset) | Clip |
| Something that happens at an instant of a clip | `UOnyxClipEvent` | Event |
| Something that stays open for a span of a clip | `UOnyxClipWindow` | Window |
| A named gameplay event a clip can send | `UOnyxMoveset::Messages` + `UOnyxClipEvent_Message` | Message |
| What a character does | `UOnyx*Component` behaviours: PlayerControl, Camera, Respawn, Patrol, ContactDamage, Stompable, Health, Combat | Behaviours |
| Services shared by the whole game | `UOnyx*Subsystem`: Audio, Lighting, Feedback, World, Score, UI | — |

### How a character animates

1. `UOnyxMovesetComponent` reads the character's `UOnyxMovementComponent` state every frame. `ResolveLocomotionClip` picks a
   role (Idle, Run, Jump, Fall, Dash, WallSlide, Crouch). Override it in Blueprint for your own rules.
2. The role is looked up in the `UOnyxMoveset` to get a `UOnyxClip`, and its flipbook plays on the sprite.
3. Attacks, hurt and death are one-shots, played with `PlayOneShot(Role)` on top of locomotion.
4. While a clip plays, each `UOnyxClipEvent` fires when playback crosses its `Time`. Each `UOnyxClipWindow` **opens** when
   playback enters it, **updates** every frame inside it and **closes** when playback leaves.
5. A `Message` event calls the actor's function of that name and broadcasts `OnClipMessage(Name)`.

Built-in events: Message, Sound, Effect, Camera Shake, Light Flash, Scene Flash.
Built-in windows: Hitbox, Invulnerable, Loop Sound.

## Naming rules

- **Prefix:** every class is `Onyx` + what it is, with the standard Unreal prefixes `U`, `A`, `F`, `S`, `E` and `I`. Use
  `Onyx2D` only for the plugin, its modules, `UOnyx2DSettings` and the `Onyx2D` namespace.
- **Name things after what they are, not after a game role.** A foe is a character with the Patrol and Contact Damage
  behaviours. Never write `Player` or `Enemy` classes.
- **Use Onyx's own words.** Moveset, Clip, Event, Window, Message, Lane, Role. Don't borrow the names of other 2D plugins or
  engines.
- **Editor classes**
  - `FOnyx<Asset>Editor` is an asset editor (`FAssetEditorToolkit`).
  - `SOnyx<Thing>` is a Slate widget.
  - `SOnyx<Name>Tool` is a tab of the Onyx2D Tools window (`SOnyxToolsWindow`).
- **Asset helpers**
  - `UOnyx*Factory` is only a real `UFactory`.
  - `FOnyx*Builder` generates assets from a spec.
  - `FOnyxAssetUtils` has the shared create, save and import helpers.
- **Libraries:** `U*Library` is only a Blueprint function library. `UOnyxEditorLibrary` is the facade for Editor Utility
  Blueprints and Python, and editor code doesn't call it.
- **Files:** one main type per file, with the file named after it.

### Asset name prefixes

| Prefix | Asset |
|---|---|
| `MS_` | Moveset |
| `CL_` | Clip |
| `FB_` | Flipbook |
| `S_` | Sprite |
| `T_` | Texture |
| `TS_` | Tile set |
| `TM_` | Tile map |
| `BP_` | Blueprint |

A character built by Onyx2D looks like this:

```
Characters/Fox/BP_Fox
Characters/Fox/MS_Fox
Characters/Fox/Clips/CL_Fox_Run
Characters/Fox/Clips/FB_Fox_Run
Characters/Fox/Sprites/S_Fox_Run_00
```

## Source folders

```
Source/
├─ Onyx2DRuntime/          the game side (ships with the game)
│  ├─ Animation/           Moveset, Clip, ClipEvent, ClipWindow, MovesetComponent
│  ├─ Character/           OnyxCharacter, behaviours, BehaviourLibrary
│  ├─ Movement/  Combat/  Camera/  Lighting/  Audio/  Feedback/  Input/  World/  AI/  Core/
├─ Onyx2DUI/               menus, HUD, settings screens (built in code)
└─ Onyx2DEditor/           editor only
   ├─ MovesetEditor/       the Moveset Editor: timeline, preview, clip list
   ├─ AssetTools/          factories and asset definitions (Content Browser integration)
   ├─ Builders/            asset generators: characters, tile sets, materials, normal maps, art importer, AssetUtils
   ├─ Level/               level operations: painter, placeables, validator, level tools
   ├─ ToolsWindow/         the Onyx2D Tools window and its tabs (Home, Import, Library, Tiles, Painter, Parallax, Level)
   ├─ Widgets/             shared widgets: flipbook preview, sprite slicer, tile palette, map canvas
   ├─ Style/  Commandlets/  Examples/  Lighting/
```

## Principles

- **Components over inheritance:** a character only wires its components together.
- **Single responsibility:** health doesn't know about visuals, and hitboxes don't know about animation.
- **Dependency inversion:** the runtime never references the UI or the editor.
- **Open for extension:** add your own clip events, windows and behaviours, or override Blueprint native events. Never edit
  the plugin to do it.
- **Original code:** Onyx2D is MIT licensed. Write everything against public engine APIs, and never copy code from other
  plugins or from the engine's private modules.
