#pragma once
#include "furi.h"
#include "engine/entity.hpp"
#include "engine/game.hpp"
#include "engine/level.hpp"
#include "game/maps.hpp"
#include "game/general.hpp"

class Player : public Entity
{
public:
    Player();
    ~Player() = default;

    bool collisionMapCheck(Vector new_position);
    void forceMapReload() { currentDynamicMap.reset(); }
    uint8_t getCurrentGameState() const noexcept { return gameState; }
    ToggleState getSoundToggle() const noexcept { return soundToggle; }
    ToggleState getVibrationToggle() const noexcept { return vibrationToggle; }
    void handleMenu(Draw *canvas, Game *game);
    void render(Draw *canvas, Game *game) override;
    void setSoundToggle(ToggleState state) { soundToggle = state; }
    void setVibrationToggle(ToggleState state) { vibrationToggle = state; }
    void setGameState(GameState state) { gameState = state; }
    void setShouldDebounce(bool debounce) { shouldDebounce = debounce; }
    bool shouldLeaveGame() const noexcept { return leaveGame == ToggleOn; }
    void update(Game *game) override;

    char player_name[64] = {0}; // Writable buffer for player name

private:
    void debounceInput(Game *game);                      // debounce input to prevent multiple actions from a single press
    void switchLevels(Game *game);                       // switch levels
    bool isPositionSafe(Vector pos);                     // check if a position is safe (not in a wall)
    Vector findSafeSpawnPosition(const char *levelName); // find a safe spawn position for a level
    std::unique_ptr<DynamicMap> currentDynamicMap = nullptr;
    bool hasBeenPositioned = false;                            // Track if player has been positioned to prevent repeated resets
    MenuIndex currentMenuIndex = MenuIndexProfile;             // current menu index (must be in the GameViewSystemMenu)
    MenuSettingsIndex currentSettingsIndex = MenuSettingsMain; // current settings index (must be in the GameViewSystemMenu in the Settings tab)
    ToggleState soundToggle = ToggleOn;                        // sound toggle state
    ToggleState vibrationToggle = ToggleOn;                    // vibration toggle state
    ToggleState leaveGame = ToggleOff;                         // leave game toggle state
    GameState gameState = GameStatePlaying;                    // current game state
    bool shouldDebounce = false;                               // whether to debounce input
    bool inputHeld = false;                                    // whether input is held
    bool justSwitchedLevels = false;                           // whether the player just switched levels
    uint8_t levelSwitchCounter = 0;                            // counter for level switch delay
    bool justStarted = true;                                   // whether the player just started the game
};