# 🎨 Asset Packs Format & Creation Guide — DIY Flipper Zero (OLED Edition)

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---

## Intro

Asset Packs allow you to load custom Animation and Icon sets without recompiling the firmware or messing with `manifest.txt` files directly. This firmware comes **pre-bundled with 55 curated asset packs** (Pokemon, Cyberpunk 2077, Akira, PipBoy, Rick and Morty, Star Wars, WatchDogs, BMO, and many more). 

See the complete [**Asset Packs Catalog & Attribution Guide**](../../assets/packs/ReadMe.md) for full screenshots, descriptions, and artist credits.

---

## Bundled Distribution Packages

During firmware compilation (`fbt updater_package`), two optimized sets of packages are produced in `dist/f7-C/`:
- **⚡ Firmware Update Packages (`flipper-z-f7-update-<version>.tgz` & `.zip`)**: 
  Includes the core system and default themes (**Momentum** and **WatchDogs**). This keeps the update lean (~2,200 total files instead of 13,000+), allowing qFlipper flashing to finish in **just 15–20 seconds** without SD card write bottlenecks at the 80% mark.
- **💾 Full SD Resources Packages (`flipper-z-f7-resources-<version>.zip` & `.tgz`)**: 
  Contains all **55 pre-compiled community asset packs**, dolphin graphics, and external databases. Users can extract this `.zip` directly to their microSD card root on a PC in **3 seconds**!

---

## How to install Asset Packs?

Installing Asset Packs is straightforward:

1. **All 55 Packs in 3 Seconds (Recommended)**:
   - Download `flipper-z-f7-resources-*.zip` from [Releases](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532/releases) (or grab them from [`mics/Asset-Packs/packs/`](../../mics/Asset-Packs/packs/)).
   - Extract directly to the root of your microSD card on your PC.
   - All 55 packs will be placed in `SD Card/asset_packs/` instantly!

2. **Core Packs via Firmware Flashing**:
   - Flashing `flipper-z-f7-update-*.tgz` in qFlipper will automatically install the core packs (`Momentum` and `WatchDogs`) in 15 seconds.
   - Any community packs already on your SD card will **remain untouched** across future firmware updates!

3. **How to activate a pack**:
   - Open the Settings app (press `Arrow UP` from desktop and select `Asset Packs` / `Interface Settings`).
   - Select your favorite theme and exit. The device will restart and load your custom animations and icons!


---

## How do I make an Asset Pack?

Asset Packs are made of 2 parts: **Anims** and **Icons**.

### Animations

Animations use the standard animation format:

```
SD/
 |-asset_packs/
                |-PackName/
                         |-Icons/
                               |...
                         |-Anims/
                               |-ExampleAnim/
                                           |-frame_0.bm
                                           |-frame_1.bm
                                           |...
                                           |-meta.txt
                               |-AlsoExample/
                                           |-frame_0.bm
                                           |-frame_1.bm
                                           |...
                                           |-meta.txt
                               |...
                               |-manifest.txt
```

`ExampleAnim` and `AlsoExample` are individual animations containing compiled frames as `frame_x.bm`. Each animation has its own `meta.txt` (width, height, frame rate, duration). Next to all animations is `manifest.txt`, defining level, mood constraints, and random weight.

Key differences with the Asset Pack animation system:
- They go in `SD/asset_packs/PackName/Anims` instead of `SD/dolphin`.
- Custom levels are supported up to level 30.

---

### Icons

Icons are compiled for dynamic loading:

#### Static icons
The `.bm` format does not include width/height headers, so static icons use `.bmx` (`[ int32 width ] + [ int32 height ] + [ standard .bm pixel data ]`). This is automatically handled by the packer script.

#### Animated icons
Stored as `.bm` sequences with a `meta` file containing `[ int32 width ] + [ int32 height ] + [ int32 frame_rate ] + [ int32 frame_count ]`.

#### Structure

```
SD/
 |-asset_packs/
                |-PackName/
                         |-Anims/
                               |...
                         |-Icons/
                               |-Animations/
                                           |-Levelup_128x64/
                                                          |-frame_0.bm
                                                          |-frame_1.bm
                                                          |...
                                                          |-meta
                                           |...
                                |-Passport/
                                         |-passport_happy_46x49.bmx
                                         |-passport_128x64.bmx
                                         |...
                                |-RFID/
                                     |-RFIDDolphinReceive_97x61.bmx
                                     |-RFIDDolphinSend_97x61.bmx
                                     |...
                                |...
```

---

### How to make a pack with the packer

All `.bm` and `.bmx` conversions are handled by `scripts/asset_packer.py`.

#### Source Repository & Tooling (`mics/Asset-Packs/`):
The complete source repository for all community packs is archived under [`mics/Asset-Packs/`](../../mics/Asset-Packs/). It contains:
- Raw animation PNGs, icon sprites, and fonts for all 53 community themes.
- Pack generation tooling (`make_pack.py`, `convert.py`, `batch_convert.py`).
- Theme previews and badges.
- `Makefile` to rebuild all asset packs in batch mode.

#### Standalone Asset Packs:
1. Install Python dependencies: `pip3 install Pillow heatshrink2`
2. Create a folder with your pack name: `PackName/Anims` and/or `PackName/Icons`
3. Place source `.png` images and `meta.txt`/`manifest.txt` files inside
4. Run `python scripts/asset_packer.py`
5. Upload the compiled pack to `SD/asset_packs` on your device.

#### Building with Firmware:
- Pre-compiled packs are placed in `assets/packs/<PackName>`
- Run `./fbt updater_package` (or `fbt.cmd --with-updater updater_package` on Windows)
- FBT automatically packs all themes into `${RESOURCES_ROOT}/asset_packs` and generates:
  - `dist/f7-C/flipper-z-f7-update-<version>.tgz`
  - `dist/f7-C/flipper-z-f7-update-<version>.zip`
  - `dist/f7-C/flipper-z-f7-resources-<version>.tgz`
  - `dist/f7-C/flipper-z-f7-resources-<version>.zip`

