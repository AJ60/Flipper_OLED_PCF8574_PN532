#include "game.hpp"
#include "engine/entity.hpp"

Game::Game(
    const char *name,
    Vector size,
    Draw *draw,
    Color fg_color,
    Color bg_color,
    CameraPerspective perspective,
    void (*start)(),
    void (*stop)())
    : name(name),
      levels{nullptr},
      current_level(nullptr),
      draw(draw),
      input(-1),
      camera(0, 0),
      pos(0, 0),
      old_pos(0, 0),
      size(size),
      is_active(false),
      bg_color(bg_color),
      fg_color(fg_color),
      camera_perspective(perspective),
      _start(start),
      _stop(stop)
{
    for (int i = 0; i < MAX_LEVELS; i++)
    {
        levels[i] = nullptr;
    }
    this->draw = draw;
    this->draw->setFont();
}

Game::~Game()
{

    for (int i = 0; i < MAX_LEVELS; i++)
    {
        if (levels[i] != nullptr)
        {
            delete levels[i];
            levels[i] = nullptr;
        }
    }
}

void Game::clamp(float &value, float min, float max)
{
    if (value < min)
        value = min;
    if (value > max)
        value = max;
}

void Game::level_add(Level *level)
{
    for (int i = 0; i < MAX_LEVELS; i++)
    {
        if (this->levels[i] == nullptr)
        {
            this->levels[i] = level;
            return;
        }
    }
}

void Game::level_remove(Level *level)
{
    for (int i = 0; i < MAX_LEVELS; i++)
    {
        if (this->levels[i] == level)
        {
            this->levels[i] = nullptr;
            delete level;
            return;
        }
    }
}

void Game::level_switch(const char *name)
{
    for (int i = 0; i < MAX_LEVELS; i++)
    {
        if (this->levels[i] && strcmp(this->levels[i]->name, name) == 0)
        {
            // Stop the current level before switching
            if (this->current_level != nullptr)
            {
                this->current_level->stop();
            }

            this->current_level = this->levels[i];
            this->current_level->start();
            return;
        }
    }
}

void Game::level_switch(int index)
{
    if (index < MAX_LEVELS && this->levels[index] != nullptr)
    {
        // Stop the current level before switching
        if (this->current_level != nullptr)
        {
            this->current_level->stop();
        }

        this->current_level = this->levels[index];
        this->current_level->start();
    }
}

void Game::render()
{
    if (this->current_level == nullptr)
    {
        return;
    }

    // render the level with the configured perspective
    this->current_level->render(this, camera_perspective);
}

void Game::start()
{
    if (this->levels[0] == nullptr)
    {
        return;
    }
    this->current_level = this->levels[0];

    // Call the game’s start callback (if any)
    if (this->_start != nullptr)
    {
        this->_start();
    }

    // Start the level
    this->current_level->start();

    // Mark the game as active
    this->is_active = true;
}

void Game::stop()
{
    if (!this->is_active)
        return;

    if (this->_stop != nullptr)
        this->_stop();

    if (this->current_level != nullptr)
        this->current_level->stop();

    this->is_active = false;

    // Clear all levels.
    for (int i = 0; i < MAX_LEVELS; i++)
    {
        delete this->levels[i];
        this->levels[i] = nullptr;
    }

    // Clear the screen.
    this->draw->clear(Vector(0, 0), size, bg_color);
}

void Game::update()
{
    if (!this->is_active || this->current_level == nullptr)
    {
        return;
    }
    // Update the level
    this->current_level->update(this);
}

void Game::setPerspective(CameraPerspective perspective)
{
    camera_perspective = perspective;
}

CameraPerspective Game::getPerspective() const
{
    return camera_perspective;
}
