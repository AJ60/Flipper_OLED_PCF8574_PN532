# Flipper zero wiegand plugin

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Oled_PCF8574_PN532](https://github.com/AJ60/Oled_PCF8574_PN532)

---



Add as git submodule: `git submodule add https://gitlab.com/bettse/flipper-wiegand-plugin.git plugin`

Add to your `application.fam`
```
App(
    appid="plugin_wiegand",
    apptype=DIYAppType.PLUGIN,
    entry_point="plugin_wiegand_ep",
    requires=["seader"],
    sources=["plugin/wiegand.c"],
)
```
