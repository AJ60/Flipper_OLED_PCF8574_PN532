#pragma once

#include <gui/gui.h>
#include "font/font.h"
#include "engine/vector.hpp"

class FreeRoamApp;

class Draw
{
public:
    Canvas *display = nullptr;                                                          // Pointer to the canvas for drawing operations.
    Draw(Canvas *canvas);                                                               // Constructor that initializes the canvas and sets foreground and background colors.
    ~Draw() = default;                                                                  // Default destructor.
    void clear(Vector position, Vector size, Color color = ColorWhite);                 // Clears the display at the specified position and size with the specified color.
    void drawCircle(Vector position, int16_t r, Color color = ColorBlack);              // Draws a circle on the display at the specified position with the specified radius and color.
    void drawLine(Vector position, Vector size, Color color = ColorBlack);              // Draws a line on the display at the specified position and size with the specified color.
    void drawPixel(Vector position, Color color = ColorBlack);                          // Draws a pixel on the display at the specified position with the specified color.
    void drawRect(Vector position, Vector size, Color color = ColorBlack);              // Draws a rectangle on the display at the specified position and size with the specified color.
    void fillCircle(int16_t x, int16_t y, int16_t r, Color color = ColorBlack);         // Fills a circle on the display at the specified position with the specified radius and color.
    void fillRect(Vector position, Vector size, Color color = ColorBlack);              // Fills a rectangle on the display at the specified position and size with the specified color.
    void fillScreen(Color color = ColorBlack);                                          // Fills the entire screen with the specified color.
    Vector getSize() const noexcept { return Vector(128, 64); }                         // Returns the size of the display.
    void icon(Vector position, const Icon *icon);                                       // Draws an icon on the display at the specified position.
    void image(Vector position, const uint8_t *bitmap, Vector size);                    // Draws a bitmap on the display at the specified position.
    void setFont(Font font = FontPrimary);                                              // Sets the font for text rendering.
    void setFontCustom(FontSize fontSize);                                              // Sets a custom font size for text rendering.
    void text(Vector position, const char *text);                                       // Draws text on the display at the specified position.
    void text(Vector position, const char *text, Color color, Font font = FontPrimary); // Draws text on the display at the specified position with the specified font.
};