# 📡 Sub-GHz Remote Plugin — DIY Flipper Zero (OLED Edition)

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---

Credit to the [SubGHz_Remote](https://github.com/DarkFlippers/SubGHz_Remote) project for the original app and documentation.

> [!NOTE]
> **Map files can now be created and edited directly on the device.**
> Go into the Sub-GHz Remote app and press the **Back** button to open the map editor.

---

### The SubGHz Remote Tool *requires* a custom user map file with `.txt` extension in the `subghz/remote` folder on the SD card.

#### If these files do not exist or are not configured properly, **you will receive an error each time you select an incorrect file**.

## You can add as many `.txt` map files as you want — the file name doesn't matter!


## Incorrect or unconfigured file error

If the `.txt` file has not been properly configured, the following error will appear:

```
Config is incorrect.

Please configure map

Press Back to Exit
```


## Setting up the `subghz/remote/example.txt` file:

```
UP: /ext/subghz/Up.sub
DOWN: /ext/subghz/Down.sub
LEFT: /ext/subghz/Left.sub
RIGHT: /ext/subghz/Right.sub
OK: /ext/subghz/Ok.sub
ULABEL: Up Label
DLABEL: Down Label
LLABEL: Left Label
RLABEL: Right Label
OKLABEL: Ok Label
```

The UP/DOWN/LEFT/RIGHT/OK file locations must point to the `.sub` capture file you want mapped to each direction.

The ULABEL/DLABEL/LLABEL/RLABEL/OKLABEL variables set the display text for each assigned file.

## Example:

```
UP: /ext/subghz/Fan1.sub
DOWN: /ext/subghz/Fan2.sub
LEFT: /ext/subghz/Door.sub
RIGHT: /ext/subghz/Garage3.sub
OK: /ext/subghz/Garage3l.sub
ULABEL: Fan ON
DLABEL: Fan OFF
LLABEL: Doorbell
RLABEL: Garage OPEN
OKLABEL: Garage CLOSE
```

## Notes
* **App Usage**
  - Press a directional button to transmit the assigned capture file.
  - Press the **Back** button to exit the app or open the map editor.

* **SubGHz Remote Map Rules**
  - File paths must not contain spaces or special characters (`-` and `_` are allowed).
  - Labels are limited to **16 characters** to prevent text from overflowing the screen.
