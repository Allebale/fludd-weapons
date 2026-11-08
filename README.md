# FLUDD Weapons

A mod for the Super Mario Sunshine PC port: FLUDD fires a Rifle, a Submachine gun or a Bazooka.

- D-pad left and right choose the weapon, R fires.
- D-pad up zooms the Rifle, with a sniper scope.
- The Bazooka's burst throws enemies and fruit, and rocket jumps you in first person while you hold Left Ctrl or Left Shift, as Team Fortress 2's crouch (or always, or never, in the settings).
- Long range, Shadow Mario difficulty and every weapon's numbers are in the mod's settings.
- The Bazooka's burst size and push, and the Rifle's blob size, each their own in first and third
  person (and while rocket jumping or zoomed).
- With the tank empty R gives FLUDD's click; in water R fills it up.
- Weapons off on Pinna Park's roller coaster (its own rockets); FLUDD's jet pack under the sea.

More (weapons made for bosses, the Bazooka's shot seen flying, hearts, rocket jump health, goop)
comes with the next game update.

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
