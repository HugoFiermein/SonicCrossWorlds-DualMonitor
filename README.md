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

### 📋 Dual-Monitor Menu & Intro Duplication
- **Menu & Intro Mirroring**: All shared menus (`CourseSelect`, `ClassSelect`, `RivalSelect`, `RivalCutin`, `RivalChoice`) and pre-race intros (`RaceBefore_*`) are mirrored across both monitors so neither player gets a black screen and nothing is sliced in half by monitor bezels!
- **2P Racer Setup (`WBP_Ready_M2`)**: Player 1's racer cards, vehicle parameters, and gadget selections are centered cleanly on Monitor 1. Player 2's are centered on Monitor 2. The "ВПЕРЁД!" (GO!) banner is rendered cleanly on each monitor.
- **Pause Menu (`WBP_PauseMenu_C`)**: Full-screen blur spans both monitors without pillarbox seams; pause dialog and buttons appear centered on the screen of the player who paused, with player stats mapped to each respective screen.

### 🎯 Restored Forward Attack Aim / Reticle
- Completely fixes the aiming reticle in 2-player mode. Bypasses single-player ultrawide offset shifts that displaced the reticle to the screen edge and restores native 1:1 pixel HUD canvas scaling, ensuring the reticle is fully visible and tracks directly in front of the car.

---

## Quick Installation

Installation requires just **1 simple step**:

1. Download the latest release: [**`CrossWorldsFix-DualMonitor-v0.0.4.zip`**](https://github.com/HugoFiermein/CrossWorldsFix/releases/latest).
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

## Credits & Acknowledgments

- **[Lyall](https://codeberg.org/Lyall/CrossWorldsFix)** — Authors of the original CrossWorldsFix mod, memory pattern scanning, and reverse engineering foundation.
- **[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)** by ThirteenAG — ASI plugin loader.
- **[safetyhook](https://github.com/cursey/safetyhook)** by cursey — Memory hooking library.
- **[Dumper-7](https://github.com/Encryqed/Dumper-7)** by Encryqed — Unreal Engine SDK generator.
