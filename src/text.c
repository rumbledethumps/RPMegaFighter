#include "text.h"
#include <rp6502.h>
#include <stdint.h>

// Graphics constants
#define SWIDTH 320

#include "graphics.h"

/**
 * Draw a simple character at position (x, y)
 */
void draw_char(int16_t x, int16_t y, char c, uint8_t color)
{
    // Simple 3x5 font for uppercase letters and digits
    // Each byte represents a row: bit 2 = left, bit 1 = middle, bit 0 = right
    static const uint8_t font[][5] = {
        // Digits 0-9
        {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
        {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
        {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
        {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
        {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
        {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
        {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
        {0b111, 0b001, 0b010, 0b010, 0b010}, // 7
        {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
        {0b111, 0b101, 0b111, 0b001, 0b111}, // 9
        // Letters (A=10, B=11, etc.)
        {0b111, 0b101, 0b111, 0b101, 0b101}, // A
        {0b110, 0b101, 0b110, 0b101, 0b110}, // B
        {0b111, 0b100, 0b100, 0b100, 0b111}, // C
        {0b110, 0b101, 0b101, 0b101, 0b110}, // D
        {0b111, 0b100, 0b110, 0b100, 0b111}, // E
        {0b111, 0b100, 0b110, 0b100, 0b100}, // F
        {0b111, 0b100, 0b101, 0b101, 0b111}, // G
        {0b101, 0b101, 0b111, 0b101, 0b101}, // H
        {0b111, 0b010, 0b010, 0b010, 0b111}, // I
        {0b001, 0b001, 0b001, 0b101, 0b111}, // J
        {0b101, 0b110, 0b100, 0b110, 0b101}, // K
        {0b100, 0b100, 0b100, 0b100, 0b111}, // L
        {0b101, 0b111, 0b111, 0b101, 0b101}, // M
        {0b101, 0b111, 0b111, 0b111, 0b101}, // N
        {0b111, 0b101, 0b101, 0b101, 0b111}, // O
        {0b111, 0b101, 0b111, 0b100, 0b100}, // P
        {0b111, 0b101, 0b101, 0b111, 0b011}, // Q
        {0b111, 0b101, 0b110, 0b110, 0b101}, // R
        {0b111, 0b100, 0b111, 0b001, 0b111}, // S
        {0b111, 0b010, 0b010, 0b010, 0b010}, // T
        {0b101, 0b101, 0b101, 0b101, 0b111}, // U
        {0b101, 0b101, 0b101, 0b101, 0b010}, // V
        {0b101, 0b101, 0b111, 0b111, 0b101}, // W
        {0b101, 0b101, 0b010, 0b101, 0b101}, // X
        {0b101, 0b101, 0b010, 0b010, 0b010}, // Y
        {0b111, 0b001, 0b010, 0b100, 0b111}, // Z
    };
    
    int idx = -1;
    if (c >= '0' && c <= '9') {
        idx = c - '0';
    } else if (c >= 'A' && c <= 'Z') {
        idx = 10 + (c - 'A');
    } else if (c >= 'a' && c <= 'z') {
        idx = 10 + (c - 'a');
    }
    
    if (idx >= 0 && idx < 36) {
        for (int row = 0; row < 5; row++) {
            uint8_t pattern = font[idx][row];
            for (int col = 0; col < 3; col++) {
                if (pattern & (1 << (2 - col))) {
                    set(x + col, y + row, color);
                }
            }
        }
    }
}

/**
 * Draw a string at position (x, y)
 */
void draw_text(int16_t x, int16_t y, const char* text, uint8_t color)
{
    int16_t offset = 0;
    while (*text) {
        if (*text == ' ') {
            offset += 4;
        } else {
            draw_char(x + offset, y, *text, color);
            offset += 4;
        }
        text++;
    }
}

/**
 * Clear a rectangular area
 */
void clear_rect(int16_t x, int16_t y, int16_t width, int16_t height)
{
    for (int16_t dy = 0; dy < height; dy++) {
        for (int16_t dx = 0; dx < width; dx++) {
            set(x + dx, y + dy, 0x00);
        }
    }
}
