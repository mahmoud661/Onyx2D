# Onyx2D

A complete 2D game framework for Unreal Engine 5.8, written in C++ and fully usable from Blueprint.
Copy this folder into your project as `Plugins/Onyx2D`, open the project and the **Onyx2D** button appears in the toolbar.

## Installation

1. Copy this folder into your project as `YourProject/Plugins/Onyx2D` (Unreal Engine 5.8). Paper2D and Enhanced Input are
   enabled with it.
2. Open the project and let Unreal compile the plugin (a Blueprint-only project asks to rebuild it; that needs Visual Studio
   with the C++ game development workload).
3. Run **Onyx2D → Configure Project for 2D** once: recommended renderer settings and the Onyx game mode as the default.

**New here?** With the **Onyx2D Examples** plugin beside this one, press **Build Starter Game** on the Onyx2D window's Home tab: a small finished game (a meadow with a sign, a
frog to talk to, enemies, a key and a locked gate) made only of Trigger Zones, Signs and the Talk behaviour, so you can open
every piece and see how it works. The full documentation is a Next.js site in the `Onyx2D-Docs` repository; the editor's
**Help** buttons open it (*Project Settings → Onyx2D → Docs Url*).

**New to Unreal?** The docs' **Learn Unreal** course teaches the editor, levels and actors, Blueprints, C++, the gameplay
framework, input, Paper2D, collision, debugging and packaging step by step, with screenshots and compiled C++. **Home →
Build Learn Unreal Examples** (from the Onyx2D Examples plugin) builds what it makes, finished, in `Content/LearnUnreal`: `BP_HelloWorld`, the `BP_Spring`
spring pad (its node graph built node by node), Onyx2D's controls as Enhanced Input assets and the `LU_Playground` level.

## Quick start: from art to a playable level

1. **Art:** open the **Onyx2D** window, go to the **Import** tab and drop your PNGs or art folders on it. Tile sheets become
   tile sets, animations become characters, other images become sprites or backgrounds. Check the list, press **Build All**.
2. **Characters:** make a **Moveset** (a character's clips by role: Idle, Run, Jump...). Cut each **Clip** from a sprite sheet
   (**New Clip**), tune frame holds and add **events** on the timeline (messages, sounds, effects, hitbox windows), then
   **New Character**: pick its behaviours (Hero = Player Control + Camera + Respawn; Patrolling Foe = Patrol + Contact Damage +
   Stompable; Fighting Foe = Patrol + Contact Damage + Enemy AI). Set your hero as **Default Player Class** in Project Settings → Onyx2D.
3. **Level:** run **Set Up Level for 2D** on a level, then open the **Level Painter**, pick your tile set, press **New Map**
   and paint. Switch to the Entities layer to drop the player, enemies, coins, checkpoints and the exit.
4. **Polish:** add 2D lights and a lighting mood, a parallax background from the **Parallax** tab, then run **Check Level**.
5. **UI:** set your title, levels, fonts and colours in Project Settings → Onyx2D UI.

## What you get

### Runtime module (`Onyx2DRuntime`)
| Feature | Class | Highlights |
|---|---|---|
| Platformer movement | `UOnyxMovementComponent` | Jump defined by **height + time to apex**, coyote time, jump buffer, variable jump height, apex hang, fall gravity, max fall speed, double jump, wall slide, wall jump, dash (in the air; on the ground it is a **roll**), one-way platforms; walking below a speed you set; crouching that shrinks the capsule (*Crouch Height Scale*) and crouch-walking; a **slide** (Down while running) that carries on low and slows down; **ledges**: falling past the top of a wall while holding towards it catches the edge, Jump or Up climbs, Down or away lets go |
| Game styles: side view or top down | `EOnyxGameStyle`, `UOnyxLevelData` (World Settings > Onyx Level) | A level is played **from the side** (a platformer) or **from above** (top down); the project picks the default, each level can choose its own, and a game can mix both. Top down, the same characters walk in eight directions with no gravity, look **Down, Up and to the Side** (clips drawn per direction: `Walk_Down`, `Walk_Up`, `Walk_Side`, plus `Walk_DownSide` / `Walk_UpSide` for eight-way art), strike, dash and are knocked back across the ground, and whoever stands lower on the screen draws in front (see *Game styles* below) |
| Characters | `AOnyxCharacter` | One class for every character, built on Paper2D `APaperCharacter`: facing (also for art drawn looking left), lit sprites, sprite feet kept on the capsule bottom, crouch, hurt/death with an optional death effect, fall death below the level, placeholder body until art exists. **What it does comes from behaviour components** (below), not from subclasses |
| Behaviours | `UOnyxPlayerControlComponent`, `UOnyxCameraComponent`, `UOnyxRespawnComponent`, `UOnyxPatrolComponent`, `UOnyxContactDamageComponent`, `UOnyxStompableComponent`, `UOnyxEnemyAIComponent`, `UOnyxBehaviourLibrary` | Composition like Unity's components: the player drives a character with **Player Control** (input on its own input component, works on any pawn the player possesses); **Respawn** brings it back at checkpoints; a platformer foe is **Patrol** + **Contact Damage** + **Stompable**, a foe that fights back adds **Enemy AI**. Mix freely, or write your own component |
| Moveset assets | `UOnyxMoveset`, `UOnyxClip`, `UOnyxClipEvent`, `UOnyxClipWindow` | A **Moveset** is everything a character can play: its **Clips** by role (Idle, Run, Attack1...) and its **Messages**. A **Clip** is one animation: flipbook frames with per-frame holds, speed, loop, and **lanes** of timed **Events** (*Message*: runs the actor's function of that name and fires *On Clip Message*; *Sound*, *Effect*, *Camera Shake*, *Light Flash*, *Scene Flash*) and **Windows** that stay open for a span (*Hitbox*, *Invulnerable*, *Loop Sound*). Subclass either in C++ or Blueprint for your own |
| Moveset playback | `UOnyxMovesetComponent` | Plays the character's **Moveset**: automatic Idle/Walk/Run/Jump/Fall/WallSlide/Dash/Roll/Slide/Crouch/CrouchWalk/LedgeHang/LedgeClimb (each falling back to a near one when the Moveset doesn't have it), **transitions** between them when it has them (*Land* after a hard landing until the character moves, *AirJump* for a jump in the air, *WallGrab* on catching a wall), one-shots (attacks, hurt, death), fires the playing clip's events and opens / closes its windows; override `ResolveLocomotionClip` in Blueprint |
| Combat | `UOnyxCombatComponent`, `UOnyxHitboxComponent`, `UOnyxHealthComponent` | Combo chains with input buffering (*Loop Combo*: slashes that alternate for as long as you press), an **Air Combo** for attacks started in the air, a **Down Air Combo** with Down held (a down-slash whose *Bounce* sends the attacker up off what it hits, air moves back), an **Up Combo** with Up held (on the ground or in the air) a **Dash Combo** for attacking out of a dash or roll (a strike the dash carries) and a **Run Combo** for attacks started while running on the ground (cuts drawn over running legs, so the run carries on). **Clash**: blades that meet push both fighters apart, knock the foe off balance and give a moment of safety; attacks cut hostile projectiles out of the air. A hitbox shape per attack (*Hitbox Offset / Size*, also per Enemy AI attack), hit windows (from the attack clip's *Hitbox* window when it has one; cancelling an attack closes them at once), lunge, recoil, knockback, i-frames (an *Invulnerable* window on a roll), teams, contact damage. Dying plays the death clip to its end and holds its last frame; a body without a death effect lies for *Corpse Lifetime*, then dissolves art pixel by art pixel (*Corpse Fade Time*); the player's screen fades out and in around a respawn |
| Energy and mending | `UOnyxEnergyComponent` | Energy gathered by the blows a character lands (*Energy Per Hit*), spent by holding **Mend** (K / F / left shoulder): a wind-up, then one *Mend Cost* drains over *Mend Time* and heals *Mend Heal*; holding on mends again; a hit breaks it off and the energy drained is lost. The HUD shows it as cells, one per mend, glowing when full. *Spend Energy* for anything else |
| Sounds made in code | `FOnyxSoundSynth` (editor), `FOnyxContentKit::MakeSounds` | Sound effects synthesised from recipes (voices of sine, triangle, square, saw or noise, pitch glides, envelopes, gliding filters, vibrato, tremolo, crushing; loops for ambience), written as WAVs and imported. The kit's set: swing, hit, clash, jump, land, footstep, dash, hurt, death, foe death, coin, mending, checkpoint, shot, burst. Play them from clips (Sound events on frames), characters, hitboxes, projectiles, gates and camera zones' music and ambience; `LogOnyxAudio` at Verbose lists what plays |
| Sounds from recordings | `FOnyxSoundCueBuilder` (editor) | Imports sound files (wav, ogg, flac, mp3) and builds sound cues from them in C++: layers played together (a slice over a body blow), each picking one of its takes at random (never the same twice running), each take levelled by its own gain, every play pitched and turned a little at random. Put them in the kit's sound slots (`FOnyxContentKit::Sounds`) to replace the synthesised ones |
| Bosses | `UOnyxBossComponent`, `AOnyxBossArena`, `AOnyxGate` | A boss is moves made of steps (a tell, the attack, an opening to punish; vanish, reappear, ambush), chosen by range, phase, weight and cooldown, with follow-ups (at most *Max Chain* in a row). Step actions: Strike, Glide, Rush, Leap, Teleport (behind / in front of the player, far end, centre, over the arena), Shoot (at the player, down, sweeping the floor), Summon. Phases at shares of health (faster, with an opening move), a stagger after a run of blows (not again for *Stagger Grace* after it gets up), an *Answer Move* when it is hacked at between its moves (*Answer Hits* blows), an intro, a slowed-down death. Fighters pass through it. The arena shuts its gates when the player steps in, resets while the screen is dark if the player falls, and opens them when the boss is beaten. The HUD shows its title card and health bar; a camera room's *Focus Actors* keep it in view |
| Getting hurt | `AOnyxCharacter` | *Hurt Stun Time* (no control for a moment, thrown back), *Blink While Invulnerable*, *Hurt Screen Flash* for the player |
| Interfaces | `IOnyxDamageable`, `IOnyxTeamMember`, `UOnyxCombatLibrary` | Make anything hittable (crates, switches) from C++ or Blueprint |
| Enemy AI | `UOnyxEnemyAIComponent` | A foe that notices the player (sight range, walls block its view), gives a start, chases (walkers stop at ledges, flyers hover above, casters keep their distance) and attacks with a readable rhythm: **wind-up** (it stops, flashes its tell colour, plays its clip, rears back or rises), **strike**, **recovery** to punish. Attacks: **Lunge**, **Pounce**, **Shoot** (fans of projectiles), **Dive**, **Charge**, each with range, timing, cooldown and damage. Hits stagger it (not mid-strike), it gives up when it loses you, can lie **dormant** until you come near (a skeleton rising from the ground), and scatters **coins** when it dies |
| Projectiles | `AOnyxProjectile`, `FOnyxProjectileSpec` | Fireballs, wisps, arrows: a flipbook with its own 2D light, speed, gravity, damage, knockback; hurts only the shooter's foes and bursts (with an effect) on walls, floors, platforms and targets |
| Game feel | `UOnyxFeedbackSubsystem`, `UOnyxCharacterEffectsComponent`, `AOnyxAfterimage` | Hit-stop, screen-shake trauma, a **flash** that turns the sprite a colour (white when hit, the tell colour when a foe winds up), hit sparks where blows land, recoil off what you strike; **Character Effects**: dust when jumping off the ground or a wall, landing and rolling (none for a jump or dash in the air or in water; an optional *Air Jump Effect* of its own), glowing **afterimages** while dashing, blinking while invulnerable; a character doesn't turn round when knocked back |
| Audio | `UOnyxAudioSubsystem`, `AOnyxLevelAudio` | Music and ambience that crossfade across levels and zones, volume categories, 2D distance falloff (see *Audio* below) |
| Interaction | `IOnyxInteractable`, `AOnyxLightSwitch` | The Interact input uses levers, doors, NPCs... (see *Interaction* below) |
| Camera | `UOnyxCameraComponent`, `UOnyxCameraProfile`, `AOnyxCameraBounds` (Camera Zone) | Cinemachine-style framing: screen offset, dead zone + soft zone, per-axis damping, look-ahead, fall look-down, vertical follow modes (swimming counts as grounded: it follows divers down), confining to zones and the level, profile blending, shake, pixel snap (see *Camera* below) |
| 2D lighting | `AOnyxLightingEnvironment`, `UOnyxLight2DComponent`, `AOnyxLight2D`, `UOnyxShadowCasterComponent`, `UOnyxLightingSubsystem` | Real 2D lighting (no 3D lights on flat cards): soft 2D lights with 2D shadows, normal-map relief, moods with blending (see *2D Lighting* below) |
| Tile collision | `UOnyxTileCollisionComponent`, `FOnyxTileCollision` | Per-tile **None / Solid / One-Way / Hazard / Edge** types, merged into a few boxes, invisible walls at the map edges (see *Collision* below) |
| World | `AOnyxOneWayPlatform`, `AOnyxMovingPlatform`, `AOnyxHazardVolume`, `AOnyxBreakable`, `AOnyxMirrorFloor` | Jump-through platforms, moving platforms (drag the waypoints in the viewport, ping-pong or loop), spikes/lava, **breakables**: any sprite as a pot or urn the player smashes (it rocks when struck, takes *Hits* blows, bursts with an effect and a sound, spills *Coins*; only the player's side breaks it, it never blocks the way, and it doesn't fill the energy unless *Feeds Energy*); **mirror floors**: polished floors that show everything above them upside down, lit as it is, fading with depth (*Width*, *Strength*, *Depth*, *Falloff*, *Tint*) |
| Water | `AOnyxWater` | Characters swim, physics objects float, bob and drift with the current, the surface ripples and splashes (see *Water* below) |
| Pickups & progress | `AOnyxPickup`, `AOnyxCheckpoint`, `AOnyxLevelExit`, `UOnyxScoreSubsystem` | Coin / health pickups (glowing placeholder or your own animated flipbook, with a collect effect), checkpoints that set the respawn point and heal (every time you come back, a resting place: *Heal On Every Visit*), drawn as you like (a look before it is reached and one after, an effect and a flaring glow as it lights; a plain post without art), level exits that fade out and open the next level (with a *Look* of their own, a door or a portal, a breathing *Glow* that swells as you step in, and an *Enter Effect*), a coin counter that survives level changes |
| Backgrounds | `AOnyxParallaxBackground`, `UOnyxParallaxComponent` | Any number of tiling texture layers, each with its own scroll factor and depth |
| Prototyping | `AOnyxLevelBuilder` | **Type a level as text** (`#` block, `=` platform, `^` spikes, `~` water, `P` player, `E` enemy, `L` light, `C` coin, `H` health, `K` checkpoint, `X` exit, `M` moving platform) with live editor preview |
| Patrols | `UOnyxPatrolComponent` | **Walk** (turn at walls / ledges, chase), **Hop** (frogs, slimes), **Fly** (birds, bats: hover, face the player); with Stompable they are defeated Mario-style; with Enemy AI, patrolling is what they do until they see you |
| Effects | `AOnyxFlipbookEffect` | Play any flipbook once at a spot (sparkles, puffs, explosions), centred whatever the sprites' pivot |
| Logic without Blueprints | `AOnyxTriggerZone`, `UOnyxAction` | An invisible zone that runs a list of **actions** when the player enters, presses Interact, or the level starts: show a message, dialogue, play a sound, spawn an effect, teleport, show / hide, move, give coins / items / health, hurt, open a level, shake the camera, switch lights, launch, set a checkpoint, play music, call a Blueprint event, control other zones. It can **need an item** (a key) or coins first, with *Otherwise Actions*. Make your own action in Blueprint (*On Run*) or C++ |
| Dialogue | `UOnyxDialogueSubsystem`, `AOnyxSign`, `UOnyxTalkComponent` | **Messages** (a banner that fades) and **conversations** (a box that types out each line, with speaker and portrait; the player can't move while reading). **Signs** to read, and the **Talk** behaviour for characters, with *Later Lines* and *After First Talk* actions |
| Items | `UOnyxScoreSubsystem`, `AOnyxPickup` | Coins and named items (keys, gems) that last across levels; **Key** pickups (*Type: Item*) |
| Game setup | `AOnyxGameMode`, `AOnyxPlayerController` | Works out of the box |
| Settings | `UOnyxGameUserSettings` | Engine settings + master volume, brightness, mute when unfocused |

### UI module (`Onyx2DUI`)
Built entirely in code, so it works with zero assets, and it is themeable in **Project Settings → Plugins → Onyx2D UI**.
- Title screen, pause menu (with blur), settings screen (Display / Graphics / Audio / Controls), confirm dialogs.
- HUD with health pips, dash cooldown, a coin counter and held items that read the player's components automatically, and
  the **Interact prompt** (`E  Read`) over the player when something usable is in reach.
- A **dialogue box** and **message banner** for signs, conversations and messages (`UOnyxDialogueWidget`).
- `UOnyxUISubsystem` shows the right screen per level: title in `MainMenuLevel`, HUD and dialogue box elsewhere, pause on the Pause input.
- Full keyboard, mouse and gamepad navigation; swap any screen for a Blueprint subclass in the settings.

### Editor module (`Onyx2DEditor`) — the **Onyx2D** panel inside Unreal
The tools are built from standard editor widgets (Details panels, toolbars, list views, dockable tabs), so they look and behave like the rest of Unreal.
Open them with the **Onyx2D** button on the main toolbar (its ⋮ dropdown jumps straight to a tool or a quick action) or **Tools → Onyx2D**.
The panel opens in its own window (drag its tab into the editor to dock it). A sidebar on the left lists the tools: click one to open
or focus it, even after closing its tab, or **Reset Layout** to get every tab back. The arrow at the top of the sidebar hides it (a thin strip
with an arrow brings it back). Each tool is an inner tab you can rearrange or tear off,
and the layout is remembered. The console command `Onyx2D.Open <Tool>` (e.g. `Onyx2D.Open Library`) opens a tool directly.

| Tool | What it does |
|---|---|
| **Home** | The start page: the steps from art to a playable level, each one a button (configure project, import, set up level, paint, fill, check and play), the cards other plugins add (`FOnyxEditorExtensions`: the Onyx2D Examples plugin's example levels), and **Extend Onyx2D**: where your code goes, with **New Blueprint** / **New C++ Class** buttons for every Onyx base class (player, enemy, patrol behaviour, pickup, light, hazard, platform, checkpoint, exit, game mode, controller). C++ classes go in your game module, never in the plugin. |
| **Library** | Everything Onyx-ready in the project, with thumbnails: characters, enemies, objects (Blueprints and your own C++ classes built on Onyx classes), tile sets, tile maps, movesets, clips and flipbooks. Pick one to see it play (characters: a button per animation); **Place in Level**, **Edit Moveset**, **Paint With It**, **Open** (C++ classes open their source), **Show in Content Browser**. Updates by itself as assets change. |
| **Import** | Drop PNG files or whole folders from the file explorer (or **Add Files / Add Folder**) and Onyx2D builds everything: tile sheets become padded, lit tile sets with collision (tile size, gaps and border detected from the file name, transparent gaps or tile edges); animations become a playable character (frame sequences `run-1.png, run-2.png...`, one strip per animation `idle.png, run.png`, or a sheet with one animation per row: words like *idle, run, jump, fall, attack, hurt, death, crouch, climb* pick the animation, the rest of the name or the folder groups files per character; all frames share one cell with the feet on one line); numbered frames without an animation word become animated sprites; prop sheets are cut into one sprite per prop; backgrounds become textures for the Parallax tab. Every guess is listed (type, name, tile size, Player/Enemy) and can be changed before **Build All**. Scripts can call `UOnyxEditorLibrary::ImportArt`. |
| **Movesets** | The Moveset Editor. Left: the **Moveset**'s clips by role (missing standard roles, like Run or Hurt, are one click away). Middle: a big preview with transport controls (play, step, loop; sounds play while previewing) and the **timeline**: a block per frame as wide as its hold (drag the right edge to change it, drag to reorder, right-click to duplicate / delete) and event **lanes** (right-click to send a Message or add a Sound, Effect, Camera Shake, a Window or your own event at that time; drag to move, snaps to frames), a ruler to scrub (Ctrl + wheel zooms). Right: the clip's loop / frames per second / speed, the selected frame (hold, sprite) and the selected event. **New Clip** opens the sprite sheet slicer (detects the grid; click frames in play order or drag across a run). **New Character** makes a playable character from the moveset with the behaviours and team you pick. Double-clicking a Moveset or a Clip opens it here; everything is undoable. |
| **Tiles** | Details panel for tile sheet, tile size, margin, spacing, collision and the map (size, scale, ground rows/tile). **Create Tile Set** / **Create Tile Map** (places the map and opens it in the Level Painter). The ground tile is outlined on the sheet. |
| **Level Painter** | LDtk-style level editor. Characters, objects and decorations are drawn as they look in the level (their sprites and animations, playing), and every character Blueprint in the project is in the object palette (each shown by the figure itself, cropped from its frame). The **palette** on the left (tiles, each in its own slot, or objects), a big canvas in the middle, and on the right the **Layers** (the tile map box, Decor / Entities / Background object layers, tile layers, and behind them the **image layers**, each with an eye toggle) and an **Inspector**. **Image layers** are the level's parallax background placed as layers of the map: drop a texture on the map (or **+ › Image Layer**) and its middle lands there; drag it to move it, drag a corner to resize it; the palette sets its picture, size, **Repeat** (off = once, where you put it), parallax sideways / up and down, colour and how far its bottom row carries down; right-click to reorder, duplicate or remove. The canvas draws them behind the tiles, inside the map, where they are placed (they move with the map); the toolbar's **Parallax** preview slides them at their own speeds as you pan, as the game's camera would see them from the view's middle. The toolbar goes to icons only in a narrow window; its **Palette** / **Panels** toggles hide the side panels. Everything placed is filed in the World Outliner under the tile map it stands in, by kind (`<Map>/Characters`, `Lights`, `Props`, `Pickups`, `Gameplay`, `Water`; `Level/...` outside the maps), and the folder button in Layers tidies the whole level (`FOnyxLevelTools::OrganizeOutliner`; your own folders are left alone). Tools: **Brush (B)**, **Rectangle (R)**, **Fill (F)**, **Erase (E)**, **Pick (I)**, **Select (S)**, **Terrain (T)** (auto-tiling); brush options **Flip X (X)**, **Flip Y (Y)** and **Random (V)** (each cell a random tile from the selected block); right-drag erases, middle-drag or Space+drag pans, the wheel zooms. Tiles go straight into the Paper2D tile map (with collision on Ground); entities (player start, patrolling foe, coin, health, checkpoint, exit, moving/one-way platform, hazard, light) are real actors placed on the cell, which you can drag around and edit in the Inspector; the canvas only shows the objects that sit on this map. **New Map** asks for a name, size and optional ground row, makes the map from the selected tile set and places it beside the maps already in the level. **Grow or shrink the map from any side, any time:** drag its border on the canvas (cells to add or remove are previewed), use the Inspector's per-side −/+ buttons, or type an exact size (`Onyx2D.ResizeMap` does it from the console). The map actor moves to make room, so painted tiles, objects, water and zones stay where they are in the level. Everything is undoable. |
| **Parallax** | **Add Background** places an `AOnyxParallaxBackground`; its layers are edited right in the panel. **Add Layers** turns every texture selected in the Content Browser into a layer (far to near, with sensible factors and depths). The Level Painter's **Backdrop** panel edits the same actor next to the map. |
| **Level** | **Set Up Level**, **Configure Project** (turns off Lumen, virtual shadow maps, distance fields, motion blur, auto exposure and TAA; makes Onyx Game Mode the default), **Check Level**, Play, Settings, Help. A grid of every Onyx actor: click to place it where the viewport looks, or drag it into the viewport. The **Level Check** lists problems (missing player start, wrong game mode, no lights, actors off the gameplay plane, unlit sprites, exits without a next level...) each with a **Fix** button, plus **Fix All**; double-click selects the actor. |

Unreal integration:
- **Place Actors → Onyx2D** category (and the toolbar's quick Add menu) with all Onyx actors.
- **Content Browser → right-click a texture → Onyx2D**: *Make Clip from Sheet…*, *Create Onyx Tile Set…* (opens the tool pre-filled), *Apply Pixel-Art Settings*. Right-click a tile map → *Paint in Level Painter*.
- Custom icons for the plugin, the tools and every Onyx actor class (Outliner, Content Browser, Place Actors).

Everything is C++. The same builders are also exposed to **Editor Utility Blueprints** through `UOnyxEditorLibrary`
(`CreateCharacterFromSheet`, `CreateTileSet`, `CreateTileMap`, `SetupCurrentLevelFor2D`, ...).

Headless builds (no scripting): `UnrealEditor-Cmd.exe YourProject.uproject -run=OnyxMaterials` rebuilds the plugin
materials (`-missing`: only those missing or out of date); the Onyx2D Examples plugin adds `-run=OnyxExamples` for the examples.
Editor console commands
(`Onyx2D.Edit`, `Onyx2D.Browse`, `Onyx2D.Select`, `Onyx2D.ShowSettings`, `Onyx2D.ShowTab`, `Onyx2D.Play`, `Onyx2D.Later`,
`Onyx2D.Capture`...) drive the editor without the mouse, for demos, tests and docs screenshots. The materials are created by
`FOnyxMaterialBuilder` and re-created automatically when the editor starts if any are missing.

Other editor helpers:
- Imported textures automatically get pixel-art settings (nearest filtering, no mipmaps, uncompressed).
- **Project Settings → Onyx2D → Default Player Class / Default Enemy Class** make a wizard-made character the player, and the enemy spawned by `E` tiles, with no game mode editing.

## Collision

Works like Unity's *Tilemap Collider 2D* + *Composite Collider 2D*, driven by data you paint:

- **Each tile in a tile set has a collision type**: **None**, **Solid**, **One-Way** (jump up through it, Down + Jump drops through), **Hazard** (hurts on touch)
  or **Edge**: it blocks only across some of its sides (`EOnyxTileEdge`: north, east, south, west), a thin wall along each, so from above a cliff's
  rim or a fence stops you crossing it but you walk along it freely (`UOnyxEditorLibrary::SetTileEdges` from C++; the painter's Edge button blocks every side;
  *Edge Thickness* on the tile collision component).
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
- **Onyx Light 2D** components are drawn every frame into a screen-space light buffer, **clipped by 2D shadows** from solid tiles
  (Onyx Tile Collision) and **Onyx Shadow Caster 2D** shapes; light soaks a little into tile surfaces. Each light has:
  - a **Shape**: *Point* (a disc), *Spot* (a cone of *Cone Angle*), *Line* (a glowing tube of *Length*: neon, a strip light, a crack
    of lava) or *Area* (a soft-edged rectangle of *Size*: a window, a lit doorway). *Radius* is the reach, the soft border for a line
    or an area, and its shadows fall from the nearest point of the shape;
  - a **Falloff**: *Smooth*, *Soft* (a long gentle tail: moonlight), *Sharp* (a hot core, a crisp rim: fire, magic) or *Even* (flat with
    a short soft edge: a window patch, a spotlight disc);
  - an optional **Pattern**, a texture that shapes and colours the light (stained glass, window bars, a grate, leaves); it turns
    with the light and shadows still clip it.
  A light's shadow is kept until the light or the walls change, so lamps that stay put cost almost nothing frame to frame.
- The **Onyx Light 2D** actor adds **presets** (torch, candle, lantern, magic, crystal, lava, window, neon, fluorescent, brazier,
  searchlight, hanging lantern, alarm, glowshroom, moon window, stained glass), a glow halo shaped like the light, and
  *Active Time*: *Night* lamps light themselves at dusk and go out at dawn (with the day-night cycle), *Day* the other way round.
- **Light Animation** (on the actor, or add it next to any Onyx Light 2D): brightness that flickers, pulses, **strobes** or
  **stutters** (a dying tube); colour drifting to *Color B* and back; a **sweep** of the light's direction (a searchlight) and a
  **sway** like a lantern on a chain (what is attached to the light swings with it). Nothing ticks when nothing moves.
- **Glowing pixels:** pixels of a lit sprite can shine by themselves, even in the dark (flames, eyes, runes, a blade's edge), without
  a second unlit sprite: `AOnyxCharacter::GlowPixels` / *Glow Threshold*, or `UOnyx2DLibrary::SetSpriteGlow` for props. By default
  the brightest pixels glow; with a **glow mask** (*Glow From Mask*) exactly the pixels you choose do. Past 1 the glow burns: the
  palest glowing pixels go white-hot while the darker ones keep their colour (a blade's white core, its blue edge).
- **Pixel Light:** light that comes from chosen pixels of the art and follows them through every frame, so only a character's
  blade (a torch's flame, a staff's crystal) lights the walls, shaped like it in each pose and turned round with the sprite. In the
  **Moveset Editor**, open the *Pixel Light* section (folded at the bottom of Details): it shows the colours of the frame on screen: pick the blade's, set the *Spread*, then
  **Build Pixel Light** (an `UOnyxPixelLightSet`: the light of every frame, worked out once in the editor, and an **Onyx Pixel
  Light** component on the character) and **Glow Mask** (the same pixels glow on the sprite). In C++: `FOnyxPixelLightBuilder`.
  Its *Halo* adds a tight glow in the air round those pixels, past the sprite's outline and over the character, the way a
  white-hot blade looks through haze. At run time it is one textured quad per character, with no per-pixel work.
  Choosing by colour needs the part to have colours of its own (check the swatches): a spear painted with the hair's colours
  would light the hair too. For such art, tick **Painted mask**: paint a copy of each texture white where it gives light and
  black elsewhere, import it beside the texture as `<texture>_GlowMask`, and the same mask drives the light and the glow.
- In the editor, a selected light draws its reach, cone, line or area, sweep and sway in the viewport.
- `Onyx2D.Lights` lists every 2D light and Pixel Light in play, `Onyx2D.Sprite [name]` what each character shows (clip, frame,
  drawn or not); `Onyx2D.SaveLightBuffer [name]` writes the light buffers (light, own glow, halo) to
  `Saved/OnyxLightBuffers` to see exactly what lights the scene. Light counts and kept shadows are logged under `LogOnyxLighting` at Verbose.
- `Onyx2D.Backdrop <layer name | all> <0 | 1>` hides or shows a parallax background layer while the game runs, to see what
  each one draws (a mist, a far city).
- `Onyx2D.PainterZoom <zoom> [column row]` (editor) zooms the Level Painter's canvas onto a cell, for scripted screenshots.
- `Onyx2D.PainterSelectLayer <name>` and `Onyx2D.PainterAddImage <texture path> [column row]` (editor) select a Level Painter layer and add an image layer, for scripted screenshots.
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
- **A character's own light:** lights marked *Own Glow* (a character's glow, its blade's flashes from Light Flash events) light the
  world around it, while a sprite with a **Light Guard** (`AOnyxCharacter::LightGuard`, 0 to 1) turns them away, takes other light
  a little less and rolls bright light off before it clips to white: a hero carrying light stays readable (a dark cloak, a pale
  mask) against the lit walls. The guard rides in the G-buffer's metallic channel, which 2D lighting doesn't otherwise use.

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
- **Breakable** (`AOnyxBreakable`, also in the palette): a pot or urn to smash; it can sit under water too, and its coins
  fall to the bottom.
- **Swimming and diving** (*Swim* on the movement component; the Moveset plays a *Swim* clip if there is one, else Fall):
  - *Diving in:* the water takes part of the fall and the rest carries the swimmer down, so a dive from higher goes deeper;
    holding Down dives head first, deeper still. Then buoyancy floats it back up (*Entry Speed Kept*, *Dive Drag*, *Dive Time*).
  - *Steering:* any direction, with Move plus Jump (up) and Down, or the gamepad stick. Speeding up and turning answer at once,
    letting go drifts a little; faster along the surface with the head out; idle swimmers drift with *Buoyancy*
    (above 0 floats up, 0 hovers, below 0 sinks).
  - *Strokes and swim dash:* tap Jump for a stroke towards where you steer (up when not steering); Dash under water is a
    burst in any direction.
  - *Getting out:* Jump at the surface jumps out; rising faster than *Breach Speed* through the surface shoots out with
    *Breach Boost* (a swim dash up from deep is a dolphin leap); swimming into a bank at the surface climbs onto it.
  - *Breath* (optional, *Limited Breath*): runs out with the head under, refills fast at the surface, then *Drown Damage*
    every second, so there is time to reach air.
  - *Bubbles:* a burst and a trail when diving in, with strokes and dashes, and breaths now and then; they wobble up and pop
    at the surface. `AOnyxWater::EmitBubbles` makes them from Blueprint too.
  - Events: *On Started / Stopped Swimming*, *On Swim Stroke*, *On Swim Dash*, *On Breached*, *On Climbed Out*,
    *On Breath Low*, *On Drowning*. AI and cutscenes steer with *Set Vertical Input*.
- **Waves:** a spring surface that ripples by itself and splashes when anything crosses it, harder the faster it goes,
  with an optional *Splash Effect* (flipbook) and *Splash Sound*.
- **Effects** (*Wakes*, *Wake Amount*, *Foam Color*, *Silt Color*): a splash sprays droplets, spreads foam on the surface and
  pushes a ring down through the water; anything moving through the water leaves a wake (bubbles, and soft streaks of
  churned water when it is fast: dives, strokes, dashes, sinking rocks); swimming along the surface leaves foam and paddle
  splashes; strokes and swim dashes send a pulse ring; landing on the bottom stirs up sand. All drawn smooth in the
  particle shader. From Blueprint: *Emit Bubbles*, *Emit Ring*, *Emit Foam*, *Emit Silt*.
- **Look:** a smooth shader, not pixels: a gradient from *Shallow Color* to *Deep Color*, drifting caustics and light shafts,
  rising bubbles and an anti-aliased surface line, drawn in front of what is in the water and darkened by the lighting mood.
- In a typed level, `~` is water (a run of `~`, repeated on the rows below for depth). `/Game/Maps/Test_Water` (built by the
  Onyx2D Examples plugin after Sunny Land) is a Sunny Land tile map with a pool, crates that float, a rock that sinks and gems to dive for,
  all placed with the Level Painter, so it opens there ready to edit. The fox starts high over the pool and dives in.

2D levels (with tile maps, a Level Builder or Onyx Lighting) open in the editor's flat **side view** of the gameplay plane,
framed on the level (*Project Settings → Onyx2D → Open Levels In Side View*).

## Interaction

The **Interact** input uses the nearest thing the character touches that implements **Onyx Interactable** (C++ or Blueprint,
on an actor or a component: *Interact*, and optionally *Is Interaction Locked* and *Get Interaction Prompt*): levers, doors,
signs, chests, NPCs. Trigger Zones (*When: Player Presses Interact*), Signs, the Talk behaviour and the Light Switch are ready-made
ones. The HUD shows the key and the prompt word over the player. With nothing to use, Player Control's *On Interact* fires
instead; while a conversation is open, Interact and Jump turn its pages.

## Trigger Zones, actions and dialogue

Drag a **Trigger Zone** out in the Level Painter (Objects tab), choose *When* and add *Actions* in the Details panel: the editor
draws the zone with a summary of what it does. A locked door is a zone with *Needs: Item Key*, a *Move* action on the door and a
"Locked" message in *Otherwise Actions*. Actions run top to bottom, each after its *Delay*; *Dialogue*, and *Move* with
*Wait Until Done*, hold the ones below until they finish. The **Level Check** flags zones with no actions, actions that aren't set
up and items nothing in the level gives. In the game, `Onyx2D.Message <text>` and `Onyx2D.Talk <speaker>: <line> | <line>`
preview the dialogue UI.

## Game styles: side view and top down

Every level is played one of two ways:
- **Side View** (the default): a platformer seen from the side. Gravity, jumps, walls, ledges and swimming; characters face
  left or right.
- **Top Down**: seen from above. Up the screen is north. Characters walk in any direction over the ground, with no gravity or
  jumping, and look down, up and to the sides.

**Project Settings > Onyx2D > Default Game Style** sets it for the project. A level can choose its own: **World Settings >
Asset User Data > Onyx Level > Game Style**, `Onyx2D.GameStyle TopDown` in the editor's console, or
`FOnyxLevelTools::SetGameStyle` from C++. An overworld from above and its dungeons from the side share the same characters:
each one plays the way its level is played. A character's movement can opt out (*Use Level Game Style* off, then its own
*Game Style*).

What changes from above:
- **Movement** (`UOnyxMovementComponent`): walking in eight directions at *Run Speed*, speeding up at *Ground Acceleration*,
  stopping at *Ground Braking*; the dash goes where it is steered; blows throw it across the ground. Its capsule becomes a
  circle at its feet (*Footprint Radius*), so walls stop the feet and the picture can overlap a wall behind it. W / Up and
  S / Down walk up and down (a Top Down key layer replaces W jumping); the stick steers all round.
- **Looking** (`AOnyxCharacter`): `GetDirection` is Down, Up or Side (with the diagonals when the Moveset has them);
  `GetAimVector` is where it strikes and dashes. Clips drawn per direction are played for it: `Run_Down`, `Run_Up`,
  `Run_Side` (drawn looking right, flipped for the left), and `Run_DownSide` / `Run_UpSide` for eight-way art. Each falls back
  to the nearest one drawn, then to the plain role, so side-view art still plays.
- **Combat**: the attack hitbox goes ahead of the feet along the way it looks, turned with it (*Hitbox Offset* X is how far
  ahead); lunges, recoil, clashes and knockback all go across the ground. Up and Down are ways to walk, so there are no up or
  air combos; draw the combo's clips per direction instead (`Attack1_Up`...).
- **Draw order**: props, items, characters and the player share one layer drawn in the order of their feet. Whoever stands
  lower on the screen is in front, so characters walk behind trees and in front of them. Tiles stay behind, the In Front layer
  in front.
- **Enemy AI and Patrol**: foes wander round where they were placed (*Wander Radius*), see the player all round within
  *Sight Range* (walls hide them), walk round walls to reach them, and lunge, charge and shoot in any direction. A pounce or a
  dive becomes a lunge.
- **Water** (`AOnyxWater`): the same box is a pond or a pool seen from above, its corners rounded by *Corner Radius*:
  shallow along the shore, deeper towards the middle (*Deep From Shore*), caustics and glints, a line of foam at the shore.
  Characters wade through the shallows (*Shore Width*, slower by the movement's *Wade Speed Scale*) and swim beyond them:
  the Moveset plays `Swim` / `SwimIdle` per direction (else Run), no attacking while swimming, and characters that can't
  swim stop where it gets deep. Ripples spread round whatever moves in it, going in splashes, and floating objects drift
  with the current.
- **Rain** (`AOnyxRain`): drops fall into the ground, each landing on a spot of its own in the view (short streaks,
  splashes spraying out across the ground).
- **Physics props** (`AOnyxPhysicsProp`): no gravity; pushed by characters, they slide to a stop (*Ground Friction*)
  and never turn.
- **Lights**: unchanged, 2D lights and their shadows work from above (solid tiles shade like walls).
- **Raised ground**: plateaus with cliff faces are painted with tiles: the face (two rows at the block's south side) is
  Solid, the top's rim is made of **Edge** tiles blocking only across its open sides, stairs through the face are walkable.
  Characters climb the stairs and walk the top; the cliff stops them from below and the rim from walking off. Blocks stack
  (a higher block's face stands on the lower one's top, each height on its own colliding layer).
- **See-through props** (`UOnyxSeeThroughSubsystem`): a prop standing in front of the player (lower on the screen) that hides
  part of it, a tree or a house, turns see-through, its art pixels dropping out in an even pattern, and comes back when the
  player walks out from behind it. *Project Settings > Onyx2D > See-Through Props* and *See-Through Amount*; an actor tagged
  `Onyx.NoSeeThrough` never fades.
- **The rest**: the camera follows both ways and looks ahead up and down; projectiles fly straight; coins from foes and pots
  skip across the ground; nothing falls out of the level. Moving and one-way platforms and mirror floors belong to the side
  view.

Solid tiles are walls from above as they are ground from the side, so tile maps, the Level Painter, lighting and the camera
work unchanged. Test commands: `Onyx2D.GoTo <Actor> [X] [Z]`, `Onyx2D.Player`, `Onyx2D.Where <Actor>`, `Onyx2D.Mood Night`,
`Onyx2D.Rain 1`, `Onyx2D.Act Interact` (talk to whoever is near).

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
- **Draw order, no z-fighting:** everything 2D sits on the gameplay plane (Y = 0) and `UOnyxDrawOrderSubsystem` gives every sprite
  its own place: Behind Tiles, Tiles, Props, Items, Characters, Player, In Front, then a slot per actor and the component's
  Translucency Sort Priority. The sprite materials turn it into a depth bias along the view direction (custom primitive data
  slot 7), so nothing moves on screen or in the world. Override per actor with a **Draw Order** component.
- The Onyx camera pins its aspect-ratio axis, so the confiner, framing and 2D light buffer always match what is rendered.

## Examples

The example levels (Starter Game, Sunny Land, Rainy Town, Cathedral, Light Gallery, Grove, the water test and the Learn
Unreal assets) and the free art they are built from live in a separate plugin, **Onyx2D Examples** (`Onyx2DExamples`). Copy
it beside this one (`Plugins/Onyx2DExamples`) to get their **Build** buttons on the Home tab and in the Onyx2D menu; Onyx2D
works without it, and a game never needs it. Its README describes each example.

## Building a game's content in C++

Your game's editor module can build its art, characters and levels the way the examples do, through the builder API in
`Onyx2DEditor/Public` (depend on `Onyx2DEditor`):
- `FOnyxContentKit`: import pictures and numbered frames, strips and flipbooks (also drawn in code, `FOnyxPixelCanvas`), props,
  tile sets, foes with Enemy AI (`MakeFoe`), shared effects (hit spark, dust, bursts, a wisp), coins, a HUD and its game mode
  (`FHudLook` for your own health pip), and placing sprites, lights and coins on a map's pixel grid;
- `FOnyxPaletteSwap`: re-colour pixel art colour for colour (character templates drawn in flat colours per body part);
- `FOnyxOutfit`: dress part-coloured frames (like the character template's) as a person, every clip and direction alike:
  the head becomes hair and a face (eyes the way it looks, turned with the head when the body tumbles), the torso a shirt or
  an open jacket with a belt, each arm a sleeve ending in a hand, each leg trousers ending in a boot, the sword steel with a
  grip, and an outline goes round the figure. Hands and boots are found as the limb pixels furthest from the body, so they
  stay right through rolls and falls. Colours, hair (*Short*, *Long*, *Bald*), hats (*Straw*, *Cap*) and a beard make each
  person; `DressStrip` dresses a whole strip;
- `UOnyxEditorLibrary::CreateCharacterFromStrips` with `FeetRowsFromBottom` for art drawn on one ground line (frames of
  different sizes centred on the same cell keep their feet in every clip), and `bFitToBody` to fit the capsule to the body
  only (weapons, trails and effects left out) and turn every frame about the body's middle, so characters are hit where
  they are seen (`FOnyxPixelCanvas::BodyBounds` finds the body);
- `FOnyxTerrains::CreateFromArt`: an auto-tiling terrain whose tile corners are read from the art (any layout, as long as
  the empty part is one flat colour); `UOnyxTileCollisionComponent::SolidOutset` lets characters stand on a rim drawn around
  the solid tiles;
- `FOnyxTileBuilder`, `FOnyxMapPainter`, `FOnyxLevelTools`, `FOnyxMaterialBuilder`, `FOnyxAssetUtils`.

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
├─ Source/Onyx2DEditor/    editor tools: Moveset Editor, Level Painter, importers, builders
├─ ARCHITECTURE.md         class map, naming rules, folders
├─ Content/Materials/      what Onyx2D draws with: lit sprite, 2D lighting, water, fog, glow (generated from C++)
└─ Resources/              icons
```
The example levels and their art are in the separate Onyx2D Examples plugin.

## Credits and license

Onyx2D is released under the MIT License (see `LICENSE`).
Onyx2D carries no art of its own; the examples' art and its credits are in the Onyx2D Examples plugin.
