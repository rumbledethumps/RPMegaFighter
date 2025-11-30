// Functions for generating randoms
#include <stdint.h>

#define swap(a, b) { uint16_t t = a; a = b; b = t; }

// LFSR and seed counter are defined in random.c; declare them here
extern uint16_t lfsr;
extern uint16_t seed_counter;

uint16_t random(uint16_t low_limit, uint16_t high_limit);

uint16_t rand16();