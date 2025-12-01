#ifndef BKGSTARS_H
#define BKGSTARS_H

#include <stdint.h>
#include "constants.h"

// Initialize the background star field
void init_stars(void);

// Draw and update the background stars with parallax scrolling
// dx, dy: change in world coordinates for scrolling
void draw_stars(int16_t dx, int16_t dy);

#endif // BKGSTARS_H
