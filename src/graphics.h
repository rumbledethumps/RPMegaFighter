#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "screen.h"
#include "constants.h"
#include <stdlib.h>

// Swap macro for line drawing
#define swap(a, b) { uint16_t t = a; a = b; b = t; }

// Routine for placing a single dot on the screen for 8bit-colour depth
static inline void set(int16_t x, int16_t y, uint8_t colour)
{
    RIA.addr0 =  x + (SCREEN_WIDTH * y);
    RIA.step0 = 1;
    RIA.rw0 = colour;
}

// ---------------------------------------------------------------------------
// Draw a straight line from (x0,y0) to (x1,y1) with given color
// using Bresenham's algorithm
// ---------------------------------------------------------------------------
static inline void draw_line(uint16_t colour, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    int16_t dx, dy;
    int16_t err;
    int16_t ystep;
    int16_t steep = abs((int16_t)y1 - (int16_t)y0) > abs((int16_t)x1 - (int16_t)x0);

    if (steep) {
        swap(x0, y0);
        swap(x1, y1);
    }

    if (x0 > x1) {
        swap(x0, x1);
        swap(y0, y1);
    }

    dx = x1 - x0;
    dy = abs((int16_t)y1 - (int16_t)y0);

    err = dx / 2;

    if (y0 < y1) {
        ystep = 1;
    } else {
        ystep = -1;
    }

    for (; x0<=x1; x0++) {
        if (steep) {
            set(y0, x0, colour);
        } else {
            set(x0, y0, colour);
        }

        err -= dy;

        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

#endif // GRAPHICS_H