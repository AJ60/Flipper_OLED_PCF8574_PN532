## Pico Game Engine

Translated from the latest version in Picoware: https://github.com/jblanked/Picoware/tree/main/src/ArduinoIDE/Picoware/src

I think we may make use of bunni's image loading solution in the future https://github.com/kbembedded/Flipper-Zero-Game-Boy-Pokemon-Trading/blob/8769074c727ef3fa2da83e30daaf1aa964349c7e/src/pokemon_data.c#L355

If I create something similar with the Pico Calc's SD, then I'll implement into both.

What I changed/added:
- 3D sprite support (sprite3d.hpp) and integrated it into Entity and Level class
- Added `operator==` to vector
- Added camera perspective support to 3D sprite
- Added public accessors for entities 
- Added perspective parameter to Game constructor
- Added perspective and camera parameters to Level constructor
- Passed perspective to the Game's render method
- Added `is_visible` method to Entity and added it to the Level's render method