#include <rp6502.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "random.h"

// uint16_t random(uint16_t low_limit, uint16_t high_limit)
// {
//     if (low_limit > high_limit) {
//         swap(low_limit, high_limit);
//     }

//     return (uint16_t)((rand() % (high_limit-low_limit)) + low_limit);
// }

uint16_t lfsr = 0xACE1u; // Do not use 0
uint16_t seed_counter = 0;

uint16_t rand16() {
    // 16-bit Galois LFSR
    // Polynomial: x^16 + x^14 + x^13 + x^11 + 1
    unsigned lsb = lfsr & 1;
    lfsr >>= 1;
    if (lsb) {
        lfsr ^= 0xB400u;
    }
    return lfsr;
}

// Helper to get a number in range [min, max]
uint16_t random(uint16_t min, uint16_t max) {
    if (min >= max) return min;
    return min + (rand16() % (max - min + 1));
}