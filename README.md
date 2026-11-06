# FLUDD Weapons

A mod for the Super Mario Sunshine PC port: FLUDD fires a Rifle, a Submachine gun or a Bazooka.

- D-pad left and right choose the weapon, R fires.
- D-pad up zooms the Rifle, with a sniper scope.
- The Bazooka's burst throws enemies and fruit, and rocket jumps you in first person while you hold Left Ctrl or Left Shift, as Team Fortress 2's crouch (or always, or never, in the settings).
- Long range, Shadow Mario difficulty and every weapon's numbers are in the mod's settings.

Needs a game build with `api->held_fludd` (Allebale's First Person builds). Works with the
[First Person mod](https://github.com/Allebale/sms-first-person-mod) for the weapons held in view.

## Install

Unzip the release into the game's `mods` folder.

## Build

```
cmake -S . -B build -A x64 -DSMS_MODS_OUT=C:/path/to/the/game/mods
cmake --build build --config Release
```

The SDK is the copy in `sdk/` (Chase's mod SDK with `api->held_fludd`, from Allebale's SDK fork).
