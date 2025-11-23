#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Initialize player state at game start
 */
void init_player(void);

/**
 * Reset player to center position (used in game over)
 */
void reset_player_position(void);

/**
 * Update player movement, rotation, and physics
 */
void update_player(void);

/**
 * Update player sprite position and rotation on screen
 */
void update_player_sprite(void);

/**
 * Fire a bullet from the player ship
 */
void fire_bullet(void);

/**
 * Decrement bullet cooldown timer
 */
void decrement_bullet_cooldown(void);

/**
 * Get player rotation for external use (e.g., bullet direction)
 */
int16_t get_player_rotation(void);

#endif // PLAYER_H
