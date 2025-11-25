#include "bkgstars.h"
#include "constants.h"
#include <rp6502.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include "random.h"
#include "graphics.h"

void init_stars(void) 
{
    for (uint8_t i = 0; i < NSTAR; i++) {
        star_x[i] = random(1, STARFIELD_X);
        // Keep stars away from HUD area (top 10 pixels)
        star_y[i] = random(11, STARFIELD_Y);
        star_colour[i] = random(1, 255);
        star_x_old[i] = star_x[i];
        star_y_old[i] = star_y[i];
    }
}

void draw_stars(int16_t dx, int16_t dy) 
{
    for (uint8_t i = 0; i < NSTAR; i++) {
        // Clear previous star position
        if (star_x_old[i] > 0 && star_x_old[i] < 320 && 
            star_y_old[i] > 0 && star_y_old[i] < 180) {
            set(star_x_old[i], star_y_old[i], 0x00);
        }
        
        // Update star position based on scroll
        star_x[i] = star_x_old[i] - dx;
        if (star_x[i] <= 0) {
            star_x[i] += STARFIELD_X;
        }
        if (star_x[i] > STARFIELD_X) {
            star_x[i] -= STARFIELD_X;
        }
        star_x_old[i] = star_x[i];
        
        star_y[i] = star_y_old[i] - dy;
        if (star_y[i] <= 10) {
            // When wrapping from top, place at bottom minus HUD offset
            star_y[i] += (STARFIELD_Y - 10);
        }
        if (star_y[i] > STARFIELD_Y) {
            // When wrapping from bottom, place below HUD area
            star_y[i] = (star_y[i] - STARFIELD_Y) + 11;
        }
        star_y_old[i] = star_y[i];
        
        // Draw star at new position if on screen (avoid HUD area at top)
        if (star_x[i] > 0 && star_x[i] < 320 && 
            star_y[i] > 10 && star_y[i] < 180) {
            set(star_x[i], star_y[i], star_colour[i]);
        }
    }
}
