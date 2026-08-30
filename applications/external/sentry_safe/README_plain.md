# Sentry safe

**Maintainer**: [**AJ_60**](https://github.com/AJ60)  
**Repository**: [https://github.com/AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532)

---


Plugin exploiting vulnerability to open any **Sentry Safe** or **Master Lock** electronic safes.
Based on the vulnerability described [here](https://github.com/H4ckd4ddy/bypass-sentry-safe)

## 🛠 Usage
1. Launch the **Sentry Safe** plugin.
2. Long-press **OK** to open the help menu (navigate with Up/Down).
3. Place GPIO wires as shown in the help section.
4. On the main interface:
   - Chose your 5-digit code (or use the default).
   - Select code type:
     - P → Primary code
     - S → Secondary code
5. Press **OK** to send payload.
6. The safe should emit a beep and unlock.

You can enter 00000 to clear selected code.