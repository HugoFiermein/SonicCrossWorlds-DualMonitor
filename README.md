# CrossWorldsFix — Dual-Monitor 2-Player Patch

[**English**](README.md) | [**Русский**](README_RU.md)

**CrossWorldsFix** is an ASI plugin for **Sonic Racing: CrossWorlds** (Unreal Engine 5.4) that enables a true **Dual-Monitor 2-Player Splitscreen** experience using **AMD Eyefinity** or **NVIDIA Surround** (32:9 virtual desktop e.g. 3840x1080, 5120x1440).

Instead of splitting a single monitor in half, each player gets their own dedicated physical monitor with their own camera, HUD, and interface!

---

## Key Features

### 🎮 Dedicated Screen for Each Player
- **Player 1 (Left Monitor)**: Full viewport and isolated Player 1 HUD on the left screen (0% – 50% of 32:9).
- **Player 2 (Right Monitor)**: Full viewport and isolated Player 2 HUD on the right screen (50% – 100% of 32:9).
- **Hides Center Divider**: Removes the game's default black vertical divider bar so it does not collide with your physical monitor bezels.

### 📋 Dual-Monitor Menu Separation
- **2P Character & Vehicle Select (`WBP_Ready_M2`)**: Player 1's racer setup, stats, and gadget windows are centered cleanly on Monitor 1. Player 2's windows are centered cleanly on Monitor 2. No menu elements are cut in half by monitor bezels!
- **Pause Menu (`WBP_PauseMenu_C`)**: Options and player stats are distributed to each player's monitor; pause action buttons appear centered on the screen of the player who paused.
- **Shared Menus**: Class and Course selection screens are cleanly aligned on Monitor 1 rather than being split down the middle by the bezel.

### 🎯 Fixed Forward Attack Aim / Reticle
- Fixes the bug where only the right half of the forward-attack aiming reticle was rendered in split-screen mode due to incorrect world-to-screen coordinate offsets. The reticle is now fully visible and accurately positioned.

---

## Quick Installation

Installation requires just **1 simple step**:

1. Download the latest release: [**`CrossWorldsFix-DualMonitor-v0.0.3.zip`**](https://github.com/HugoFiermein/CrossWorldsFix/releases/latest).
2. Extract the 3 files directly into your game's executable directory:
   ```
   SonicRacingCrossWorlds\UNION\Binaries\Win64\
   ```
   Files included:
   - `CrossWorldsFix.asi`
   - `CrossWorldsFix.ini`
   - `winmm.dll` (Ultimate ASI Loader x64)

3. Enable **AMD Eyefinity** or **NVIDIA Surround** (so your two monitors form a single 32:9 resolution like 3840x1080 or 5120x1440).
4. Launch the game from Steam, select 32:9 fullscreen resolution, and start 2-player split-screen!

### Steam Deck / Linux (Proton)
If running under Wine/Proton:
- Open Steam -> Right-click the game -> **Properties...** -> **Launch Options**
- Add:
  ```bash
  WINEDLLOVERRIDES="winmm=n,b" %command%
  ```

---

## Uninstallation

To completely uninstall the patch, simply delete the 3 files from `SonicRacingCrossWorlds\UNION\Binaries\Win64\`:
- `CrossWorldsFix.asi`
- `CrossWorldsFix.ini`
- `winmm.dll`

The game files remain 100% untouched and original.

---

## Configuration

Settings can be customized in [**`CrossWorldsFix.ini`**](CrossWorldsFix.ini):

```ini
[Dual Monitor Splitscreen]
Enabled = true               ; Enables dedicated 2-player dual-monitor rendering
HideCenterDividerLine = true ; Hides the in-game black center divider line
Mirror2PMenus = true         ; Positions 2P menus cleanly on each player's monitor

[Fix Aspect Ratio]
Enabled = true               ; Unlocks 32:9 ultrawide desktop support (removes black bars)

[Fix HUD]
Enabled = true               ; Corrects HUD scaling and 3D position markers
```

---

## Credits

- **Lyall** — Author of the original [CrossWorldsFix](https://codeberg.org/Lyall/CrossWorldsFix) ASI loader and pattern-scanning foundation.
- **Hugo Fiermein** — Dual-monitor 2-player viewport isolation, dual-screen HUD layout, menu separation, and reticle aiming fix.
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) by ThirteenAG
- [safetyhook](https://github.com/cursey/safetyhook)
- [spdlog](https://github.com/gabime/spdlog)
- [inipp](https://github.com/mcmtroffaes/inipp)
- [Dumper-7](https://github.com/Encryqed/Dumper-7)
