#ifndef BKGSTARS_H
#define BKGSTARS_H

#include <stdint.h>
#include "constants.h"

// Star position and color arrays
extern int16_t star_x[NSTAR];      // X-position -- World coordinates
extern int16_t star_y[NSTAR];      // Y-position -- World coordinates
extern int16_t star_x_old[NSTAR];  // prev X-position -- World coordinates
extern int16_t star_y_old[NSTAR];  // prev Y-position -- World coordinates
extern uint8_t star_colour[NSTAR]; // Colour

// Initialize the background star field
void init_stars(void);

// Draw and update the background stars with parallax scrolling
// dx, dy: change in world coordinates for scrolling
void draw_stars(int16_t dx, int16_t dy);

#endif // BKGSTARS_H
