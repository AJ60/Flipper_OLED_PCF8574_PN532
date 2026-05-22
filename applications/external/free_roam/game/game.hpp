#pragma once

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <memory>
#include "easy_flipper/easy_flipper.h"
#include "flipper_http/flipper_http.h"
#include "jsmn/jsmn.h"
#include "font/font.h"
#include "engine/engine.hpp"
#include "game/general.hpp"
#include "game/loading.hpp"
#include "game/player.hpp"

typedef enum
{
    GameViewTitle = 0,        // title, start, and menu (menu)
    GameViewSystemMenu = 1,   // profile, settings, about (menu)
    GameViewLobbyMenu = 2,    // local or online (menu)
    GameViewGameLocal = 3,    // game view local (gameplay)
    GameViewGameOnline = 4,   // game view online (gameplay)
    GameViewWelcome = 5,      // welcome view
    GameViewLogin = 6,        // login view
    GameViewRegistration = 7, // registration view
    GameViewUserInfo = 8      // user info view
} GameMainView;

typedef enum
{
    LoginCredentialsMissing = -1, // Credentials missing
    LoginSuccess = 0,             // Login successful
    LoginUserNotFound = 1,        // User not found
    LoginWrongPassword = 2,       // Wrong password
    LoginWaiting = 3,             // Waiting for response
    LoginNotStarted = 4,          // Login not started
    LoginRequestError = 5,        // Request error
} LoginStatus;

typedef enum
{
    RegistrationCredentialsMissing = -1, // Credentials missing
    RegistrationSuccess = 0,             // Registration successful
    RegistrationUserExists = 1,          // User already exists
    RegistrationRequestError = 2,        // Request error
    RegistrationNotStarted = 3,          // Registration not started
    RegistrationWaiting = 4,             // Waiting for response
} RegistrationStatus;

typedef enum
{
    UserInfoCredentialsMissing = -1, // Credentials missing
    UserInfoSuccess = 0,             // User info fetched successfully
    UserInfoRequestError = 1,        // Request error
    UserInfoNotStarted = 2,          // User info request not started
    UserInfoWaiting = 3,             // Waiting for response
    UserInfoParseError = 4,          // Error parsing user info
} UserInfoStatus;

typedef enum
{
    RequestTypeLogin = 0,        // Request login
    RequestTypeRegistration = 1, // Request registration
    RequestTypeUserInfo = 2,     // Request user info
} RequestType;

class FreeRoamApp;
class FreeRoamGame
{
private:
    GameMainView currentMainview = GameViewWelcome;            // current main view of the game
    TitleIndex currentTitleIndex = TitleIndexStart;            // current title index (must be in the GameViewTitle)
    MenuIndex currentMenuIndex = MenuIndexProfile;             // current menu index (must be in the GameViewSystemMenu)
    LobbyMenuIndex currentLobbyMenuIndex = LobbyMenuLocal;     // current lobby menu index (must be in the GameViewLobbyMenu)
    MenuSettingsIndex currentSettingsIndex = MenuSettingsMain; // current settings index (must be in the GameViewSystemMenu in the Settings tab)
    ToggleState soundToggle = ToggleOn;                        // sound toggle state
    ToggleState vibrationToggle = ToggleOn;                    // vibration toggle state

    // helpers
    bool startGame(Canvas *canvas);     // start the actual game
    void switchToNextLevel();           // switch to the next level in the game
    void switchToLevel(int levelIndex); // switch to a specific level by index
    void updateSoundToggle();           // update sound toggle state
    void updateVibrationToggle();       // update vibration toggle state

    // Input methods
    void mainViewLobbyMenuInput();    // handle input in the lobby menu view
    void mainViewSystemMenuInput();   // handle input in the system menu view
    void mainViewTitleInput();        // handle input in the title view
    void mainViewGameLocalInput();    // handle input in the local game view
    void mainViewGameOnlineInput();   // handle input in the online game view
    void mainViewWelcomeInput();      // handle input in the welcome view
    void mainViewLoginInput();        // handle input in the login view
    void mainViewRegistrationInput(); // handle input in the registration view
    void mainViewUserInfoInput();     // handle input in the user info view
    void inputManager();              // manage input across all views

    // Static callback wrappers
    static uint32_t callbackToSubmenu(void *context); // callback to send the user back to the submenu
    static void timerCallback(void *context);         // timer callback (workaround to force view redraw)

    // Drawing methods
    uint8_t rainFrame = 0;                     // frame counter for rain effect
    uint8_t welcomeFrame = 0;                  // frame counter for welcome animation
    void drawRainEffect(Canvas *canvas);       // draw rain effect on the canvas
    void drawTitleView(Canvas *canvas);        // draw the title view
    void drawSystemMenuView(Canvas *canvas);   // draw the system menu view
    void drawCurrentView(Canvas *canvas);      // draw the current view based on the main view state
    void drawLobbyMenuView(Canvas *canvas);    // draw the lobby menu view
    void drawGameLocalView(Canvas *canvas);    // draw the local game view
    void drawGameOnlineView(Canvas *canvas);   // draw the online game view
    void drawWelcomeView(Canvas *canvas);      // draw the welcome view
    void drawLoginView(Canvas *canvas);        // draw the login view
    void drawRegistrationView(Canvas *canvas); // draw the registration view
    void drawUserInfoView(Canvas *canvas);     // draw the user info view

    // Instance variables for state
    InputKey lastInput = InputKeyMAX;
    bool inputHeld = false;
    void debounceInput();
    bool shouldDebounce = false;

    std::unique_ptr<GameEngine> engine; // Engine instance
    std::unique_ptr<Draw> draw;         // Draw instance

    // Level switching functionality
    int currentLevelIndex = 0;                                   // Current level index
    const int totalLevels = 3;                                   // Total number of levels available
    const char *levelNames[3] = {"Tutorial", "First", "Second"}; // Level names for display

    std::unique_ptr<Player> player;   // Player instance
    std::unique_ptr<Loading> loading; // Loading instance for animations
    bool isGameRunning = false;       // Flag to check if the game is running

    // HTTP methods
    LoginStatus loginStatus = LoginNotStarted;                      // Current login status
    RegistrationStatus registrationStatus = RegistrationNotStarted; // Current registration status
    UserInfoStatus userInfoStatus = UserInfoNotStarted;             // Current user info status
    HTTPState getHttpState();
    bool setHttpState(HTTPState state = IDLE) noexcept;
    bool httpRequestIsFinished() { return this->getHttpState() != INACTIVE && this->getHttpState() != RECEIVING; }
    void userRequest(RequestType requestType);

    float atof(const char *nptr) { return (float)strtod(nptr, NULL); }
    float atof_furi(const FuriString *nptr) { return atof(furi_string_get_cstr(nptr)); }
    int atoi(const char *nptr) { return (int)strtol(nptr, NULL, 10); }
    int atoi_furi(const FuriString *nptr) { return atoi(furi_string_get_cstr(nptr)); }

public:
    FreeRoamGame();
    ~FreeRoamGame();

    bool init(ViewDispatcher **viewDispatcher, void *appContext); // initialize the game
    void endGame();                                               // end the game and return to the submenu
    bool isActive() const { return shouldReturnToMenu == false; } // Check if the game is active
    void updateDraw(Canvas *canvas);                              // update and draw the game
    void updateInput(InputEvent *event);                          // update input for the game

    // Public accessors needed for ViewPort callback
    bool shouldReturnToMenu = false; // Flag to signal return to menu
    ViewDispatcher **viewDispatcherRef = nullptr;
    void *appContext = nullptr;
};
