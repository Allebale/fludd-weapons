# FLUDD Weapons

A mod for the Super Mario Sunshine PC port: FLUDD fires a Rifle, a Submachine gun or a Bazooka.

- D-pad left and right choose the weapon, R fires.
- D-pad up zooms the Rifle, with a sniper scope.
- The Bazooka's burst throws enemies and fruit, and rocket jumps you in first person while you hold Left Ctrl or Left Shift, as Team Fortress 2's crouch (or always, or never, in the settings).
- Long range, Shadow Mario difficulty and every weapon's numbers are in the mod's settings.
- (Coming Soon) Weapons made for bosses: Proto Piranha and Petey Piranha hit right by the Rifle,
  Submachine gun and Bazooka (a setting, needs the next game update).
- (Coming Soon) The Bazooka's burst cleans a large area of goop (needs the next game update).
- (Coming Soon) How much goop each weapon cleans, up to 10 times the game's own (a setting for each).
- (Coming Soon) The Bazooka's goop area: how far round its burst goop is washed off, apart from its burst size (a setting).
- (Coming Soon) A Bazooka hit on an enemy already in the air pushes it forward, not up (a setting, needs the next game update).
- (Coming Soon) The Bazooka's shot seen flying: a streak of water, or a small or big green water balloon that bursts where it lands (a setting).
- (Coming Soon) Each rocket jump costs Mario 1 health, as in Team Fortress 2 (a setting, on by default).
- (Coming Soon) With rocket jumps on, an enemy the Bazooka defeats drops a heart beside its coin: 1 health back (a setting, on). Mario cries out when a rocket jump hurts him (a setting, on).

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
