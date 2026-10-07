# CrossWorldsFix

[**English**](README.md) | [**Русский**](README_RU.md)

**CrossWorldsFix** is an ASI plugin for **Sonic Racing: CrossWorlds** (UE5) that adds support for Ultrawide monitors (21:9, 32:9) and true **Dual-Monitor 2-Player Splitscreen** (AMD Eyefinity / NVIDIA Surround).

---

## Features

### 🎮 Dual-Monitor 2-Player Splitscreen (32:9 / 3840x1080 & higher)
- **Dedicated Monitor Layout**: Player 1 HUD & viewport are rendered on Monitor 1 (left screen), and Player 2 HUD & viewport are rendered on Monitor 2 (right screen).
- **Bezel Divider Removal**: Hides the in-game black vertical dividing bar in 2-player split-screen so it doesn't collide with your physical monitor bezels.
- **Dual Monitor 2P Menus**: Character and machine selection screens (`WBP_Ready_M2`) are split across both screens, so Player 1 configures their racer on Monitor 1 and Player 2 configures on Monitor 2 without elements overlapping the middle monitor bezels.
- **Fixed Aim & Reticle**: Fixes the forward attack item aiming crosshair clipping bug (which previously caused only the right half to be visible).

### 🖥️ Ultrawide & Narrower Support
- Removes pillarboxing / black bars on 21:9, 32:9, 48:9 or narrower aspect ratios.
- Corrects HUD positioning and racing position world-to-screen markers.
- Optional full HUD spanning (`Span Racing HUD`).

---

## Quick Installation

Installation is very simple and takes just 1 step:

1. Download the latest release archive (`CrossWorldsFix-DualMonitor.zip`).
2. Extract the 3 files into the game's executable directory:
   ```
   SonicRacingCrossWorlds\UNION\Binaries\Win64\
   ```
   Files included:
   - `CrossWorldsFix.asi`
   - `CrossWorldsFix.ini`
   - `winmm.dll` (Ultimate ASI Loader x64)

3. Launch the game through Steam!

### Steam Deck / Linux (Proton)
If playing under Proton/Wine:
- Right-click the game in Steam -> **Properties...** -> **Launch Options**
- Add:
  ```bash
  WINEDLLOVERRIDES="winmm=n,b" %command%
  ```

---

## Uninstallation

To completely uninstall the fix, delete the 3 files from `SonicRacingCrossWorlds\UNION\Binaries\Win64\`:
- `CrossWorldsFix.asi`
- `CrossWorldsFix.ini`
- `winmm.dll`

---

## Configuration

Settings can be customized inside [**`CrossWorldsFix.ini`**](CrossWorldsFix.ini):

```ini
[Fix Aspect Ratio]
Enabled = true          ; Removes black bars on Ultrawide / 32:9

[Fix HUD]
Enabled = true          ; Fixes HUD scaling and position markers

[Dual Monitor Splitscreen]
Enabled = true          ; Binds P1 to Monitor 1 and P2 to Monitor 2
HideCenterDividerLine = true ; Hides game's black center line
Mirror2PMenus = true    ; Splits 2P Ready & Setup menus across both monitors
```

---

## Credits
- Special thanks to **Volf** for the original Ultrawide fix base.
- Enhanced with Dual-Monitor Splitscreen & Reticle Fix by **Hugo Fiermein**.
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)
- [safetyhook](https://github.com/cursey/safetyhook)
- [spdlog](https://github.com/gabime/spdlog)
- [inipp](https://github.com/mcmtroffaes/inipp)
- [Dumper-7](https://github.com/Encryqed/Dumper-7)
