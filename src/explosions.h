#ifndef EXPLOSIONS_H
#define EXPLOSIONS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool active;
    int16_t x, y;
    int16_t vx, vy;
    uint8_t frame;
    uint8_t timer;
} explosion_t;

#define MAX_EXPLOSIONS 16

void init_explosions(void);
void update_explosions(void);
void start_explosion(int16_t x, int16_t y);

#endif