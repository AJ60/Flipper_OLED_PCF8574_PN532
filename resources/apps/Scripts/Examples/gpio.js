let eventLoop = require("event_loop");
let gpio = require("gpio");

// initialize pins (see the pin list at the bottom for what is usable on this board)
let led = gpio.get("pb2"); // MCU PB2: plain GPIO, no ADC/PWM
let led2 = gpio.get("pc0"); // MCU PA7: PWM-capable (TIM17_CH1)
let pot = gpio.get("pa4"); // MCU PA4: ADC-capable (ADC_IN5)
led.init({ direction: "out", outMode: "push_pull" });
pot.init({ direction: "in", inMode: "analog" });

// blink led
print("Commencing blinking (PB2)");
eventLoop.subscribe(eventLoop.timer("periodic", 1000), function (_, _item, led, state) {
    led.write(state);
    return [led, !state];
}, led, true);

// cycle led pwm
print("Commencing PWM (PC0)");
eventLoop.subscribe(eventLoop.timer("periodic", 10), function (_, _item, led2, state) {
    led2.pwmWrite(10000, state);
    return [led2, (state + 1) % 101];
}, led2, 0);

// read potentiometer
print("Reading analog (PA4)");
eventLoop.subscribe(eventLoop.timer("periodic", 1000), function (_, _item, pot) {
    print("PA4 is at", pot.readAnalog(), "mV");
}, pot);

// the program will just exit unless this is here
eventLoop.run();

// possible pins (Flipper-compatible labels; actual MCU wiring on this DIY board)
// "PA7" aka 2   (PB5 = SPI1 MOSI -> blocked from GPIO apps)
// "PA6" aka 3   (PA6 = SPI1 MISO -> blocked)
// "PA4" aka 4   (PA4: GPIO / ADC IN5 / PWM LPTIM2)           <- usable
// "PB3" aka 5   (PB3 = SPI1 SCK -> blocked)
// "PB2" aka 6   (PB2: GPIO only, no ADC/PWM)                 <- usable
// "PC3" aka 7   (PA5 = LF-RFID carrier -> blocked)
// "PA14" aka 10 (SWD clock)
// "PA13" aka 12 (SWD data)
// "PB6" aka 13  (USART1 TX)
// "PB7" aka 14  (USART1 RX)
// "PC1" aka 15  (PB4 = NFC MISO -> blocked)
// "PC0" aka 16  (PA7: GPIO / ADC IN8 / PWM TIM17_CH1 / I2C3 SCL) <- usable
// "PA3" aka 17  (iButton)

// possible modes
// { direction: "out", outMode: "push_pull" }
// { direction: "out", outMode: "open_drain" }
// { direction: "in", inMode: "analog" }
// { direction: "in", inMode: "plain_digital" }
// { direction: "in", inMode: "interrupt", edge: "rising" | "falling" | "both" }
// { direction: "in", inMode: "event", edge: "rising" | "falling" | "both" }
// all variants support an optional `pull` field: undefined, "up" or "down"
