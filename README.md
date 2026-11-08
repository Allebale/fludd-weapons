# FLUDD Weapons

A mod for the Super Mario Sunshine PC port: FLUDD fires a Rifle, a Submachine gun or a Bazooka.

- D-pad left and right choose the weapon, R fires.
- D-pad up zooms the Rifle, with a sniper scope.
- The Bazooka's burst throws enemies and fruit, and rocket jumps you in first person while you hold Left Ctrl or Left Shift, as Team Fortress 2's crouch (or always, or never, in the settings).
- Long range, Shadow Mario difficulty and every weapon's numbers are in the mod's settings.
- Weapons made for bosses (a setting): Proto Piranha, Petey Piranha, Big Chain Chomp, Phantamanta
  and Eely-Mouth's teeth take a set number of Rifle, Submachine gun and Bazooka shots.
- The Rifle aims itself at the enemy nearest the middle of the view.
- The Bazooka's burst cleans a large area of goop; how much goop each weapon cleans, and the
  Bazooka's goop area, are settings.
- Friendly characters covered in goop wash clean in 1 Bazooka shot, 2 Rifle shots or 42 Submachine
  gun shots.
- A Bazooka hit on an enemy already in the air pushes it forward, not up (a setting).
- The Bazooka's shot seen flying: a streak of water, or a small or big green water balloon that
  bursts where it lands (a setting).
- Each rocket jump costs Mario 1 health, as in Team Fortress 2, and he cries out (settings, on by
  default). With rocket jumps on, an enemy the Bazooka defeats drops a heart beside its coin: 1
  health back.
- Weapons off on Pinna Park's roller coaster (its own rockets); FLUDD's jet pack under the sea.

The newest features (bosses, goop, the Bazooka's shot, hearts, rocket jump health) need a game
build newer than Chase's beta 2026.11.06; on that one the rest works.

Needs a game build with `api->held_fludd` (Chase's beta 2026.11.06 or newer). Works with the
[First Person mod](https://github.com/Allebale/sms-first-person-mod) for the weapons held in view.

## Install

Unzip the release into the game's `mods` folder.

## Build

```
cmake -S . -B build -A x64 -DSMS_MODS_OUT=C:/path/to/the/game/mods
cmake --build build --config Release
```

The SDK is the copy in `sdk/` (Chase's mod SDK with `api->held_fludd`, from Allebale's SDK fork).
