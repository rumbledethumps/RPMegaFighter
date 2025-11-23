#ifndef TEXT_H
#define TEXT_H

#include <stdint.h>

// Draw a single character at position (x, y)
void draw_char(int16_t x, int16_t y, char c, uint8_t color);

// Draw a string at position (x, y)
void draw_text(int16_t x, int16_t y, const char* text, uint8_t color);

// Clear a rectangular area
void clear_rect(int16_t x, int16_t y, int16_t width, int16_t height);

#endif // TEXT_H
