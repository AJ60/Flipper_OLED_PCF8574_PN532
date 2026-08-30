# 🎨 Asset Packs Format & Creation Guide — DIY Flipper Zero (OLED Edition)

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Oled_PCF8574_PN532](https://github.com/AJ60/Oled_PCF8574_PN532)

---

## Intro

Asset Packs allow you to load custom Animation and Icon sets without recompiling the firmware or messing with `manifest.txt` files directly. Here you can find info on how to install Asset Packs and also how to make your own.

---

## How to install Asset Packs?

Installing Asset Packs is straightforward:

1. Open qFlipper and navigate to `SD Card` → `asset_packs`; if you do not see this folder, create it yourself.

2. Here (`SD/asset_packs`) is where all Asset Packs are stored. Unzip your packs and upload the folders here.
   You should see `SD/asset_packs/PackName/Anims` and/or `SD/asset_packs/PackName/Icons`.

3. Open the Settings app (from the home screen press `Arrow UP` and select `Asset Packs` / `Interface Settings`) and select the asset pack you want. When you exit, the device will restart and your animations and icons will use the selected pack!

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

#### Standalone Asset Packs:
1. Install Python dependencies: `pip3 install Pillow heatshrink2`
2. Create a folder with your pack name: `PackName/Anims` and/or `PackName/Icons`
3. Place source `.png` images and `meta.txt`/`manifest.txt` files inside
4. Run `python scripts/asset_packer.py`
5. Upload the compiled pack to `SD/asset_packs` on your device.

#### Building with Firmware:
- Place packs in `assets/dolphin/custom`
- Run `./fbt updater_package` to compile firmware with custom assets included.
