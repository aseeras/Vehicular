# Vehicular Manslaughter

An original arena vehicular-combat game for Unreal Engine 5 (C++). It has a single-player career and
2–4 player local split-screen.

Race, ram and run down pedestrians in a walled arena against five AI rivals. There are three ways to win:

- **Race:** finish 3 laps before any rival does.
- **Wreck:** destroy every rival car.
- **Carnage:** score 15,000 points.

You lose if the clock hits zero, your car is wrecked, or a rival finishes the race first.

Every event earns **credits** to spend in the **garage** on upgrades. Winning moves you on to the next
event, which has a new arena layout, more rivals with better upgrades, and a higher carnage target.

## Requirements

- Unreal Engine **5.8** or newer, installed from the Epic Games Launcher.
- A C++ toolchain: Visual Studio 2022 with "Game development with C++" on Windows, or Xcode on macOS.

The project file targets UE 5.8. If you have a newer engine, right-click `Vehicular.uproject` and choose
**Switch Unreal Engine Version**.

## First-time setup

1. Clone the repo and right-click `Vehicular.uproject`, then choose **Generate Visual Studio project files**.
2. Open `Vehicular.uproject`. Click **Yes** when it offers to build the `Vehicular` module.
3. **Add a car.** Chaos vehicles need a rigged skeletal mesh, which can't be generated in code. The easiest
   option is Epic's free Vehicle pack:
   - In the Content Browser, click **Add** > **Add Feature or Content Pack** > **Blueprint** > **Vehicle** > **Add to Project**.
   - The game automatically loads `/Game/Vehicles/SportsCar/SKM_SportsCar` and `ABP_SportsCar` from that pack.
   - To use your own car instead, see "Using your own assets" below.
4. Press **Play**.

You don't need to build a level. When the loaded map has no checkpoints, the game builds one of its four
arenas at startup from engine basic shapes, including sky and lighting if the map has none (see
[Arenas](#arenas)).

## Main menu

The game opens on the main menu, with the car slowly circling behind it:

- **Career:** opens the garage for your current event.
- **New Career:** wipes credits, upgrades, cars and progress, after asking you to confirm. Your settings
  are kept.
- **Split-Screen:** 2–4 player local multiplayer (see [Split-screen](#split-screen)).
- **Quick Race:** one race on any arena, with any car you own and a rival difficulty from 1 to 8. It gives
  no credits and no career progress.
- **Settings:**
  - master, effects and engine volume
  - particle amount
  - graphics quality (Low to Epic)
  - window mode (fullscreen, borderless or windowed)
- **Controls:** the full control list.
- **Quit**

During a race, press **P / Start** to open the pause menu: Resume, Restart, or Quit to main menu. In menus,
**Esc / Backspace / B** goes back. In the editor, Esc ends Play-In-Editor, so use Backspace there instead.

## Split-screen

Choose **Split-Screen** on the main menu and set up the match:

- 2, 3 or 4 players
- the arena
- each player's car (every car is available here, owned or not)
- 0–8 AI rivals and their difficulty
- whether P1 uses the keyboard or a gamepad

How the controls are assigned:

- **P1 on keyboard (default):** gamepad 1 drives P2, gamepad 2 drives P3, and so on. Two players need one
  gamepad.
- **P1 on gamepad:** gamepad 1 drives P1, gamepad 2 drives P2, and so on. Every player needs a gamepad.

Rules:

- Everyone starts with no garage upgrades, so it's a level playing field.
- There's no countdown clock.
- You win by finishing the laps first, being the last car standing, or reaching the carnage target first.
- If an AI rival finishes first, or every player is wrecked, nobody wins.

Each player gets their own view with their own HUD: points, combo, position, lap, damage and nitro.
Players have colour-coded roof markers and name tags (P1 blue, P2 red, P3 green, P4 yellow). Pickup
and scoring messages only show on the screen of the player they're for. Rivals chase whichever player
is nearest. The results screen shows a scoreboard for all players.

Split-screen matches don't pay credits or affect your career. After the results, you go back to the
split-screen setup screen.

## Arenas

| Arena      | Layout | Pedestrians |
|------------|--------|-------------|
| The Lot    | Oval circuit, broken inner barrier, central jump plaza with pillars | 60 |
| Salt Flats | Huge figure-8 with a head-on crossroads in the middle, rock spires, big lobe jumps, bright noon sun | 45 |
| Dockyard   | Fast loop round a container-stack maze with two cross-yard shortcuts, gantry cranes, overcast | 55 |
| Downtown   | Ring road around a 4×3 grid of city blocks, with side streets as shortcuts, crowded sidewalks and towers at sunset | 90 |

Career events rotate through the arenas in that order. Each event also gets its own random layout of
props and pickups.

## Building a Mac .dmg

On a Mac with Unreal Engine and Xcode installed, after doing the first-time setup above (including the
Vehicle content pack), run this from the repo root:

```bash
./Scripts/build_mac_dmg.sh
```

The script:

1. finds your newest Unreal Engine install (set `UE_ROOT` to choose a different one)
2. packages a Shipping build for macOS
3. ad-hoc signs it
4. writes `Build/VehicularManslaughter-Mac.dmg`

The disk image contains the app and an Applications shortcut. The app isn't notarized, so on other Macs
you open it the first time with right-click > Open.

## Controls

| Action              | Keyboard        | Gamepad            |
|---------------------|-----------------|--------------------|
| Throttle / reverse  | W / S (or ↑ ↓)  | RT / LT            |
| Steer               | A / D (or ← →)  | Left stick         |
| Handbrake           | Space           | B                  |
| Nitro               | Left Shift      | A                  |
| Use weapon          | E               | X                  |
| Recover (flip back) | R               | Y                  |
| Camera (chase/hood) | C               | Right stick click  |
| Pause               | P               | Start              |
| Select (menus)        | Enter         | A                 |
| Back (menus)          | Esc / Backspace | B               |
| Garage menu up / down | ↑ ↓ or W / S  | D-pad             |
| Browse cars (garage)  | ← → or A / D  | D-pad left / right |

## Gameplay

- **Timer:** you start with 60 s. You gain time for checkpoints (+8 s), pedestrians (+2 s), damaging
  rivals and wrecking them (+15 s). Recovering a flipped car costs 3 s.
- **Damage:** each car has four zones: front, rear, left and right.
  - Front damage cuts engine power and rear damage cuts drive.
  - Side damage costs grip on that side, and a battered car steers worse.
  - Parts fly off at 50% and 0%. The car is wrecked at 25% total health.
  - In a crash between cars, the car doing the ramming takes less damage.
- **Pedestrians:** they wander until a fast car approaches, then panic and run. Hitting one above
  about 13 km/h scores points; repeated hits within 3 s build a combo (up to x10).
- **Pickups:**
  - **Nitro:** refills the boost tank.
  - **Repair:** restores the car fully.
  - **Armour:** reduces damage taken by 60% for 15 s.
  - **Ram Spikes:** doubles the ramming damage you deal for 15 s.
  - **EMP:** a held weapon that stuns and damages nearby cars.
  - **Oil Slick:** a held weapon you drop behind you; it kills the grip of anyone who drives through it.
- **Rivals:** they race the checkpoints and switch to hunting you when you're close. They back off when badly
  damaged, use weapons, boost on straights and recover themselves when flipped. Each one has a coloured
  marker above the car.

## Career and garage

- Choose **Career** on the main menu to open the **garage**, then choose **Start Event** to race.
- After each match, press Enter or A to return to the garage.
- **Credits earned:** your points ÷ 5, plus a win bonus of 1,000 + 250 × (event − 1).
- Progress saves automatically to the `VehicularCareer` save slot. To start over, delete
  `Saved/SaveGames/VehicularCareer.sav`.

| Upgrade        | Effect per level                           |
|----------------|--------------------------------------------|
| Engine         | +10% engine torque                         |
| Tyres          | +6% grip                                   |
| Armour Plating | −8% damage taken                           |
| Nitro Tank     | +20% boost duration, fuller starting tank  |
| Ram Bar        | +12% ramming damage dealt                  |

Each upgrade has 5 levels, costing 400, 900, 1,600, 2,500 and 3,600 credits. Upgrades apply to whichever
car you drive.

### Cars

Select the **Car** row in the garage, then use Left and Right to browse. Press Enter or A to buy a car or
switch to one you own.

| Car            | Unlock                | Character |
|----------------|-----------------------|-----------|
| Street Brawler | Free                  | Balanced all-rounder |
| Wasp           | 1 win, 3,000 cr       | Light and fast, big nitro, fragile |
| The Mule       | 1 win, 4,000 cr       | Heavy truck: tough, hits hard, slow to turn |
| Juggernaut     | 2 wins, 8,000 cr      | Armoured and spiked, ramming monster |
| Hornet X       | 4 wins, 12,000 cr     | Top-tier speed and grip |

Cars differ in mass, power, grip, steering, toughness, ramming strength and nitro. Each one also has its
own bolt-on parts so you can tell them apart:

- bull bars
- a roof rack with lamps
- a spoiler
- a spiked ram
- armour plates
- side exhausts

Rivals drive better cars in later events.

To change the roster or add cars, go to **Project Settings > Game > Vehicular Cars**. Each car can point
to its own `VMVehiclePawn` Blueprint if you want it to have a different 3D model.

Winning moves you to the next event:

- **New arena:** each event has a different layout of pillars and pickups.
- **More rivals:** one extra rival every 2 events, up to 8.
- **Stronger rivals:** rivals carry upgrades up to one level below your event's tier, and the tier rises
  each event up to level 5.
- **Higher target:** the carnage target rises by 2,500 points per event.

## Sound

All sound effects are synthesised in code at runtime, so the project needs no audio files. That includes:

- the engine (pitch follows RPM, and the volume follows the throttle)
- crashes and explosions
- pedestrian yelps and squashes
- pickups, the EMP, oil and nitro
- countdown beeps, checkpoints, the win and lose stings, and garage sounds

To replace any of them with your own assets, go to **Project Settings > Game > Vehicular Audio**. You can
assign a Sound Wave or MetaSound to any effect or to the engine loop, and set the effects and engine
volumes there too.

## Particle effects

Particles are simulated in C++ and drawn with instanced basic shapes, so the project needs no VFX assets.
Effects include:

- sparks on impacts
- engine smoke that grows and turns black as the car is damaged
- fire and a smoke column on wrecked cars
- explosions
- tyre smoke when you slide or handbrake
- nitro exhaust flames
- pedestrian splats
- an EMP shockwave ring
- oil splashes

The particles use the engine's plain shape material, so they're opaque and shrink away instead of fading.
For proper glowing, fading VFX, go to **Project Settings > Game > Vehicular Effects** and assign Niagara
systems to the burst effects: sparks, explosion, splat and EMP. You can also adjust particle density and
the particle limit there.

## Using your own assets

All the gameplay classes can be subclassed in Blueprint:

- **Car:** create a Blueprint subclass of `VMVehiclePawn`. Set its Skeletal Mesh and Anim Class, and make
  the wheel bone names in *Vehicle Movement > Wheel Setups* match your rig.
- **Game mode:** create a Blueprint subclass of `VMGameMode`. Set `Default Pawn Class` and `Rival Class` to
  your car Blueprint, then set it as the default game mode in Project Settings.
  - All the rules (laps, timer, points, bonuses, rival count, pedestrian count) are editable there.
- **Pedestrians:** subclass `VMPedestrian` and set it as `Pedestrian Class`.
- **Custom tracks:** place `VMCheckpoint` actors in your own level, numbered from 0. Index 0 is the
  start/finish line, and gates face the driving direction. If the level has checkpoints, the game uses
  them instead of the built-in arena. The start grid forms just past checkpoint 0.

## Code layout (`Source/Vehicular`)

| File | Purpose |
|------|---------|
| `VMGameMode` | Rules, timer, scoring, combos, race progress, win/lose, spawning |
| `VMVehiclePawn` | Chaos wheeled car, input, impact damage, nitro, pickups, recovery |
| `VMDamageComponent` | Four-zone damage model |
| `VMWheels` | Front/rear wheel tuning |
| `VMRivalAIController` | Rival driving AI (race / hunt / flee) |
| `VMPedestrian` | Wandering and panicking pedestrians, knockdown physics |
| `VMCheckpoint` | Race gates |
| `VMPickup`, `VMOilSlick` | Power-ups and the oil hazard |
| `VMArenaBuilder` | The four procedural arenas, their track shapes and lighting |
| `VMGameModeMenus.cpp` | Main menu, quick race, settings, controls and pause menu |
| `VMHUD` | Canvas HUD and garage screen (no UMG assets needed) |
| `VMProfileSubsystem`, `VMSaveGame` | Career save: credits, upgrades, event progress |
| `VMSoundSubsystem`, `VMSynth` | Procedural sound effects and engine audio |
| `VMAudioSettings` | Project Settings page for replacing sounds |
| `VMEffectsSubsystem`, `VMEffectsSettings` | Procedural particles and the Niagara override settings |
| `VMCarCatalog` | Unlockable car roster and stats |

## Status and next steps

This is a first playable foundation. It was written without access to an Unreal Editor, so it has **not been
compiled yet**. Expect a few small fixes on the first build, depending on your engine version.

Ideas for next steps:

- more arenas
- custom key rebinding
- Niagara versions of the continuous effects (smoke, fire, tyre smoke, nitro flames)
