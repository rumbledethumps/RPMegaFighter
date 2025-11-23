#ifndef FIGHTERS_H
#define FIGHTERS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Initialize all enemy fighters and ebullets at game start
 */
void init_fighters(void);

/**
 * Update enemy fighter AI, movement, and collision detection
 */
void update_fighters(void);

/**
 * Fire an enemy bullet from a visible fighter toward predicted player position
 */
void fire_ebullet(void);

/**
 * Update enemy bullet positions and collision detection
 */
void update_ebullets(void);

/**
 * Render fighter sprites to screen (converts world to screen coordinates)
 */
void render_fighters(void);

/**
 * Move all fighters offscreen (for game over)
 */
void move_fighters_offscreen(void);

/**
 * Move all enemy bullets offscreen (for game over)
 */
void move_ebullets_offscreen(void);

/**
 * Check if a bullet hit a fighter and handle the collision
 * Returns true if hit occurred
 */
bool check_bullet_fighter_collision(int16_t bullet_x, int16_t bullet_y, 
                                     int16_t* player_score_out, int16_t* game_score_out);

/**
 * Decrement ebullet cooldown timer
 */
void decrement_ebullet_cooldown(void);

/**
 * Increase difficulty by decreasing ebullet cooldown and increasing fighter speed
 */
void increase_fighter_difficulty(void);

/**
 * Reset difficulty parameters to initial values (called on game over)
 */
void reset_fighter_difficulty(void);

#endif // FIGHTERS_H
