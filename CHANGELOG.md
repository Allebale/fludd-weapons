# Changelog

## 1.4.1

- **Back in, from the 1.3.1 and 1.3.2 pre-releases** (1.4.0 left them out by mistake): shooter aim in
  third person (hold L: over-the-shoulder camera, crosshair, mouse aim, its settings), Coin Magnet, the
  rocket jump controller button, goop on 3D Grass, and Mario turning and diving properly while aiming.

## 1.4.0

- **Stable game:** built with the final mod SDK 1.8.0, for the stable game (launcher Stable 1.2.0 and newer). No more pre-release, and no rebuild warning.

- **Ice goop:** black goop can turn to ice when another mod asks for it (the Christmas Additions mod
  does): white-blue ice instead of black, just as slippery and fast, and enemies it touches freeze
  solid where they stand, pale blue, before they are defeated. Mods ask through the
  `fludd-weapons.goop` service (`set_ice`).

## 1.3.0 (pre-release)

Needs the launcher's Beta build of the game (Mod API 1.8 beta 2 or newer). On the stable game, use 1.0.3.

### The big change

- **The weapons now live entirely in this mod.** The game no longer has weapons of its own, so
  every weapon's shots, hits, models, sounds and effects are the mod's, built on the mod API's
  general tools (water drops, actors, models, FLUDD's pose).
- **Recommended settings preset:** a Settings preset row, Recommended (every default, from
  Allebale's tested values) or Custom.

### New weapons

- **Fire goop:** burns enemies and characters standing in it, and throws enemies into nearby fire.
- **Electric goop:** shocks enemies and characters standing in it.
- **Black goop:** slippery goop Mario runs fast through, with turning in the air after a jump off it.
- Goop settings: puddle size, water use, goop on walls, burns or shocks before a defeat, hurting
  friendly characters, and goop quality from Very low to Very high (Medium by default).

### Weapons and controls

- Each weapon can be turned off (skipped by the D-pad and its key).
- A keyboard key for each weapon (1 to 9 and 0, or none).
- Split-screen co-op: each player chooses and fires their own weapon, and their shots, pushes and
  goop are their own, seen right in both views.

### Bazooka

- Rocket jumps: strength 130 percent, a key to throw enemies up (height setting), no rocket jump
  damage when a key shot hits an enemy (on), enemies popped in the air, and self-damage can now
  take your last health.
- Air shots defeat enemies (setting, off), fire rate 0.8 s, burst size 130 percent, third person
  push and burst size up to 1000 percent.
- New setting: **Bazooka leaves graffiti symbols for FLUDD in third person** (on).

### Hits and bosses

- Bosses take hits: Proto Piranha, Petey Piranha, Phantamanta, Gooper Blooper's teeth, Chain Chomp
  and Shadow Mario (every Rifle and SMG shot at him hits).
- Rifle and SMG shots give the game's own water hit when they reach their target; FLUDD's own
  spray can defeat enemies (setting).
- Characters' goop, hiding pictures, fruit, balls and graffiti all react to the weapons.

### Fixes

- No shots or weapon models in a stage's opening, cutscenes or talks.
- Weapon models no longer float in the air after a graffiti portal.
- The Rifle's scope is drawn under the HUD.
- No grey water puddles under goop, and less slowdown with goop around.
- Loading a save state goes straight back into play.

## 1.0.3

The Bazooka's burst and push and the Rifle's blob for first and third person, the empty click and
filling up in water, the roller coaster, choosing under the sea.
