#ifndef SBULLETS_H
#define SBULLETS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * sbullets.h - Player super bullet (spread shot) management system
 * 
 * Handles super bullet firing (3-bullet spread), movement, and collision detection
 * Fires when button C is pressed
 */

// Super bullet structure (same as regular bullets)
typedef struct {
    int16_t x, y;           // Position
    int16_t status;         // -1 = inactive, 0-23 = active with direction
    int16_t vx_rem, vy_rem; // Velocity remainder for sub-pixel movement
} SBullet;

/**
 * Initialize super bullet system
 */
void init_sbullets(void);

/**
 * Fire a spread of 3 super bullets (left, center, right of player rotation)
 * Returns true if bullets were fired, false if on cooldown
 */
bool fire_sbullet(uint8_t player_rotation);

/**
 * Update all active super bullets
 * - Move bullets based on direction
 * - Check collisions with enemies
 * - Remove off-screen bullets
 */
void update_sbullets(void);

#endif // SBULLETS_H
