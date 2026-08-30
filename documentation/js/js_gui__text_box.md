# Text box GUI view {#js_gui__text_box}

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---



Displays a scrollable read-only text field.

<img src="text_box.png" width="200" alt="Sample screenshot of the view" />

```js
let eventLoop = require("event_loop");
let gui = require("gui");
let textBoxView = require("gui/text_box");
```

This module depends on the `gui` module, which in turn depends on the
`event_loop` module, so they **must** be imported in this order. It is also
recommended to conceptualize these modules first before using this one.

## Example
For an example, refer to the `gui.js` example script.

## View props

| Prop     | Type    | Description                        |
|----------|---------|------------------------------------|
| `text`   | `string`| Text to show in the text box.                             |
| `font`   | `string`| The font to display the text in (`"text"` or `"hex"`).    |
| `focus`  | `string`| The initial focus of the text box (`"start"` or `"end"`). |
