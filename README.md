# Onyx2D

A complete 2D game framework for Unreal Engine 5.8, written in C++ and fully usable from Blueprint.
Copy this folder into your project as `Plugins/Onyx2D`, open the project and the **Onyx2D** button appears in the toolbar.

## Quick start

1. Put the plugin in `YourProject/Plugins/Onyx2D`, open the project in UE 5.8 and let it compile. Run **Onyx2D → Configure Project for 2D**,
   then **Set Up Level for 2D** on a level and press **Play**.
2. Controls (keyboard / gamepad):

| Action | Keyboard | Gamepad |
|---|---|---|
| Move | A / D, ← / → | Left stick, D-pad |
| Jump (hold for higher) | Space, W, ↑ | A |
| Dash | Left Shift, L | RB, B |
| Attack (combo) | J, Left mouse | X |
| Drop through platform | S + Jump | D-pad down + A |
| Pause | Esc, P | Start |

No input assets are required: a full default scheme is built in code (`UOnyxInputConfig::CreateDefault`).
Create a **Onyx Input Config** data asset and assign it to `OnyxPlayerController` to rebind.

## What you get

### Runtime module (`Onyx2DRuntime`)
| Feature | Class | Highlights |
|---|---|---|
| Platformer movement | `UOnyxMovementComponent` | Jump defined by **height + time to apex**, coyote time, jump buffer, variable jump height, apex hang, fall gravity, max fall speed, double jump, wall slide, wall jump, dash (ground + air), one-way platforms |
| Characters | `AOnyxCharacter`, `AOnyxPlayerCharacter`, `AOnyxEnemyCharacter` | Built on Paper2D `APaperCharacter`; facing (also for art drawn looking left), lit sprites, sprite feet kept on the capsule bottom, crouch, hurt/death, fall death below the level, respawn, placeholder body until art exists |
| Animation | `UOnyxAnimatorComponent` | Name → flipbook map, automatic Idle/Run/Jump/Fall/WallSlide/Dash/Crouch, one-shots, **frame events**; override `ResolveLocomotionAnimation` in Blueprint |
| Combat | `UOnyxCombatComponent`, `UOnyxHitboxComponent`, `UOnyxHealthComponent` | Combo chains with input buffering, hit windows, lunge, knockback, i-frames, teams, contact damage |
| Interfaces | `IOnyxDamageable`, `IOnyxTeamMember`, `UOnyxCombatLibrary` | Make anything hittable (crates, switches) from C++ or Blueprint |
| Game feel | `UOnyxFeedbackSubsystem` | Hit-stop, screen-shake trauma, sprite flash |
| Camera | `UOnyxCameraComponent`, `UOnyxCameraProfile`, `AOnyxCameraBounds` (Camera Zone) | Cinemachine-style framing: screen offset, dead zone + soft zone, per-axis damping, look-ahead, fall look-down, vertical follow modes, confining to zones and the level, profile blending, shake, pixel snap (see *Camera* below) |
| 2D lighting | `AOnyxLightingEnvironment`, `UOnyxLight2DComponent`, `AOnyxLight2D`, `UOnyxShadowCasterComponent`, `UOnyxLightingSubsystem` | Real 2D lighting (no 3D lights on flat cards): soft 2D lights with 2D shadows, normal-map relief, moods with blending (see *2D Lighting* below) |
| Tile collision | `UOnyxTileCollisionComponent`, `FOnyxTileCollision` | Per-tile **None / Solid / One-Way / Hazard** types, merged into a few boxes, invisible walls at the map edges (see *Collision* below) |
| World | `AOnyxOneWayPlatform`, `AOnyxMovingPlatform`, `AOnyxHazardVolume` | Jump-through platforms, moving platforms (drag the waypoints in the viewport, ping-pong or loop), spikes/lava |
| Pickups & progress | `AOnyxPickup`, `AOnyxCheckpoint`, `AOnyxLevelExit`, `UOnyxScoreSubsystem` | Coin / health pickups (glowing placeholder or your own animated flipbook, with a collect effect), checkpoints that set the respawn point and heal, level exits that fade out and open the next level, a coin counter that survives level changes |
| Backgrounds | `AOnyxParallaxBackground`, `UOnyxParallaxComponent` | Any number of tiling texture layers, each with its own scroll factor and depth |
| Prototyping | `AOnyxLevelBuilder` | **Type a level as text** (`#` block, `=` platform, `^` spikes, `P` player, `E` enemy, `L` light, `C` coin, `H` health, `K` checkpoint, `X` exit, `M` moving platform) with live editor preview |
| Enemies | `UOnyxPatrolComponent`, `AOnyxEnemyCharacter` | **Walk** (turn at walls / ledges, chase), **Hop** (frogs, slimes), **Fly** (birds, bats: hover, face the player); stomp them Mario-style; death effects |
| Effects | `AOnyxFlipbookEffect` | Play any flipbook once at a spot (sparkles, puffs, explosions), centred whatever the sprites' pivot |
| Game setup | `AOnyxGameMode`, `AOnyxPlayerController` | Works out of the box |
| Settings | `UOnyxGameUserSettings` | Engine settings + master volume, brightness, mute when unfocused |

### UI module (`Onyx2DUI`)
Built entirely in code, so it works with zero assets, and it is themeable in **Project Settings → Plugins → Onyx2D UI**.
- Title screen, pause menu (with blur), settings screen (Display / Graphics / Audio / Controls), confirm dialogs.
- HUD with health pips, dash cooldown and a coin counter that read the player's components automatically.
- `UOnyxUISubsystem` shows the right screen per level: title in `MainMenuLevel`, HUD elsewhere, pause on the Pause input.
- Full keyboard, mouse and gamepad navigation; swap any screen for a Blueprint subclass in the settings.

### Editor module (`Onyx2DEditor`) — the **Onyx2D** panel inside Unreal
The tools are built from standard editor widgets (Details panels, toolbars, list views, dockable tabs), so they look and behave like the rest of Unreal.
Open them with the **Onyx2D** button on the main toolbar (its ⋮ dropdown jumps straight to a tool or a quick action) or **Tools → Onyx2D**.
The panel opens in its own window (drag its tab into the editor to dock it); each tool is an inner tab you can rearrange or tear off, and the layout is remembered.

| Tool | What it does |
|---|---|
| **Import** | Drop PNG files or whole folders from the file explorer (or **Add Files / Add Folder**) and Onyx2D builds everything: tile sheets become padded, lit tile sets with collision (tile size, gaps and border detected from the file name, transparent gaps or tile edges); animations become a playable character (frame sequences `run-1.png, run-2.png...`, one strip per animation `idle.png, run.png`, or a sheet with one animation per row: words like *idle, run, jump, fall, attack, hurt, death, crouch, climb* pick the animation, the rest of the name or the folder groups files per character; all frames share one cell with the feet on one line); numbered frames without an animation word become animated sprites; prop sheets are cut into one sprite per prop; backgrounds become textures for the Parallax tab. Every guess is listed (type, name, tile size, Player/Enemy) and can be changed before **Build All**. Scripts can call `UOnyxEditorLibrary::ImportArt`. |
| **Character Wizard** | Details panel with the sheet, frame size, output folder, Player/Enemy and the animation rows (row, start column, frame count, FPS, loop, optional strip image). **Detect Frame Size** finds the grid from the transparent gaps; frame counts stop at the first empty cell. The sheet view outlines the frames of the animation picked in the preview, which plays with transport controls. **Auto Fit Capsule** sizes the collision capsule to the visible pixels and puts the feet on the ground. **Create Character** makes sprites, flipbooks and a ready Blueprint (warnings go to the *Onyx2D* Message Log). |
| **Animations** | Pick a Onyx character in the toolbar to get a table of its animations (double-click to rename, flipbook, play rate, loop, frame count) with a live preview and a **Frame Events** table (add at the current frame). Every edit is undoable; Save / Open Blueprint in the toolbar. |
| **Tiles** | Details panel for tile sheet, tile size, margin, spacing, collision and the map (size, scale, ground rows/tile). **Create Tile Set** / **Create Tile Map** (places the map and opens it in the Level Painter). The ground tile is outlined on the sheet. |
| **Level Painter** | LDtk-style level editor. **Layers** on the left (the tile map's tile layers plus an **Entities** layer, each with an eye toggle), your **tile set palette** or the **entity palette** under them, a big canvas in the middle and an **Inspector** on the right. Tools: **Brush (B)**, **Rectangle (R)**, **Fill (F)**, **Erase (E)**, **Pick (I)**, **Select (S)**, **Terrain (T)** (auto-tiling); brush options **Flip X (X)**, **Flip Y (Y)** and **Random (V)** (each cell a random tile from the selected block); right-drag erases, middle-drag or Space+drag pans, the wheel zooms. Tiles go straight into the Paper2D tile map (with collision on Ground); entities (player start, enemy, coin, health, checkpoint, exit, moving/one-way platform, hazard, light) are real actors placed on the cell, which you can drag around and edit in the Inspector. **New Map** makes a 60x24 map from the selected tile set and places it in the level. Everything is undoable. |
| **Parallax** | **Add Background** places an `AOnyxParallaxBackground`; its layers are edited right in the panel. **Add Layers** turns every texture selected in the Content Browser into a layer (far to near, with sensible factors and depths). |
| **Level** | **Set Up Level**, **Configure Project** (turns off Lumen, virtual shadow maps, distance fields, motion blur, auto exposure and TAA; makes Onyx Game Mode the default), **Check Level**, Play, Settings, Help. A grid of every Onyx actor: click to place it where the viewport looks, or drag it into the viewport. The **Level Check** lists problems (missing player start, wrong game mode, no lights, actors off the gameplay plane, unlit sprites, exits without a next level...) each with a **Fix** button, plus **Fix All**; double-click selects the actor. |

Unreal integration:
- **Place Actors → Onyx2D** category (and the toolbar's quick Add menu) with all Onyx actors.
- **Content Browser → right-click a texture → Onyx2D**: *Create Onyx Character…*, *Create Onyx Tile Set…* (opens the tool pre-filled), *Apply Pixel-Art Settings*. Right-click a tile map → *Paint in Level Painter*.
- Custom icons for the plugin, the tools and every Onyx actor class (Outliner, Content Browser, Place Actors).

Everything is C++. The same builders are also exposed to **Editor Utility Blueprints** through `UOnyxEditorLibrary`
(`CreateCharacterFromSheet`, `CreateTileSet`, `CreateTileMap`, `SetupCurrentLevelFor2D`, ...).

Headless builds (no scripting): `UnrealEditor-Cmd.exe YourProject.uproject -run=OnyxExamples` rebuilds missing plugin
materials and the Sunny Land example; add `-materials` to rebuild only the plugin materials. The materials are created by
`FOnyxMaterialBuilder` and re-created automatically when the editor starts if any are missing.

Other editor helpers:
- Imported textures automatically get pixel-art settings (nearest filtering, no mipmaps, uncompressed).
- **Project Settings → Onyx2D → Default Player Class / Default Enemy Class** make a wizard-made character the player, and the enemy spawned by `E` tiles, with no game mode editing.

## Collision

Works like Unity's *Tilemap Collider 2D* + *Composite Collider 2D*, driven by data you paint:

- **Each tile in a tile set has a collision type**: **None**, **Solid**, **One-Way** (jump up through it, Down + Jump drops through) or **Hazard** (hurts on touch).
  In the Level Painter, turn on **Collision (C)**: the palette and the canvas are tinted by type, and the collision bar sets the type of the selected tiles.
  **Auto** guesses it from the pixels (tiles that are mostly filled are Solid). The type is stored in the tile's metadata (`Onyx.Solid`...), so it travels with the tile set.
- **Each layer decides whether it collides** (the collision toggle next to each layer), so decoration layers never block.
- `UOnyxTileCollisionComponent` (added to every painted tile map) merges the cells into as few boxes as possible, builds one-way top slices and hazard hitboxes,
  puts invisible walls on the left and right edges of the map (`bBlockLevelEdges`), and registers the map as the **level bounds**.
- Characters that fall below the level bounds die and respawn at the last checkpoint (`FallDeathDepth`).

## 2D Lighting

Sprites are lit the way 2D engines do it (like Unity's 2D renderer), not by 3D lights shining on flat cards:

- **Onyx Lighting** (one per level) owns the look: a **global light** (ambient colour), a 2D **sun** that only shapes normal maps,
  a **background tint** for the parallax layers, bloom, vignette, colour grading and **neutral tonemapping with fixed exposure**,
  so pixel art keeps its exact painted colours. Presets: **Day, Dusk, Night, Cave, Dungeon**, or tune every value.
- **Onyx Light 2D** components are drawn every frame into a screen-space light buffer as soft discs or cones, **clipped by 2D shadows**
  from solid tiles (Onyx Tile Collision) and **Onyx Shadow Caster 2D** shapes; light soaks a little into tile surfaces.
  The **Onyx Light 2D** actor adds presets (torch, candle, lantern, magic, crystal, lava, window), flicker / pulse and a glow halo.
- A post-process composite lights every lit sprite from that buffer (`PP_OnyxLight2D`); **normal maps** give the art relief.
  Generate them with *right-click a texture / sprite / tile set > Generate Normal Map*; the Character Wizard and Tiles tool make them automatically.
- **Camera Zones** can carry a **Lighting Profile**: walk into a cave and the mood fades to dark; scripts can `PushOverride` a mood too.
- The editor viewport shows the same lighting while you build the level.

## Camera

The player camera (`UOnyxCameraComponent`) follows the rules of Unity's Cinemachine 2D framing:

| Setting | Effect |
|---|---|
| View Height / Projection / FOV | How much of the world is visible (zoom), perspective or orthographic |
| Target Offset, Screen Offset | Where the player sits on screen (e.g. slightly low, to see more above) |
| Dead Zone, Soft Zone, Damping | No movement inside the dead zone, smooth catch-up in the soft zone, hard limit at its edge; damping per axis |
| Vertical Follow | **Always**, **When Grounded** (no bobbing on jumps, like most platformers) or **Locked** |
| Look-Ahead (time, max, smoothing, vertical) | Leads the view in the direction of movement |
| Fall Look-Down | Shows more below when falling fast |
| Confine | **None**, **Zones**, **Level** or **Zones Then Level** (default): zones first, never past the edge of the level |
| Pixel Snap, Shake | Crisp pixel art; screen-shake trauma from hits |
| **Pixel Perfect**, Art Pixel Size | Every art pixel covers the same whole number of screen pixels, the camera moves one screen pixel at a time, orthographic: no uneven pixels, no shimmer |

- Save settings as a **Camera Profile** (Data Asset) and reuse them.
- A **Camera Zone** (Onyx2D → Level → *Camera Zone*) switches to its profile and/or confines the view while the player is inside.
  Zones have a priority and a blend time; the camera blends both settings and confinement smoothly instead of cutting.
- From Blueprint: `PushOverride(Profile, BlendTime)` / `ClearOverride` for cutscenes and boss intros, `AddTrauma` for shake, `SnapToTarget` after teleports.

## Clean pixel art (no seams, no shimmer)

- **Tile padding**: tile sets get a border of copies of each tile's edge pixels (extrusion), so no lines flash between tiles
  while the camera moves. Automatic for tile sets made by the Tiles tool; for others use *right-click a tile set > Fix Tile Seams*
  (the Level Check offers it too). Indices, collision and terrains are kept.
- **Pixel-perfect camera** (Camera > Pixels, on by default): whole screen pixels per art pixel, orthographic, snapped to the screen-pixel grid.
  The Character Wizard sets Art Pixel Size to the character's Centimeters Per Pixel (4 by default, the same as tiles).
- **Sprite pixel snapping** (Onyx characters, animated pickups): sprites are drawn on whole art pixels while movement stays smooth.
- Tile maps sit just behind the gameplay plane (Y = -20) so sprites never z-fight with background tiles; the Level Check fixes old maps.
- The Onyx camera pins its aspect-ratio axis, so the confiner, framing and 2D light buffer always match what is rendered.

## Example: Sunny Land

**Onyx2D menu > Examples > Build Sunny Land Example** (or `-run=OnyxExamples`, C++: `FOnyxSunnyLandExample`) builds a complete level from **Sunny Land** by ansimuz (CC0, opengameart.org/node/74142, shipped in
`Extras/SunnyLand`): frame-by-frame animations packed into strips, an animated fox (idle, run, jump, fall, crouch, hurt),
an opossum (walks), eagles (fly) and frogs (hop) that can be stomped, animated cherries and gems with collect effects, the original
level **imported from its Tiled map** with the game's solid and one-way tiles, props, parallax sea and jungle, a sunny mood outside and
a dark, torch-lit cave underground, plus a **Ground** terrain for auto-tiling. Open `/Game/Maps/SunnyLand` and press Play.

Importers used there (also in the editor):
- **Content Browser > select frame images > Pack Into Animation Strip** (natural name order, feet aligned).
- `ImportTiledMap` (Tiled JSON: layers, names, visibility, flipped / rotated tiles), `CreateCharacterFromStrips`, `SetTileCollision`,
  `CreateTerrain` / `PaintTerrain` in `UOnyxEditorLibrary` for Editor Utility Blueprints.

## Making a game with it

1. **Art:** drag your sprite sheets into the Content Browser (pixel-art settings are applied automatically).
2. **Player:** right-click the sheet → **Onyx2D → Create Onyx Character…**, check the rows, then click **Create Character**.
   Set it as **Default Player Class** in Project Settings → Onyx2D. Tune **Movement → Jump Settings** on the Blueprint.
3. **Enemies:** Blueprint children of `OnyxEnemyCharacter` (patrol + contact damage) or `OnyxCharacter`.
4. **Levels:** open **Onyx2D → Level Painter**, pick your tile set, press **New Map**, paint the ground, then switch to the Entities layer and drop the player, enemies, coins, checkpoints and the exit (or block out with `OnyxLevelBuilder`). Drop pickups, checkpoints, moving platforms and an exit from **Onyx2D → Level**, add a parallax background, then run **Check Level**.
5. **Shadows:** give flat objects a `OnyxShadowCaster` component and keep backgrounds lit (any lit material).
6. **UI:** set your title, levels, fonts and colors in Project Settings → Onyx2D UI.

## Architecture

- **Components over inheritance.** Each feature (movement, health, animation, combat, camera) is a self-contained component.
  Characters only wire them together.
- **Single responsibility.** Health doesn't know about visuals; hitboxes don't know about animation; the camera doesn't know about combat.
- **Dependency inversion.** The runtime never references the UI: pause goes through `FOnyxGameDelegates::OnPauseRequested`,
  and screen shake goes through `UOnyxFeedbackSubsystem::OnTrauma`.
- **Interfaces** (`IOnyxDamageable`, `IOnyxTeamMember`) keep combat open for new hittable things without changes.
- **Open for extension.** Blueprint-native events (`ResolveLocomotionAnimation`), soft-class screen overrides and data-asset input.
- **Registries instead of actor iteration** (`UOnyxWorldSubsystem`) keep per-frame queries cheap.

## Plugin layout
```
Onyx2D/
├─ Onyx2D.uplugin
├─ LICENSE
├─ Source/Onyx2DRuntime/   gameplay, 2D lighting, camera, world actors
├─ Source/Onyx2DUI/        menus, HUD, settings
├─ Source/Onyx2DEditor/    editor tools: Level Painter, Character Wizard, importers, examples
├─ Content/Materials/      lit sprite, 2D lighting composite, glow (generated from C++)
├─ Resources/              icons
└─ Extras/SunnyLand/       art for the Sunny Land example (CC0)
```

## Credits and license

Onyx2D is released under the MIT License (see `LICENSE`).
The Sunny Land example art in `Extras/SunnyLand` is by Luis Zuno ([@ansimuz](https://ansimuz.com)), public domain (CC0); see
`Extras/SunnyLand/public-license.txt`.
