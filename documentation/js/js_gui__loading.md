# Loading GUI view {#js_gui__loading}

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Oled_PCF8574_PN532](https://github.com/AJ60/Oled_PCF8574_PN532)

---



Displays an animated hourglass icon. Suppresses all `navigation` events, making it impossible for the user to exit the view by pressing the BACK key.

<img src="loading.png" width="200" alt="Sample screenshot of the view" />

```js
let eventLoop = require("event_loop");
let gui = require("gui");
let loadingView = require("gui/loading");
```

This module depends on the `gui` module, which in turn depends on the
`event_loop` module, so they **must** be imported in this order. It is also
recommended to conceptualize these modules first before using this one.

# Example
For an example refer to the GUI example.

# View props
This view does not have any props.
