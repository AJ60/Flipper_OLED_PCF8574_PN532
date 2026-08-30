# ⚙️ Sub-GHz Frequency Settings — DIY Flipper Zero (OLED Edition)

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---

## How to add new Sub-GHz frequencies

#### CC1101 Frequency range specs: 300–348 MHz, 386–464 MHz, and 778–928 MHz (+ 350 MHz and 467 MHz added to default range)

### From Flipper

You can manage the Sub-GHz frequencies list directly from the device menu:
`Settings → Protocols → Sub-GHz Frequencies`

- **Use Defaults**: whether to include the default frequency list; if yes, your custom frequencies go at the END of the default list
- **Static Freqs**: list used by `Read`, `Read RAW`, and `Frequency Analyzer`
- **Hopper Freqs**: list used by `Read > Config > Hopping: ON`

This menu is a utility for configuring the normal config file. See below for a manual config file guide.


### From config file

Edit the user settings file located on your microSD card — `subghz/assets/setting_user` (remove `.example` from name to activate)

In this file you will find extra frequencies already added.
If you need a custom one, make sure it's not already listed.

### Default frequency list
```
    /* 300 - 348 */
    300000000,
    302757000,
    303000000,
    303875000,
    303900000,
    304250000,
    307000000,
    307500000,
    307800000,
    309000000,
    310000000,
    312000000,
    312100000,
    312200000,
    313000000,
    313850000,
    314000000,
    314350000,
    314980000,
    315000000,
    318000000,
    320000000,
    320150000,
    330000000,
    345000000,
    348000000,
    350000000,

    /* 387 - 464 */
    387000000,
    390000000,
    418000000,
    430000000,
    430500000,
    431000000,
    431500000,
    433075000, /* LPD433 first */
    433220000,
    433420000,
    433657070,
    433889000,
    433920000 | FREQUENCY_FLAG_DEFAULT, /* LPD433 mid */
    434075000,
    434176948,
    434190000,
    434390000,
    434420000,
    434620000,
    434775000, /* LPD433 last channels */
    438900000,
    440175000,
    462750000,
    464000000,
    467750000,

    /* 779 - 928 */
    779000000,
    868350000,
    868400000,
    868460000,
    868800000,
    868950000,
    906400000,
    915000000,
    925000000,
    928000000,
```

### User frequencies added AFTER the default list! Continue until you reach the end of that list.

### To disable the default list and use ONLY user-added frequencies:
Change this line:
`#Add_standard_frequencies: true`
to:
`Add_standard_frequencies: false`

**You need to have custom frequencies added in both lists (main frequency list AND hopping list). Replacing only hopping freqs will not work with that setting set to false — you need to add something to the main list since it will be empty.**

### To add your own frequency to the user list:
Add a new line:
`Frequency: 928000000` — where `928000000` is your frequency. Keep it in that format (9 digits)!

### Hopper frequency list
To add a new frequency to the hopper:
Add a new line: `Hopper_frequency: 345000000`

Keep the hopper list as small as possible, or hopper functionality becomes too slow.
If `#Add_standard_frequencies: true` is not changed, your frequencies will be added after the default ones.

### Default hopper list
```
    315000000,
    433920000,
    434420000,
    868350000,
```
