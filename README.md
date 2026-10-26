# fireworks.inc

Fireworks for any SA-MP gamemode. Players place or throw fireworks, pick the type and colours, light them, and watch them burst into shapes in the sky. There's also an automatic firework show.

- **No plugins:** plain SA-MP objects only (0.3.7 / 0.3.DL).
- **No admin needed:** every player can use every command.
- **Best at night:** the sparks are SA-MP's glowing light objects. They shine in the dark and look faint in daylight.

## Install
1. Put `fireworks.inc` in `pawno/include/`.
2. Add it to your gamemode, after `a_samp`. If you use Pawn.CMD, put it after Pawn.CMD too:
   ```pawn
   #include <a_samp>
   #include <Pawn.CMD>   // optional
   #include <fireworks>
   ```
3. Compile and restart. That's it.

**Your own callbacks keep working.** The include hooks these with the standard ALS method:
- `OnGameModeInit`, `OnGameModeExit`
- `OnObjectMoved`, `OnDialogResponse`, `OnPlayerDisconnect`
- `OnPlayerCommandText` (only without Pawn.CMD)

**Commands:** with Pawn.CMD they're registered as real Pawn.CMD commands. Without it they work through `OnPlayerCommandText`.

## Commands
| Command | What it does |
|---|---|
| `/fireworks` or `/fw` | Menu: type, then colours, then size, then place or throw |
| `/fw <type> <colour> [big] [throw]` | Same without the menu, e.g. `/fw 7 9 big throw` |
| `/fwlist` | Shows the type and colour numbers |
| `/fwlight` | Lights the nearest firework *you* placed (4 s fuse) |
| `/fwshow [seconds]` | Automatic show around you (10–600 s, default 60) with a finale |
| `/fwstop` | Stops the show and clears every firework on the server |

**Placing vs throwing:**
- **Placed** fireworks wait for `/fwlight`.
- **Thrown** fireworks arc forward, land, and launch 2.5 s later.

**Types:** 1 Peony, 2 Chrysanthemum, 3 Ring, 4 Willow, 5 Palm, 6 Crossette, 7 Heart, 8 Star, 9 Glitter, 10 Cake (12 shots), 11 Fountain.

**Colours:** 1 White, 2 Red, 3 Green, 4 Blue, 5 Red & White, 6 Blue & White, 7 Green & White, 8 Red & Blue, 9 Rainbow.

## Settings (optional)
`#define` any of these **before** `#include <fireworks>`:

| Setting | Default | What it does |
|---|---|---|
| `FW_DIALOG_BASE` | 27300 | Dialog ids used (this and the next 3). Change it if it clashes with your dialogs |
| `FW_MAX_PER_PLAYER` | 5 | Unlit fireworks one player can have set up |
| `FW_MAX_PLACED` | 40 | Fireworks on the ground, server-wide |
| `FW_MAX_PARTICLES` | 650 | Cap on live spark objects (SA-MP's limit is 1000 objects in total) |
| `FW_PLAYER_COOLDOWN` | 1500 | ms between placing or throwing |
| `FW_FUSE_TIME` | 4000 | Fuse after `/fwlight` (ms) |
| `FW_THROW_FUSE` | 2500 | Fuse after a thrown firework lands (ms) |
| `FW_LIGHT_RANGE` | 3.0 | How close you must be to `/fwlight` |
| `FW_DRAW_DISTANCE` | 400.0 | How far away the sparks are visible |

## Optional callbacks
Add these to your gamemode if you want to react to fireworks:

```pawn
// Every time a firework bursts in the sky
forward FW_OnFireworkBurst(Float:x, Float:y, Float:z);
public FW_OnFireworkBurst(Float:x, Float:y, Float:z)
{
    return 1;
}

// Someone started /fwshow
forward FW_OnShowStart(playerid, seconds);
public FW_OnShowStart(playerid, seconds)
{
    return 1;
}
```

## Notes
- **Objects:** each burst uses 25–80 plain objects for a few seconds. The particle cap keeps it under SA-MP's 1000-object limit. If your gamemode already uses many non-streamed objects (`CreateObject`), lower `FW_MAX_PARTICLES`. Streamer objects don't count.
- **Bangs:** they're small explosions 40–70 m up, so they can't hurt anyone.
- **Indoors:** fireworks only work outside (interior 0).
- **`/fwstop`:** any player can clear all fireworks. If that gets abused, delete the `/fwstop` block in the include.
- **Warning 239** ("literal array/string passed to a non-const parameter"): newer compilers show this because of SA-MP's own `a_samp.inc`. It's harmless; add `#pragma warning disable 239` at the top of your gamemode to hide it.
