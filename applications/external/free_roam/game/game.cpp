#include "game/game.hpp"
#include "app.hpp"
#include "free_roam_icons.h"
#include "engine/draw.hpp"
#include "engine/game.hpp"
#include "engine/engine.hpp"
#include "game/sprites.hpp"
#include <math.h>

FreeRoamGame::FreeRoamGame()
{
    // nothing to do
}

FreeRoamGame::~FreeRoamGame()
{
    // also nothing to do anymore since I moved the view/timer to the app
}

void FreeRoamGame::updateDraw(Canvas *canvas)
{
    this->drawCurrentView(canvas);

    if (this->player && this->player->shouldLeaveGame())
    {
        this->soundToggle = this->player->getSoundToggle();
        this->vibrationToggle = this->player->getVibrationToggle();
        this->endGame(); // End the game if the player wants to leave
    }
}

void FreeRoamGame::updateInput(InputEvent *event)
{
    if (!event)
    {
        FURI_LOG_E("FreeRoamGame", "updateInput: no event available");
        return;
    }

    this->lastInput = event->key;

    // Only run inputManager when not in an active game to avoid input conflicts
    if (!(this->currentMainview == GameViewGameLocal && this->isGameRunning))
    {
        this->inputManager();
    }
}

uint32_t FreeRoamGame::callbackToSubmenu(void *context)
{
    UNUSED(context);
    return FreeRoamViewSubmenu;
}

void FreeRoamGame::debounceInput()
{
    static uint8_t debounceCounter = 0;
    if (shouldDebounce)
    {
        lastInput = InputKeyMAX;
        debounceCounter++;
        if (debounceCounter < 4)
        {
            return;
        }
        debounceCounter = 0;
        shouldDebounce = false;
        inputHeld = false;
    }
}

bool FreeRoamGame::startGame(Canvas *canvas)
{
    canvas_clear(canvas);
    canvas_draw_str(canvas, 0, 10, "Initializing game...");

    if (isGameRunning || engine)
    {
        FURI_LOG_E("FreeRoamGame", "Game already running, skipping start");
        return true;
    }

    auto drawTemp = std::make_unique<Draw>(canvas);
    if (!drawTemp)
    {
        FURI_LOG_E("FreeRoamGame", "Failed to create Draw object");
        return false;
    }

    // Create the game instance with 3rd person perspective
    auto game = std::make_unique<Game>("Free Roam", Vector(128, 64), drawTemp.get(), ColorBlack, ColorWhite, CAMERA_THIRD_PERSON);
    if (!game)
    {
        FURI_LOG_E("FreeRoamGame", "Failed to create Game object");
        return false;
    }

    // Create the player instance if it doesn't exist
    if (!player)
    {
        player = std::make_unique<Player>();
        if (!player)
        {
            FURI_LOG_E("FreeRoamGame", "Failed to create Player object");
            return false;
        }
    }

    // set sound/vibration toggle states
    player->setSoundToggle(soundToggle);
    player->setVibrationToggle(vibrationToggle);

    canvas_clear(canvas);
    canvas_draw_str(canvas, 0, 10, "Adding levels and player...");

    // add levels and player to the game
    std::unique_ptr<Level> level1 = std::make_unique<Level>("Tutorial", Vector(128, 64), game.get());
    std::unique_ptr<Level> level2 = std::make_unique<Level>("First", Vector(128, 64), game.get());
    std::unique_ptr<Level> level3 = std::make_unique<Level>("Second", Vector(128, 64), game.get());

    level1->entity_add(player.get());
    level2->entity_add(player.get());
    level3->entity_add(player.get());

    // add some 3D sprites
    std::unique_ptr<Entity> tutorialGuard1 = std::make_unique<Sprite>("Tutorial Guard 1", Vector(3, 7), SPRITE_3D_HUMANOID, 1.7f, M_PI / 4, 0.f, Vector(9, 7));
    std::unique_ptr<Entity> tutorialGuard2 = std::make_unique<Sprite>("Tutorial Guard 2", Vector(6, 2), SPRITE_3D_HUMANOID, 1.7f, M_PI / 4, 0.f, Vector(1, 2));
    level1->entity_add(tutorialGuard1.release());
    level1->entity_add(tutorialGuard2.release());

    game->level_add(level1.release());
    game->level_add(level2.release());
    game->level_add(level3.release());

    this->engine = std::make_unique<GameEngine>(game.release(), 240);
    if (!this->engine)
    {
        FURI_LOG_E("FreeRoamGame", "Failed to create GameEngine");
        return false;
    }

    // i think if we store draw object, we can manage its lifetime
    this->draw = std::move(drawTemp);

    canvas_clear(canvas);
    canvas_draw_str(canvas, 0, 10, "Starting game engine...");

    return true;
}

void FreeRoamGame::endGame()
{
    shouldReturnToMenu = true;
    isGameRunning = false;

    this->updateSoundToggle();
    this->updateVibrationToggle();

    if (engine)
    {
        engine->stop();
    }

    if (draw)
    {
        draw.reset();
    }
}

void FreeRoamGame::switchToNextLevel()
{
    if (!isGameRunning || !engine || !engine->getGame())
    {
        FURI_LOG_W("FreeRoamGame", "Cannot switch level - game not running or engine not ready");
        return;
    }

    // Cycle to next level
    currentLevelIndex = (currentLevelIndex + 1) % totalLevels;

    // Switch to the new level using the engine's game instance
    engine->getGame()->level_switch(currentLevelIndex);

    // Force the Player to update its currentDynamicMap on next render
    if (player)
    {
        player->forceMapReload();
    }
}
void FreeRoamGame::switchToLevel(int levelIndex)
{
    if (!isGameRunning || !engine || !engine->getGame())
        return;

    // Ensure the level index is within bounds
    if (levelIndex < 0 || levelIndex >= totalLevels)
    {
        FURI_LOG_E("FreeRoamGame", "Invalid level index: %d", levelIndex);
        return;
    }

    currentLevelIndex = levelIndex;

    // Switch to the specified level using the engine's game instance
    engine->getGame()->level_switch(currentLevelIndex);

    // Force the Player to update its currentDynamicMap on next render
    if (player)
    {
        player->forceMapReload();
    }
}

void FreeRoamGame::updateSoundToggle()
{
    FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
    if (app)
    {
        app->setSoundEnabled(soundToggle == ToggleOn);
    }
}

void FreeRoamGame::updateVibrationToggle()
{
    FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
    if (app)
    {
        app->setVibrationEnabled(vibrationToggle == ToggleOn);
    }
}

void FreeRoamGame::mainViewLobbyMenuInput()
{
    switch (lastInput)
    {
    case InputKeyUp:
        currentLobbyMenuIndex = LobbyMenuLocal; // Switch to local menu
        shouldDebounce = true;
        break;
    case InputKeyDown:
        currentLobbyMenuIndex = LobbyMenuOnline; // Switch to online menu
        shouldDebounce = true;
        break;
    case InputKeyOk:
        currentMainview = GameViewUserInfo;
        this->userRequest(RequestTypeUserInfo);
        userInfoStatus = UserInfoWaiting;
        shouldDebounce = true;
        break;
    case InputKeyBack:
        currentMainview = GameViewTitle; // Switch back to title view
        shouldDebounce = true;
        break;
    default:
        break;
    }
}
void FreeRoamGame::mainViewSystemMenuInput()
{
    if (lastInput == InputKeyBack)
    {
        currentMainview = GameViewTitle; // Switch back to title view
        shouldDebounce = true;
        return;
    }

    if (currentMenuIndex != MenuIndexSettings)
    {
        switch (lastInput)
        {
        case InputKeyBack:
            currentMainview = GameViewTitle;
            shouldDebounce = true;
            return;
        case InputKeyUp:
            if (currentMenuIndex > MenuIndexProfile)
            {
                currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex - 1);
            }
            shouldDebounce = true;
            break;
        case InputKeyDown:
            if (currentMenuIndex < MenuIndexAbout)
            {
                currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex + 1);
            }
            shouldDebounce = true;
            break;
        default:
            break;
        };
    }
    else
    {
        switch (currentSettingsIndex)
        {
        case MenuSettingsMain:
            // back to title, up to profile, down to settings, left to sound
            switch (lastInput)
            {
            case InputKeyBack:
                currentMainview = GameViewTitle;
                shouldDebounce = true;
                return;
            case InputKeyUp:
                if (currentMenuIndex > MenuIndexProfile)
                {
                    currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex - 1);
                }
                shouldDebounce = true;
                break;
            case InputKeyDown:
                if (currentMenuIndex < MenuIndexAbout)
                {
                    currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex + 1);
                }
                shouldDebounce = true;
                break;
            case InputKeyLeft:
                currentSettingsIndex = MenuSettingsSound; // Switch to sound settings
                shouldDebounce = true;
                break;
            default:
                break;
            };
            break;
        case MenuSettingsSound:
            // sound on/off (using OK button), down to vibration, right to MainSettingsMain
            switch (lastInput)
            {
            case InputKeyOk:
            {
                // Toggle sound on/off
                soundToggle = soundToggle == ToggleOn ? ToggleOff : ToggleOn;
                shouldDebounce = true;
            }
            break;
            case InputKeyRight:
                currentSettingsIndex = MenuSettingsMain; // Switch back to main settings
                shouldDebounce = true;
                break;
            case InputKeyDown:
                currentSettingsIndex = MenuSettingsVibration; // Switch to vibration settings
                shouldDebounce = true;
                break;
            default:
                break;
            };
            break;
        case MenuSettingsVibration:
            // vibration on/off (using OK button), up to sound, right to MainSettingsMain, down to leave game
            switch (lastInput)
            {
            case InputKeyOk:
            {
                // Toggle vibration on/off
                vibrationToggle = vibrationToggle == ToggleOn ? ToggleOff : ToggleOn;
                shouldDebounce = true;
            }
            break;
            case InputKeyRight:
                currentSettingsIndex = MenuSettingsMain; // Switch back to main settings
                shouldDebounce = true;
                break;
            case InputKeyUp:
                currentSettingsIndex = MenuSettingsSound; // Switch to sound settings
                shouldDebounce = true;
                break;
            case InputKeyDown:
                currentSettingsIndex = MenuSettingsLeave; // Switch to leave game settings
                shouldDebounce = true;
                break;
            default:
                break;
            };
            break;
        case MenuSettingsLeave:
            // leave game (using OK button), up to vibration, right to MainSettingsMain
            switch (lastInput)
            {
            case InputKeyOk:
                // Leave game
                this->endGame();
                shouldDebounce = true;
                break;
            case InputKeyRight:
                currentSettingsIndex = MenuSettingsMain; // Switch back to main settings
                shouldDebounce = true;
                break;
            case InputKeyUp:
                currentSettingsIndex = MenuSettingsVibration; // Switch to vibration settings
                shouldDebounce = true;
                break;
            default:
                break;
            };
            break;
        default:
            break;
        };
    }

    // Handle input held events for OK button
    if (lastInput == InputKeyOk)
    {
        switch (currentSettingsIndex)
        {
        case MenuSettingsSound:
            // Toggle sound on/off
            soundToggle = soundToggle == ToggleOn ? ToggleOff : ToggleOn;
            shouldDebounce = true;
            {
                this->updateSoundToggle();
            }
            break;
        case MenuSettingsVibration:
            // Toggle vibration on/off
            vibrationToggle = vibrationToggle == ToggleOn ? ToggleOff : ToggleOn;
            shouldDebounce = true;
            {
                this->updateVibrationToggle();
            }
            break;
        case MenuSettingsLeave:
            this->endGame();
            shouldDebounce = true;
            break;
        default:
            break;
        }
    }
}
void FreeRoamGame::mainViewTitleInput()
{
    switch (lastInput)
    {
    case InputKeyUp:
        currentTitleIndex = TitleIndexStart;
        shouldDebounce = true;
        break;
    case InputKeyDown:
        currentTitleIndex = TitleIndexMenu;
        shouldDebounce = true;
        break;
    case InputKeyBack:
        this->endGame();
        shouldDebounce = true;
        return;
    case InputKeyOk:
        if (currentTitleIndex == TitleIndexStart)
        {
            currentMainview = GameViewLobbyMenu; // Switch to lobby menu view
        }
        else if (currentTitleIndex == TitleIndexMenu)
        {
            currentMainview = GameViewSystemMenu; // Switch to system menu view
        }
        shouldDebounce = true;
        break;
    default:
        break;
    };
}
void FreeRoamGame::mainViewGameLocalInput()
{
    // nothing to do here (for now bro)
    // handling is now done in drawGameLocalView() to avoid conflicts
    // with game engine input processing
}
void FreeRoamGame::mainViewGameOnlineInput()
{
    // Handle input for online game view
    if (lastInput == InputKeyBack)
    {
        currentMainview = GameViewLobbyMenu; // Switch back to lobby menu
        shouldDebounce = true;
    }
}
void FreeRoamGame::mainViewWelcomeInput()
{
    // Handle input for welcome view
    if (lastInput == InputKeyOk)
    {
        if (loginStatus != LoginSuccess)
        {
            currentMainview = GameViewLogin; // Switch to login view
            this->userRequest(RequestTypeLogin);
            loginStatus = LoginWaiting;
        }
        else
        {
            currentMainview = GameViewTitle; // Switch to title view
        }
        shouldDebounce = true;
    }
    else if (lastInput == InputKeyBack)
    {
        endGame(); // End the game and return to the submenu
        shouldDebounce = true;
    }
}
void FreeRoamGame::mainViewLoginInput()
{
    // Handle input for login view
    // this is only a loading/default view for
    // logging in the user
    if (lastInput == InputKeyBack)
    {
        currentMainview = GameViewWelcome; // Switch back to welcome view
        shouldDebounce = true;
    }
}
void FreeRoamGame::mainViewRegistrationInput()
{
    // Handle input for registration view
    // this is only a loading/default view for if
    // the login fails and user does not exist
    if (lastInput == InputKeyBack)
    {
        currentMainview = GameViewWelcome; // Switch back to welcome view
        shouldDebounce = true;
    }
}
void FreeRoamGame::mainViewUserInfoInput()
{
    // Handle input for user info view
    // like the registration view, this is only a loading/default view
    // for loading user info
    if (lastInput == InputKeyBack)
    {
        currentMainview = GameViewWelcome; // Switch back to welcome view
        shouldDebounce = true;
    }
}
void FreeRoamGame::inputManager()
{
    static int inputHeldCounter = 0;

    // Track input held state
    if (lastInput != InputKeyMAX)
    {
        inputHeldCounter++;
        if (inputHeldCounter > 10)
        {
            this->inputHeld = true;
        }
    }
    else
    {
        inputHeldCounter = 0;
        this->inputHeld = false;
    }

    // Handle input for all views, but don't debounce when game engine is running
    if (currentMainview == GameViewGameLocal || currentMainview == GameViewGameOnline)
    {
        // When game is running, completely skip debouncing to avoid interference
        if (!isGameRunning)
        {
            // Only debounce if game is not actually running (e.g., during startup)
            if (lastInput == InputKeyBack)
            {
                debounceInput();
            }
        }
        // When game is running, let all inputs pass through without any debouncing
    }
    else
    {
        // For menu views, debounce all inputs
        debounceInput();
    }

    // Process input for all views
    switch (currentMainview)
    {
    case GameViewSystemMenu:
        mainViewSystemMenuInput();
        break;
    case GameViewLobbyMenu:
        mainViewLobbyMenuInput();
        break;
    case GameViewTitle:
        mainViewTitleInput();
        break;
    case GameViewGameLocal:
        mainViewGameLocalInput();
        break;
    case GameViewGameOnline:
        mainViewGameOnlineInput();
        break;
    case GameViewWelcome:
        mainViewWelcomeInput();
        break;
    case GameViewLogin:
        mainViewLoginInput();
        break;
    case GameViewRegistration:
        mainViewRegistrationInput();
        break;
    case GameViewUserInfo:
        mainViewUserInfoInput();
        break;
    default:
        break;
    }
}

void FreeRoamGame::drawCurrentView(Canvas *canvas)
{
    switch (currentMainview)
    {
    case GameViewTitle:
        drawTitleView(canvas);
        break;
    case GameViewSystemMenu:
        drawSystemMenuView(canvas);
        break;
    case GameViewLobbyMenu:
        drawLobbyMenuView(canvas);
        break;
    case GameViewGameLocal:
        drawGameLocalView(canvas);
        break;
    case GameViewGameOnline:
        drawGameOnlineView(canvas);
        break;
    case GameViewWelcome:
        drawWelcomeView(canvas);
        break;
    case GameViewLogin:
        drawLoginView(canvas);
        break;
    case GameViewRegistration:
        drawRegistrationView(canvas);
        break;
    case GameViewUserInfo:
        drawUserInfoView(canvas);
        break;
    default:
        canvas_clear(canvas);
        canvas_draw_str(canvas, 0, 10, "Unknown View");
        break;
    }
}

void FreeRoamGame::drawRainEffect(Canvas *canvas)
{
    // rain droplets/star droplets effect
    for (int i = 0; i < 8; i++)
    {
        // Use pseudo-random offsets based on frame and droplet index
        uint8_t seed = (rainFrame + i * 37) & 0xFF;
        uint8_t x = (rainFrame + seed * 13) & 0x7F;
        uint8_t y = (rainFrame * 2 + seed * 7 + i * 23) & 0x3F;

        // Draw star-like droplet
        canvas_draw_dot(canvas, x, y);
        canvas_draw_dot(canvas, x - 1, y);
        canvas_draw_dot(canvas, x + 1, y);
        canvas_draw_dot(canvas, x, y - 1);
        canvas_draw_dot(canvas, x, y + 1);
    }

    rainFrame += 1;
    if (rainFrame > 128)
    {
        rainFrame = 0;
    }
}

void FreeRoamGame::drawTitleView(Canvas *canvas)
{
    canvas_clear(canvas);

    // rain effect
    drawRainEffect(canvas);

    // draw title text
    if (currentTitleIndex == TitleIndexStart)
    {
        canvas_draw_box(canvas, 36, 16, 56, 16);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 54, 27, "Start");
        canvas_draw_box(canvas, 36, 32, 56, 16);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 54, 42, "Menu");
    }
    else if (currentTitleIndex == TitleIndexMenu)
    {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 36, 16, 56, 16);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 54, 27, "Start");
        canvas_draw_box(canvas, 36, 32, 56, 16);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 54, 42, "Menu");
        canvas_set_color(canvas, ColorBlack);
    }
}

void FreeRoamGame::drawSystemMenuView(Canvas *canvas)
{
    canvas_clear(canvas);
    canvas_draw_icon(canvas, 0, 0, &I_icon_menu_128x64px);

    switch (currentMenuIndex)
    {
    case MenuIndexProfile:
    {
        // draw info
        char health[32];
        char xp[32];
        char level[32];
        char strength[32];

        snprintf(level, sizeof(level), "Level   : %d", 0);
        snprintf(health, sizeof(health), "Health  : %d", 0);
        snprintf(xp, sizeof(xp), "XP      : %d", 0);
        snprintf(strength, sizeof(strength), "Strength: %d", 0);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 6, 16, "Unknown");
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        canvas_draw_str(canvas, 6, 30, level);
        canvas_draw_str(canvas, 6, 37, health);
        canvas_draw_str(canvas, 6, 44, xp);
        canvas_draw_str(canvas, 6, 51, strength);

        // draw a box around the selected option
        canvas_draw_frame(canvas, 76, 6, 46, 46);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 80, 18, "Profile");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 80, 32, "Settings");
        canvas_draw_str(canvas, 80, 46, "About");
    }
    break;
    case MenuIndexSettings: // sound on/off, vibration on/off, and leave game
    {
        char soundStatus[16];
        char vibrationStatus[16];
        snprintf(soundStatus, sizeof(soundStatus), "Sound: %s", toggleToString(soundToggle));
        snprintf(vibrationStatus, sizeof(vibrationStatus), "Vibrate: %s", toggleToString(vibrationToggle));
        // draw settings info
        switch (currentSettingsIndex)
        {
        case MenuSettingsMain:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 6, 16, "Settings");
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            canvas_draw_str(canvas, 6, 30, soundStatus);
            canvas_draw_str(canvas, 6, 40, vibrationStatus);
            canvas_draw_str(canvas, 6, 50, "Leave Game");
            break;
        case MenuSettingsSound:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 6, 16, "Settings");
            canvas_set_font_custom(canvas, FONT_SIZE_LARGE);
            canvas_draw_str(canvas, 6, 30, soundStatus);
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            canvas_draw_str(canvas, 6, 40, vibrationStatus);
            canvas_draw_str(canvas, 6, 50, "Leave Game");
            break;
        case MenuSettingsVibration:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 6, 16, "Settings");
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            canvas_draw_str(canvas, 6, 30, soundStatus);
            canvas_set_font_custom(canvas, FONT_SIZE_LARGE);
            canvas_draw_str(canvas, 6, 40, vibrationStatus);
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            canvas_draw_str(canvas, 6, 50, "Leave Game");
            break;
        case MenuSettingsLeave:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 6, 16, "Settings");
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            canvas_draw_str(canvas, 6, 30, soundStatus);
            canvas_draw_str(canvas, 6, 40, vibrationStatus);
            canvas_set_font_custom(canvas, FONT_SIZE_LARGE);
            canvas_draw_str(canvas, 6, 50, "Leave Game");
            break;
        default:
            break;
        };
        // draw a box around the selected option
        canvas_draw_frame(canvas, 76, 6, 46, 46);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 80, 18, "Profile");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 79, 32, "Settings");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 80, 46, "About");
    }
    break;
    case MenuIndexAbout:
    {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 6, 16, "Free Roam");
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        canvas_draw_str_multi(canvas, 6, 25, "Creator: JBlanked");
        canvas_draw_str(canvas, 6, 59, "www.github.com/jblanked");

        // draw a box around the selected option
        canvas_draw_frame(canvas, 76, 6, 46, 46);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 80, 18, "Profile");
        canvas_draw_str(canvas, 80, 32, "Settings");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 80, 46, "About");
    }
    break;
    default:
        canvas_clear(canvas);
        canvas_draw_str(canvas, 0, 10, "Unknown Menu");
        break;
    };
}

void FreeRoamGame::drawLobbyMenuView(Canvas *canvas)
{
    canvas_clear(canvas);

    // rain effect
    drawRainEffect(canvas);

    // draw lobby text
    if (currentLobbyMenuIndex == LobbyMenuLocal)
    {
        canvas_draw_box(canvas, 36, 16, 56, 16);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 54, 27, "Local");
        canvas_draw_box(canvas, 36, 32, 56, 16);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 54, 42, "Online");
    }
    else if (currentLobbyMenuIndex == LobbyMenuOnline)
    {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 36, 16, 56, 16);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 54, 27, "Local");
        canvas_draw_box(canvas, 36, 32, 56, 16);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 54, 42, "Online");
        canvas_set_color(canvas, ColorBlack);
    }
}

void FreeRoamGame::drawGameLocalView(Canvas *canvas)
{
    if (this->isGameRunning)
    {
        if (engine)
        {
            if (player && player->shouldLeaveGame())
            {
                this->endGame();
                return;
            }
            engine->updateGameInput(lastInput);
            lastInput = InputKeyMAX; // Reset after processing
            engine->runAsync(false); // Run the game engine
        }
        return;
    }
    else
    {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 25, 32, "Starting Game...");
        isGameRunning = startGame(canvas);
        if (isGameRunning && engine)
        {
            engine->runAsync(false); // Run the game engine immediately
        }
    }
}
void FreeRoamGame::drawGameOnlineView(Canvas *canvas)
{
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "Not available yet");
}

void FreeRoamGame::drawWelcomeView(Canvas *canvas)
{
    canvas_clear(canvas);

    // rain effect
    drawRainEffect(canvas);

    // Draw welcome text with blinking effect
    // Blink every 15 frames (show for 15, hide for 15)
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    if ((welcomeFrame / 15) % 2 == 0)
    {
        canvas_draw_str(canvas, 34, 60, "Press OK to start");
    }
    welcomeFrame++;

    // Reset frame counter to prevent overflow
    if (welcomeFrame >= 30)
    {
        welcomeFrame = 0;
    }

    // Draw a box around the OK button
    canvas_draw_box(canvas, 40, 25, 56, 16);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 56, 35, "Welcome");
    canvas_set_color(canvas, ColorBlack);
}
void FreeRoamGame::drawLoginView(Canvas *canvas)
{
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch (loginStatus)
    {
    case LoginWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Logging in...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            char response[256];
            FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
            if (app && app->load_char("login", response, sizeof(response)))
            {
                if (strstr(response, "[SUCCESS]") != NULL)
                {
                    loginStatus = LoginSuccess;
                    currentMainview = GameViewTitle; // switch to title view
                }
                else if (strstr(response, "User not found") != NULL)
                {
                    loginStatus = LoginNotStarted;
                    currentMainview = GameViewRegistration;
                    this->userRequest(RequestTypeRegistration);
                    registrationStatus = RegistrationWaiting;
                }
                else
                {
                    loginStatus = LoginRequestError;
                }
            }
            else
            {
                loginStatus = LoginRequestError;
            }
        }
        break;
    case LoginSuccess:
        canvas_draw_str(canvas, 0, 10, "Login successful!");
        canvas_draw_str(canvas, 0, 20, "Press OK to continue.");
        break;
    case LoginCredentialsMissing:
        canvas_draw_str(canvas, 0, 10, "Missing credentials!");
        canvas_draw_str(canvas, 0, 20, "Please set your username");
        canvas_draw_str(canvas, 0, 30, "and password in the app.");
        break;
    case LoginRequestError:
        canvas_draw_str(canvas, 0, 10, "Login request failed!");
        canvas_draw_str(canvas, 0, 20, "Check your network and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Logging in...");
        break;
    }
}

void FreeRoamGame::drawRegistrationView(Canvas *canvas)
{
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch (registrationStatus)
    {
    case RegistrationWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Registering...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            char response[256];
            FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
            if (app && app->load_char("register", response, sizeof(response)))
            {
                if (strstr(response, "[SUCCESS]") != NULL)
                {
                    registrationStatus = RegistrationSuccess;
                    currentMainview = GameViewTitle; // switch to title view
                }
                else if (strstr(response, "Username or password not provided") != NULL)
                {
                    registrationStatus = RegistrationCredentialsMissing;
                }
                else if (strstr(response, "User already exists") != NULL)
                {
                    registrationStatus = RegistrationUserExists;
                }
                else
                {
                    registrationStatus = RegistrationRequestError;
                }
            }
            else
            {
                registrationStatus = RegistrationRequestError;
            }
        }
        break;
    case RegistrationSuccess:
        canvas_draw_str(canvas, 0, 10, "Registration successful!");
        canvas_draw_str(canvas, 0, 20, "Press OK to continue.");
        break;
    case RegistrationCredentialsMissing:
        canvas_draw_str(canvas, 0, 10, "Missing credentials!");
        canvas_draw_str(canvas, 0, 20, "Please update your username");
        canvas_draw_str(canvas, 0, 30, "and password in the settings.");
        break;
    case RegistrationRequestError:
        canvas_draw_str(canvas, 0, 10, "Registration request failed!");
        canvas_draw_str(canvas, 0, 20, "Check your network and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Registering...");
        break;
    }
}

void FreeRoamGame::drawUserInfoView(Canvas *canvas)
{
    static bool loadingStarted = false;
    switch (userInfoStatus)
    {
    case UserInfoWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Fetching...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            canvas_draw_str(canvas, 0, 10, "Loading user info...");
            canvas_draw_str(canvas, 0, 20, "Please wait...");
            canvas_draw_str(canvas, 0, 30, "It may take up to 15 seconds.");
            char response[512];
            FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
            if (app && app->load_char("user_info", response, sizeof(response)))
            {
                userInfoStatus = UserInfoSuccess;
                // they're in! let's go
                char *game_stats = get_json_value("game_stats", response);
                if (!game_stats)
                {
                    FURI_LOG_E(TAG, "Failed to parse game_stats");
                    userInfoStatus = UserInfoParseError;
                    if (loading)
                    {
                        loading->stop();
                    }
                    loadingStarted = false;
                    return;
                }
                canvas_clear(canvas);
                canvas_draw_str(canvas, 0, 10, "User info loaded!");
                char *username = get_json_value("username", game_stats);
                char *level = get_json_value("level", game_stats);
                char *xp = get_json_value("xp", game_stats);
                char *health = get_json_value("health", game_stats);
                char *strength = get_json_value("strength", game_stats);
                char *max_health = get_json_value("max_health", game_stats);
                if (!username || !level || !xp || !health || !strength || !max_health)
                {
                    FURI_LOG_E(TAG, "Failed to parse user info");
                    userInfoStatus = UserInfoParseError;
                    if (username)
                        ::free(username);
                    if (level)
                        ::free(level);
                    if (xp)
                        ::free(xp);
                    if (health)
                        ::free(health);
                    if (strength)
                        ::free(strength);
                    if (max_health)
                        ::free(max_health);
                    ::free(game_stats);
                    if (loading)
                    {
                        loading->stop();
                    }
                    loadingStarted = false;
                    return;
                }

                canvas_clear(canvas);
                canvas_draw_str(canvas, 0, 10, "User data found!");

                if (!player)
                {
                    player = std::make_unique<Player>();
                    if (!player)
                    {
                        FURI_LOG_E("FreeRoamGame", "Failed to create Player object");
                        userInfoStatus = UserInfoParseError;
                        if (loading)
                        {
                            loading->stop();
                        }
                        loadingStarted = false;
                        return;
                    }
                }

                canvas_clear(canvas);
                canvas_draw_str(canvas, 0, 10, "Player created!");

                // Update player info
                snprintf(player->player_name, sizeof(player->player_name), "%s", username);
                player->name = player->player_name;
                player->level = atoi(level);
                player->xp = atoi(xp);
                player->health = atoi(health);
                player->strength = atoi(strength);
                player->max_health = atoi(max_health);

                canvas_clear(canvas);
                canvas_draw_str(canvas, 0, 10, "Player info updated!");

                // clean em up gang
                ::free(username);
                ::free(level);
                ::free(xp);
                ::free(health);
                ::free(strength);
                ::free(max_health);
                ::free(game_stats);

                canvas_clear(canvas);
                canvas_draw_str(canvas, 0, 10, "Memory freed!");

                if (currentLobbyMenuIndex == LobbyMenuLocal)
                {
                    currentMainview = GameViewGameLocal; // Switch to local game view
                }
                else if (currentLobbyMenuIndex == LobbyMenuOnline)
                {
                    currentMainview = GameViewGameOnline; // Switch to online game view
                }
                if (loading)
                {
                    loading->stop();
                }
                loadingStarted = false;

                canvas_clear(canvas);
                canvas_draw_str(canvas, 0, 10, "User info loaded successfully!");
                canvas_draw_str(canvas, 0, 20, "Please wait...");
                canvas_draw_str(canvas, 0, 30, "Starting game...");
                canvas_draw_str(canvas, 0, 40, "It may take up to 15 seconds.");

                this->isGameRunning = startGame(canvas);
                return;
            }
            else
            {
                userInfoStatus = UserInfoRequestError;
            }
        }
        break;
    case UserInfoSuccess:
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 10, "User info loaded successfully!");
        canvas_draw_str(canvas, 0, 20, "Press OK to continue.");
        break;
    case UserInfoCredentialsMissing:
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 10, "Missing credentials!");
        canvas_draw_str(canvas, 0, 20, "Please update your username");
        canvas_draw_str(canvas, 0, 30, "and password in the settings.");
        break;
    case UserInfoRequestError:
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 10, "User info request failed!");
        canvas_draw_str(canvas, 0, 20, "Check your network and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    case UserInfoParseError:
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 10, "Failed to parse user info!");
        canvas_draw_str(canvas, 0, 20, "Try again...");
        break;
    default:
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 10, "Loading user info...");
        break;
    }
}

HTTPState FreeRoamGame::getHttpState()
{
    FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
    if (app)
    {
        return app->getHttpState();
    }
    return INACTIVE;
}

bool FreeRoamGame::setHttpState(HTTPState state)
{
    FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
    if (app)
    {
        return app->setHttpState(state);
    }
    return false;
}

void FreeRoamGame::userRequest(RequestType requestType)
{
    FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
    if (!app)
    {
        FURI_LOG_E(TAG, "FreeRoamGame::userRequest: App context is null");
        return;
    }
    char *username = (char *)malloc(64);
    char *password = (char *)malloc(64);
    if (!username || !password)
    {
        FURI_LOG_E(TAG, "userRequest: Failed to allocate memory for credentials");
        if (username)
            free(username);
        if (password)
            free(password);
        return;
    }
    bool credentialsLoaded = true;
    if (!app->load_char("user_name", username, 64))
    {
        FURI_LOG_E(TAG, "Failed to load user_name");
        credentialsLoaded = false;
    }
    if (!app->load_char("user_pass", password, 64))
    {
        FURI_LOG_E(TAG, "Failed to load user_pass");
        credentialsLoaded = false;
    }
    if (!credentialsLoaded)
    {
        switch (requestType)
        {
        case RequestTypeLogin:
            loginStatus = LoginCredentialsMissing;
            break;
        case RequestTypeRegistration:
            registrationStatus = RegistrationCredentialsMissing;
            break;
        case RequestTypeUserInfo:
            userInfoStatus = UserInfoCredentialsMissing;
            break;
        }
        free(username);
        free(password);
        return;
    }
    char *payload = (char *)malloc(256);
    if (!payload)
    {
        FURI_LOG_E(TAG, "userRequest: Failed to allocate memory for payload");
        free(username);
        free(password);
        return;
    }
    snprintf(payload, 256, "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);
    switch (requestType)
    {
    case RequestTypeLogin:
        if (!app->httpRequestAsync("login.txt", "https://www.jblanked.com/flipper/api/user/login/", POST, "{\"Content-Type\":\"application/json\"}", payload))
        {
            loginStatus = LoginRequestError;
        }
        break;
    case RequestTypeRegistration:
        if (!app->httpRequestAsync("register.txt", "https://www.jblanked.com/flipper/api/user/register/", POST, "{\"Content-Type\":\"application/json\"}", payload))
        {
            registrationStatus = RegistrationRequestError;
        }
        break;
    case RequestTypeUserInfo:
    {
        char *url = (char *)malloc(128);
        if (!url)
        {
            FURI_LOG_E(TAG, "userRequest: Failed to allocate memory for url");
            userInfoStatus = UserInfoRequestError;
            free(username);
            free(password);
            free(payload);
            return;
        }
        snprintf(url, 128, "https://www.jblanked.com/flipper/api/user/game-stats/%s/", username);
        if (!app->httpRequestAsync("user_info.txt", url, GET, "{\"Content-Type\":\"application/json\"}"))
        {
            userInfoStatus = UserInfoRequestError;
        }
        free(url);
    }
    break;
    default:
        FURI_LOG_E(TAG, "Unknown request type: %d", requestType);
        loginStatus = LoginRequestError;
        registrationStatus = RegistrationRequestError;
        userInfoStatus = UserInfoRequestError;
        free(username);
        free(password);
        free(payload);
        return;
    }
    free(username);
    free(password);
    free(payload);
}

bool FreeRoamGame::init(ViewDispatcher **viewDispatcher, void *appContext)
{
    this->shouldReturnToMenu = false;
    this->viewDispatcherRef = viewDispatcher;
    this->appContext = appContext;

    FreeRoamApp *app = static_cast<FreeRoamApp *>(appContext);
    if (!app)
    {
        FURI_LOG_E(TAG, "FreeRoamGame::init: App context is null");
        return false;
    }

    // set sound/vibration toggle states
    this->soundToggle = app->isSoundEnabled() ? ToggleOn : ToggleOff;
    this->vibrationToggle = app->isVibrationEnabled() ? ToggleOn : ToggleOff;

    if (!app->isBoardConnected())
    {
        FURI_LOG_E(TAG, "FreeRoamGame::init: Board is not connected");
        easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");
        endGame(); // End the game if board is not connected
        return false;
    }

    return true;
}