#pragma once
#include "engine/draw.hpp"
#include "engine/level.hpp"
#include "engine/vector.hpp"
#include "engine/entity.hpp"

#define MAX_LEVELS 10

// Forward declaration
class Entity;

class Game
{
public:
    Game(
        const char *name,
        Vector size, // game/world size
        Draw *draw,
        Color fg_color = ColorBlack,                         // 0x01
        Color bg_color = ColorWhite,                         // 0x00
        CameraPerspective perspective = CAMERA_FIRST_PERSON, // Default perspective
        void (*start)() = NULL,
        void (*stop)() = NULL);
    ~Game();
    // Clamp a value between a lower and upper bound.
    void clamp(float &value, float min, float max);
    void level_add(Level *level);                       // Add a level to the game
    void level_remove(Level *level);                    // Remove a level from the game
    void level_switch(const char *name);                // Switch to a level by name
    void level_switch(int index);                       // Switch to a level by index
    void render();                                      // Called every frame to render the game
    void setPerspective(CameraPerspective perspective); // Set camera perspective
    CameraPerspective getPerspective() const;           // Get current camera perspective
    void start();                                       // Called when the game starts
    void stop();                                        // Called when the game stops
    void update();                                      // Called every frame to update the game

    const char *name;                     // Name of the game
    Level *levels[MAX_LEVELS];            // Array of levels
    Level *current_level;                 // Current level
    Draw *draw;                           // Draw object for rendering
    uint8_t input;                        // Last input (e.g., one of the BUTTON_ constants)
    Vector camera;                        // Camera position
    Vector pos;                           // Player position
    Vector old_pos;                       // Previous position
    Vector size;                          // Game/World size
    bool is_active;                       // Whether the game is active
    Color bg_color;                       // Background color
    Color fg_color;                       // Foreground color
    CameraPerspective camera_perspective; // Current camera perspective
private:
    void (*_start)();
    void (*_stop)();
};
