# Onyx2D

A complete 2D game framework for Unreal Engine 5.8, written in C++ and fully usable from Blueprint.
Copy this folder into your project as `Plugins/Onyx2D`, open the project and the **Onyx2D** button appears in the toolbar.

## Installation

1. Copy this folder into your project as `YourProject/Plugins/Onyx2D` (Unreal Engine 5.8). Paper2D and Enhanced Input are
   enabled with it.
2. Open the project and let Unreal compile the plugin (a Blueprint-only project asks to rebuild it; that needs Visual Studio
   with the C++ game development workload).
3. Run **Onyx2D → Configure Project for 2D** once: recommended renderer settings and the Onyx game mode as the default.

## Quick start: from art to a playable level

1. **Art:** open the **Onyx2D** window, go to the **Import** tab and drop your PNGs or art folders on it. Tile sheets become
   tile sets, animations become characters, other images become sprites or backgrounds. Check the list, press **Build All**.
2. **Characters:** make a **Moveset** (a character's clips by role: Idle, Run, Jump...). Cut each **Clip** from a sprite sheet
   (**New Clip**), tune frame holds and add **events** on the timeline (messages, sounds, effects, hitbox windows), then
   **New Character**: pick its behaviours (Hero = Player Control + Camera + Respawn; Patrolling Foe = Patrol + Contact Damage +
   Stompable). Set your hero as **Default Player Class** in Project Settings → Onyx2D.
3. **Level:** run **Set Up Level for 2D** on a level, then open the **Level Painter**, pick your tile set, press **New Map**
   and paint. Switch to the Entities layer to drop the player, enemies, coins, checkpoints and the exit.
4. **Polish:** add 2D lights and a lighting mood, a parallax background from the **Parallax** tab, then run **Check Level**.
5. **UI:** set your title, levels, fonts and colours in Project Settings → Onyx2D UI.

## What you get

### Runtime module (`Onyx2DRuntime`)
| Feature | Class | Highlights |
|---|---|---|
| Platformer movement | `UOnyxMovementComponent` | Jump defined by **height + time to apex**, coyote time, jump buffer, variable jump height, apex hang, fall gravity, max fall speed, double jump, wall slide, wall jump, dash (ground + air), one-way platforms |
| Characters | `AOnyxCharacter` | One class for every character, built on Paper2D `APaperCharacter`: facing (also for art drawn looking left), lit sprites, sprite feet kept on the capsule bottom, crouch, hurt/death with an optional death effect, fall death below the level, placeholder body until art exists. **What it does comes from behaviour components** (below), not from subclasses |
| Behaviours | `UOnyxPlayerControlComponent`, `UOnyxCameraComponent`, `UOnyxRespawnComponent`, `UOnyxPatrolComponent`, `UOnyxContactDamageComponent`, `UOnyxStompableComponent`, `UOnyxBehaviourLibrary` | Composition like Unity's components: the player drives a character with **Player Control** (input on its own input component, works on any pawn the player possesses); **Respawn** brings it back at checkpoints; a foe is **Patrol** + **Contact Damage** + **Stompable**. Mix freely, or write your own component |
| Moveset assets | `UOnyxMoveset`, `UOnyxClip`, `UOnyxClipEvent`, `UOnyxClipWindow` | A **Moveset** is everything a character can play: its **Clips** by role (Idle, Run, Attack1...) and its **Messages**. A **Clip** is one animation: flipbook frames with per-frame holds, speed, loop, and **lanes** of timed **Events** (*Message*: runs the actor's function of that name and fires *On Clip Message*; *Sound*, *Effect*, *Camera Shake*, *Light Flash*, *Scene Flash*) and **Windows** that stay open for a span (*Hitbox*, *Invulnerable*, *Loop Sound*). Subclass either in C++ or Blueprint for your own |
| Moveset playback | `UOnyxMovesetComponent` | Plays the character's **Moveset**: automatic Idle/Run/Jump/Fall/WallSlide/Dash/Crouch, one-shots (attacks, hurt, death), fires the playing clip's events and opens / closes its windows; override `ResolveLocomotionClip` in Blueprint |
| Combat | `UOnyxCombatComponent`, `UOnyxHitboxComponent`, `UOnyxHealthComponent` | Combo chains with input buffering, hit windows (from the attack clip's *Hitbox* window when it has one), lunge, knockback, i-frames, teams, contact damage |
| Interfaces | `IOnyxDamageable`, `IOnyxTeamMember`, `UOnyxCombatLibrary` | Make anything hittable (crates, switches) from C++ or Blueprint |
| Game feel | `UOnyxFeedbackSubsystem` | Hit-stop, screen-shake trauma, sprite flash |
| Audio | `UOnyxAudioSubsystem`, `AOnyxLevelAudio` | Music and ambience that crossfade across levels and zones, volume categories, 2D distance falloff (see *Audio* below) |
| Interaction | `IOnyxInteractable`, `AOnyxLightSwitch` | The Interact input uses levers, doors, NPCs... (see *Interaction* below) |
| Camera | `UOnyxCameraComponent`, `UOnyxCameraProfile`, `AOnyxCameraBounds` (Camera Zone) | Cinemachine-style framing: screen offset, dead zone + soft zone, per-axis damping, look-ahead, fall look-down, vertical follow modes, confining to zones and the level, profile blending, shake, pixel snap (see *Camera* below) |
| 2D lighting | `AOnyxLightingEnvironment`, `UOnyxLight2DComponent`, `AOnyxLight2D`, `UOnyxShadowCasterComponent`, `UOnyxLightingSubsystem` | Real 2D lighting (no 3D lights on flat cards): soft 2D lights with 2D shadows, normal-map relief, moods with blending (see *2D Lighting* below) |
| Tile collision | `UOnyxTileCollisionComponent`, `FOnyxTileCollision` | Per-tile **None / Solid / One-Way / Hazard** types, merged into a few boxes, invisible walls at the map edges (see *Collision* below) |
| World | `AOnyxOneWayPlatform`, `AOnyxMovingPlatform`, `AOnyxHazardVolume` | Jump-through platforms, moving platforms (drag the waypoints in the viewport, ping-pong or loop), spikes/lava |
| Water | `AOnyxWater` | Characters swim, physics objects float, bob and drift with the current, the surface ripples and splashes (see *Water* below) |
| Pickups & progress | `AOnyxPickup`, `AOnyxCheckpoint`, `AOnyxLevelExit`, `UOnyxScoreSubsystem` | Coin / health pickups (glowing placeholder or your own animated flipbook, with a collect effect), checkpoints that set the respawn point and heal, level exits that fade out and open the next level, a coin counter that survives level changes |
| Backgrounds | `AOnyxParallaxBackground`, `UOnyxParallaxComponent` | Any number of tiling texture layers, each with its own scroll factor and depth |
| Prototyping | `AOnyxLevelBuilder` | **Type a level as text** (`#` block, `=` platform, `^` spikes, `~` water, `P` player, `E` enemy, `L` light, `C` coin, `H` health, `K` checkpoint, `X` exit, `M` moving platform) with live editor preview |
| Patrols | `UOnyxPatrolComponent` | **Walk** (turn at walls / ledges, chase), **Hop** (frogs, slimes), **Fly** (birds, bats: hover, face the player); with Stompable they are defeated Mario-style |
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
The panel opens in its own window (drag its tab into the editor to dock it). A tool bar across the top is always there: click a tool to open
or focus it, even after closing its tab, or **Reset Layout** to get every tab back. Each tool is an inner tab you can rearrange or tear off,
and the layout is remembered. The console command `Onyx2D.Open <Tool>` (e.g. `Onyx2D.Open Library`) opens a tool directly.

| Tool | What it does |
|---|---|
| **Home** | The start page: the steps from art to a playable level, each one a button (configure project, import, set up level, paint, fill, check and play), the Sunny Land example, and **Extend Onyx2D**: where your code goes, with **New Blueprint** / **New C++ Class** buttons for every Onyx base class (player, enemy, patrol behaviour, pickup, light, hazard, platform, checkpoint, exit, game mode, controller). C++ classes go in your game module, never in the plugin. |
| **Library** | Everything Onyx-ready in the project, with thumbnails: characters, enemies, objects (Blueprints and your own C++ classes built on Onyx classes), tile sets, tile maps, movesets, clips and flipbooks. Pick one to see it play (characters: a button per animation); **Place in Level**, **Edit Moveset**, **Paint With It**, **Open** (C++ classes open their source), **Show in Content Browser**. Updates by itself as assets change. |
| **Import** | Drop PNG files or whole folders from the file explorer (or **Add Files / Add Folder**) and Onyx2D builds everything: tile sheets become padded, lit tile sets with collision (tile size, gaps and border detected from the file name, transparent gaps or tile edges); animations become a playable character (frame sequences `run-1.png, run-2.png...`, one strip per animation `idle.png, run.png`, or a sheet with one animation per row: words like *idle, run, jump, fall, attack, hurt, death, crouch, climb* pick the animation, the rest of the name or the folder groups files per character; all frames share one cell with the feet on one line); numbered frames without an animation word become animated sprites; prop sheets are cut into one sprite per prop; backgrounds become textures for the Parallax tab. Every guess is listed (type, name, tile size, Player/Enemy) and can be changed before **Build All**. Scripts can call `UOnyxEditorLibrary::ImportArt`. |
| **Movesets** | The Moveset Editor. Left: the **Moveset**'s clips by role (missing standard roles, like Run or Hurt, are one click away). Middle: a big preview with transport controls (play, step, loop; sounds play while previewing) and the **timeline**: a block per frame as wide as its hold (drag the right edge to change it, drag to reorder, right-click to duplicate / delete) and event **lanes** (right-click to send a Message or add a Sound, Effect, Camera Shake, a Window or your own event at that time; drag to move, snaps to frames), a ruler to scrub (Ctrl + wheel zooms). Right: the clip's loop / frames per second / speed, the selected frame (hold, sprite) and the selected event. **New Clip** opens the sprite sheet slicer (detects the grid; click frames in play order or drag across a run). **New Character** makes a playable character from the moveset with the behaviours and team you pick. Double-clicking a Moveset or a Clip opens it here; everything is undoable. |
| **Tiles** | Details panel for tile sheet, tile size, margin, spacing, collision and the map (size, scale, ground rows/tile). **Create Tile Set** / **Create Tile Map** (places the map and opens it in the Level Painter). The ground tile is outlined on the sheet. |
| **Level Painter** | LDtk-style level editor. Characters, objects and decorations are drawn as they look in the level (their sprites and animations, playing), and every character Blueprint in the project is in the object palette. **Layers** on the left (the tile map's tile layers plus an **Entities** layer, each with an eye toggle), your **tile set palette** or the **entity palette** under them, a big canvas in the middle and an **Inspector** on the right. Tools: **Brush (B)**, **Rectangle (R)**, **Fill (F)**, **Erase (E)**, **Pick (I)**, **Select (S)**, **Terrain (T)** (auto-tiling); brush options **Flip X (X)**, **Flip Y (Y)** and **Random (V)** (each cell a random tile from the selected block); right-drag erases, middle-drag or Space+drag pans, the wheel zooms. Tiles go straight into the Paper2D tile map (with collision on Ground); entities (player start, patrolling foe, coin, health, checkpoint, exit, moving/one-way platform, hazard, light) are real actors placed on the cell, which you can drag around and edit in the Inspector; the canvas only shows the objects that sit on this map. **New Map** asks for a name, size and optional ground row, makes the map from the selected tile set and places it beside the maps already in the level. Everything is undoable. |
| **Parallax** | **Add Background** places an `AOnyxParallaxBackground`; its layers are edited right in the panel. **Add Layers** turns every texture selected in the Content Browser into a layer (far to near, with sensible factors and depths). |
| **Level** | **Set Up Level**, **Configure Project** (turns off Lumen, virtual shadow maps, distance fields, motion blur, auto exposure and TAA; makes Onyx Game Mode the default), **Check Level**, Play, Settings, Help. A grid of every Onyx actor: click to place it where the viewport looks, or drag it into the viewport. The **Level Check** lists problems (missing player start, wrong game mode, no lights, actors off the gameplay plane, unlit sprites, exits without a next level...) each with a **Fix** button, plus **Fix All**; double-click selects the actor. |

Unreal integration:
- **Place Actors → Onyx2D** category (and the toolbar's quick Add menu) with all Onyx actors.
- **Content Browser → right-click a texture → Onyx2D**: *Make Clip from Sheet…*, *Create Onyx Tile Set…* (opens the tool pre-filled), *Apply Pixel-Art Settings*. Right-click a tile map → *Paint in Level Painter*.
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
  Generate them with *right-click a texture / sprite / tile set > Generate Normal Map*; the Moveset Editor, the importer and the Tiles tool make them automatically.
- **Camera Zones** can carry a **Lighting Profile**: walk into a cave and the mood fades to dark; scripts can `PushOverride` a mood too.
- The editor viewport shows the same lighting while you build the level.
- **Day and night:** tick *Day Night Cycle* on Onyx Lighting, set the *Day Length* and the moods through the day (night, dawn,
  day, dusk by default); scrub *Time Of Day* to preview the level at any hour. *On Day Phase Changed* fires at nightfall and daybreak.
- **Switching lights:** every light fades on / off (`Set Light Enabled`, *Start Enabled*); the **Onyx Light Switch** (a lever, or a
  pressure plate with *Switch On Touch*) controls any number of lights. Onyx Light 2D actors can carry a looping **Ambient Sound**
  (a crackling torch) and a **Switch Sound**, both following the light.
- **Flashes:** `Flash` on the lighting subsystem lights up the whole scene for a moment (lightning, explosions); `AOnyxLightFlash::Spawn`
  makes a short burst of 2D light. Both are also clip **events**: *Scene Flash* and *Light Flash* on the timeline.

## Audio

- **Onyx Audio** (`UOnyxAudioSubsystem`) plays everything: a **music** channel and an **ambience** channel that crossfade and keep
  playing across level loads, sounds by category so the **Music / Sound Effects / Interface** sliders (Settings > Audio, on top
  of Master) apply, and a distance falloff made for side-on worlds (full volume around the camera, panned left / right further out).
- **Onyx Level Audio** sets a level's music and ambience (Set Up Level for 2D adds one). **Camera Zones** have *Music* and
  *Ambience*: walk into the cave and the cave track fades in, walk out and the level's comes back.
- **Characters** have *Sounds* (jump, land, dash, hurt, death) with a little pitch variation; footsteps and swings are **Sound
  events** on a clip's timeline (they can follow the character). **Hitboxes** have a *Hit Sound* and *Hit Effect*;
  checkpoints, level exits, pickups and light switches have their own sounds.
- From Blueprint: *Play Sound At*, *Play Sound 2D*, *Play Sound Attached*, *Set Level Music*, *Push Music* / *Pop Music* (boss fights).

## Water

**Onyx Water** (`AOnyxWater`) is a body of water: its location is the middle of the surface, *Size* sets width and depth.
In the **Level Painter**, pick *Water* on the Entities layer and drag out the block of cells it fills (a click gives 8 x 4);
with **Select (S)** drag its edges or corners to resize it and its inside to move it, and edit it in the Inspector.
Physics works like Unity's buoyancy effector, plus what Unity leaves to you:

- **Floating:** physics objects are lifted at several points across them by the part under the surface (*Density* in g/cm³,
  1 = water: lighter objects float, heavier ones sink), so they bob, tilt and right themselves; *Steadiness* settles them on
  a flat side like game crates (0 = pure physics, where a square block floats on a corner); slowed by *Drag* and *Spin Drag*,
  carried by the *Current* (with *Current Variation* over time).
- **Physics Prop** (`AOnyxPhysicsProp`, in the painter's palette and Place Actors): any sprite as a crate, barrel or rock that
  falls, tumbles and floats, with its own *Density* (wood 0.6, stone 2.5). It moves only in the level's plane.
- **Swimming:** Onyx characters swim when their middle goes under. Jump strokes upward, Down dives, at the surface they bob
  with the head out and Jump leaves the water. Tune it in *Swim* on the movement component (speed, stroke, sinking, exit jump);
  the Moveset plays a *Swim* clip if the character has one (else Fall). Events: *On Started Swimming*, *On Stopped Swimming*, *On Swim Stroke*.
- **Waves:** a spring surface that ripples by itself and splashes when anything crosses it, harder the faster it goes,
  with an optional *Splash Effect* (flipbook) and *Splash Sound*.
- **Look:** a smooth shader, not pixels: a gradient from *Shallow Color* to *Deep Color*, drifting caustics and light shafts,
  rising bubbles and an anti-aliased surface line, drawn in front of what is in the water and darkened by the lighting mood.
- In a typed level, `~` is water (a run of `~`, repeated on the rows below for depth). `/Game/Maps/Test_Water` (built with the
  examples after Sunny Land) is a Sunny Land tile map with a pool, crates that float, a rock that sinks and gems to dive for,
  all placed with the Level Painter, so it opens there ready to edit.

2D levels (with tile maps, a Level Builder or Onyx Lighting) open in the editor's flat **side view** of the gameplay plane,
framed on the level (*Project Settings → Onyx2D → Open Levels In Side View*).

## Interaction

The **Interact** input uses the nearest thing the character touches that implements **Onyx Interactable** (C++ or Blueprint:
*Interact*, and optionally *Is Interaction Locked* and *Get Interaction Prompt*): levers, doors, signs, chests, NPCs. The Light Switch is one. With nothing to
use, Player Control's *On Interact* fires instead.

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
  New characters get Art Pixel Size = their Centimeters Per Pixel (4 by default, the same as tiles).
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

## Input

`AOnyxPlayerController` comes with a default set of bindings (move, jump, dash, attack, drop through one-way platforms,
pause) for keyboard and gamepad, built in code (`UOnyxInputConfig::CreateDefault`), so a new project needs no input assets.
To use your own bindings, create an **Onyx Input Config** data asset and assign it to the player controller (or your subclass).

## Architecture

- **Components over inheritance.** Each feature (movement, health, animation, combat, camera) is a self-contained component.
  Characters only wire them together.
- **Single responsibility.** Health doesn't know about visuals; hitboxes don't know about animation; the camera doesn't know about combat.
- **Dependency inversion.** The runtime never references the UI: pause goes through `FOnyxGameDelegates::OnPauseRequested`,
  and screen shake goes through `UOnyxFeedbackSubsystem::OnTrauma`.
- **Interfaces** (`IOnyxDamageable`, `IOnyxTeamMember`) keep combat open for new hittable things without changes.
- **Open for extension.** Blueprint-native events (`ResolveLocomotionClip`), your own clip events and windows, soft-class screen overrides and data-asset input.

The class map, naming rules and source folders are in [ARCHITECTURE.md](ARCHITECTURE.md).
- **Registries instead of actor iteration** (`UOnyxWorldSubsystem`) keep per-frame queries cheap.

## Plugin layout
```
Onyx2D/
├─ Onyx2D.uplugin
├─ LICENSE
├─ Source/Onyx2DRuntime/   gameplay, 2D lighting, camera, world actors
├─ Source/Onyx2DUI/        menus, HUD, settings
├─ Source/Onyx2DEditor/    editor tools: Moveset Editor, Level Painter, importers, examples
├─ ARCHITECTURE.md         class map, naming rules, folders
├─ Content/Materials/      lit sprite, 2D lighting composite, glow (generated from C++)
├─ Resources/              icons
└─ Extras/SunnyLand/       art for the Sunny Land example (CC0)
```

## Credits and license

Onyx2D is released under the MIT License (see `LICENSE`).
The Sunny Land example art in `Extras/SunnyLand` is by Luis Zuno ([@ansimuz](https://ansimuz.com)), public domain (CC0); see
`Extras/SunnyLand/public-license.txt`.
