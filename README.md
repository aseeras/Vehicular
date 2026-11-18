# Vehicular Manslaughter

An original, single-player arena vehicular-combat game for Unreal Engine 5 (C++).

Race, ram and run down pedestrians in a walled arena against five AI rivals. There are three ways to win:

- **Race:** finish 3 laps before any rival does.
- **Wreck:** destroy every rival car.
- **Carnage:** score 15,000 points.

You lose if the clock hits zero, your car is wrecked, or a rival finishes the race first.

## Requirements

- Unreal Engine **5.7** or newer, installed from the Epic Games Launcher.
- A C++ toolchain: Visual Studio 2022 with "Game development with C++" on Windows, or Xcode on macOS.

The project file targets UE 5.7. If you have a newer engine, right-click `Vehicular.uproject` and choose
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

You don't need to build a level. When the loaded map has no checkpoints, the game creates its own arena at
startup from engine basic shapes. That arena includes:

- an oval circuit with 8 checkpoint gates
- a broken inner barrier you can cut through
- a central plaza with jump ramps and pillars
- 18 pickups and 60 pedestrians
- sky and lighting, if the map has none

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
| Restart (after match) | Enter         | Back               |

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
| `VMArenaBuilder` | Procedural default arena and lighting |
| `VMHUD` | Canvas HUD (no UMG assets needed) |

## Status and next steps

This is a first playable foundation. It was written without access to an Unreal Editor, so it has **not been
compiled yet**. Expect a few small fixes on the first build, depending on your engine version.

Ideas for next steps:

- sound effects
- particle effects for sparks, smoke and fire
- a front-end menu
- a garage with persistent credits and upgrades between events
- more arenas
