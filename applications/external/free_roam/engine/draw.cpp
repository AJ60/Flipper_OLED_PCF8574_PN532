#include "engine/draw.hpp"

Draw::Draw(Canvas *canvas)
    : display(canvas)
{
    canvas_reset(display);
    canvas_clear(display);
}

void Draw::clear(Vector position, Vector size, Color color)
{
    canvas_set_color(display, color);
    canvas_draw_box(display, position.x, position.y, size.x, size.y);
}

void Draw::drawCircle(Vector position, int16_t r, Color color)
{
    canvas_set_color(display, color);
    canvas_draw_circle(display, position.x, position.y, r);
}

void Draw::drawLine(Vector position, Vector size, Color color)
{
    canvas_set_color(display, color);
    canvas_draw_line(display, position.x, position.y, size.x, size.y);
}

void Draw::drawPixel(Vector position, Color color)
{
    canvas_set_color(display, color);
    canvas_draw_dot(display, position.x, position.y);
}

void Draw::drawRect(Vector position, Vector size, Color color)
{
    canvas_set_color(display, color);
    canvas_draw_frame(display, position.x, position.y, size.x, size.y);
}

void Draw::fillCircle(int16_t x, int16_t y, int16_t r, Color color)
{
    canvas_set_color(display, color);

    for (int16_t dy = -r; dy <= r; dy++)
    {
        for (int16_t dx = -r; dx <= r; dx++)
        {
            if (dx * dx + dy * dy <= r * r)
            {
                canvas_draw_dot(display, x + dx, y + dy);
            }
        }
    }
}

void Draw::fillRect(Vector position, Vector size, Color color)
{
    canvas_set_color(display, color);
    canvas_draw_box(display, position.x, position.y, size.x, size.y);
}

void Draw::fillScreen(Color color)
{
    canvas_set_color(display, color);
    canvas_clear(display);
}

void Draw::icon(Vector position, const Icon *icon)
{
    if (icon == nullptr)
    {
        return;
    }
    canvas_draw_icon(display, position.x, position.y, icon);
}

void Draw::image(Vector position, const uint8_t *bitmap, Vector size)
{
    if (bitmap == nullptr)
    {
        return;
    }
    canvas_draw_bitmap(display, position.x, position.y, size.x, size.y, bitmap);
    // might try canvas_draw_xbm if that doesnt look as expected
}

void Draw::setFontCustom(FontSize fontSize)
{
    canvas_set_font_custom(display, fontSize);
}

void Draw::setFont(Font font)
{
    canvas_set_font(display, font);
}

void Draw::text(Vector position, const char *text)
{
    canvas_draw_str(display, position.x, position.x, text);
}

void Draw::text(Vector position, const char *text, Color color, Font font)
{
    canvas_set_font(display, font);
    canvas_set_color(display, color);
    canvas_draw_str(display, position.x, position.x, text);
}