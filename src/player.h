#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>
#include <stdbool.h>

extern int16_t player_x;
extern int16_t player_y;
extern int16_t scroll_dx;
extern int16_t scroll_dy;

extern bool player_is_dying;
void trigger_player_death(void);

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
 * @param demomode true when running demo (AI) mode, false for normal input-driven play
 */
void update_player(bool demomode);

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
