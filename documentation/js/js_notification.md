# Notification module {#js_notification}

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---



```js
let notify = require("notification");
```
## Methods

### success()
"Success" flipper notification message.

**Example**
```js
notify.success();
```

<br>

### error()
"Error" flipper notification message.

**Example**
```js
notify.error();
```

<br>

### blink()
Blink notification LED.

**Parameters**
- Blink color (blue/red/green/yellow/cyan/magenta)
- Blink type (short/long)

**Examples**
```js
notify.blink("red", "short"); // Short blink of red LED
notify.blink("green", "short"); // Long blink of green LED
```