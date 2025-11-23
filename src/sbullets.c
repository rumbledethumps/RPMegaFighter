#include "sbullets.h"
#include "screen.h"
#include "sound.h"
#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>

// Get SHIP_ROTATION_STEPS from screen.h
// (It's defined there as the number of rotation steps)

// ============================================================================
// CONSTANTS
// ============================================================================

#define MAX_SBULLETS        3
#define SBULLET_COOLDOWN    120  // Frames between super bullet shots
#define SBULLET_SPEED_SHIFT 6   // Divide by 64 for bullet speed (~4 pixels/frame)

// ============================================================================
// EXTERNAL DEPENDENCIES
// ============================================================================

// Game state from main
extern int16_t player_score;
extern int16_t game_score;

// Player position
extern int16_t player_x;
extern int16_t player_y;

// World offset
extern int16_t world_offset_x;
extern int16_t world_offset_y;

// Sprite configuration addresses
extern unsigned SBULLET_CONFIG;

// Lookup tables from definitions.h
extern const int16_t sin_fix[25];
extern const int16_t cos_fix[25];

// Collision check from fighters module
extern bool check_bullet_fighter_collision(int16_t bullet_x, int16_t bullet_y,
                                          int16_t* player_score_out, int16_t* game_score_out);

// ============================================================================
// MODULE STATE
// ============================================================================

static SBullet sbullets[MAX_SBULLETS];
static uint16_t sbullet_cooldown_timer = 0;

// ============================================================================
// FUNCTIONS
// ============================================================================

void init_sbullets(void)
{
    for (uint8_t i = 0; i < MAX_SBULLETS; i++) {
        sbullets[i].status = -1;
        sbullets[i].x = 0;
        sbullets[i].y = 0;
        sbullets[i].vx_rem = 0;
        sbullets[i].vy_rem = 0;
    }
    sbullet_cooldown_timer = 0;
}

bool fire_sbullet(uint8_t player_rotation)
{
    // Check if on cooldown
    if (sbullet_cooldown_timer > 0) {
        sbullet_cooldown_timer--;
        return false;
    }
    
    // Check if all 3 bullets are available
    if (sbullets[0].status >= 0 || sbullets[1].status >= 0 || sbullets[2].status >= 0) {
        return false;
    }
    
    // Reset cooldown
    sbullet_cooldown_timer = SBULLET_COOLDOWN;
    
    // Fire 3 bullets: left (-1), center (0), right (+1) of player rotation
    int16_t start_x = player_x + 2;  // Center of player sprite (8x8 -> 4 pixels offset)
    int16_t start_y = player_y + 2;
    
    // Left bullet (rotation - 1)
    sbullets[0].status = player_rotation - 1;
    if (sbullets[0].status < 0) {
        sbullets[0].status = SHIP_ROTATION_STEPS - 1;
    }
    sbullets[0].x = start_x;
    sbullets[0].y = start_y;
    sbullets[0].vx_rem = 0;
    sbullets[0].vy_rem = 0;
    
    // Center bullet (player rotation)
    sbullets[1].status = player_rotation;
    sbullets[1].x = start_x;
    sbullets[1].y = start_y;
    sbullets[1].vx_rem = 0;
    sbullets[1].vy_rem = 0;
    
    // Right bullet (rotation + 1)
    sbullets[2].status = player_rotation + 1;
    if (sbullets[2].status >= SHIP_ROTATION_STEPS) {
        sbullets[2].status = 0;
    }
    sbullets[2].x = start_x;
    sbullets[2].y = start_y;
    sbullets[2].vx_rem = 0;
    sbullets[2].vy_rem = 0;
    
    // Play sound effect
    play_sound(SFX_TYPE_PLAYER_FIRE, 880, PSG_WAVE_SQUARE, 0, 3, 2, 2);
    
    return true;
}

void update_sbullets(void)
{
    // Decrement cooldown timer
    if (sbullet_cooldown_timer > 0) {
        sbullet_cooldown_timer--;
    }
    
    for (uint8_t i = 0; i < MAX_SBULLETS; i++) {
        if (sbullets[i].status < 0) {
            // Move sprite offscreen when inactive
            unsigned ptr = SBULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            continue;
        }
        
        // Check collision with fighters before moving
        if (check_bullet_fighter_collision(sbullets[i].x, sbullets[i].y, &player_score, &game_score)) {
            // Hit a fighter - deactivate bullet
            sbullets[i].status = -1;
            unsigned ptr = SBULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            continue;
        }
        
        // Calculate velocity based on stored direction
        int16_t bvx_req = -sin_fix[sbullets[i].status];
        int16_t bvy_req = -cos_fix[sbullets[i].status];
        
        // Apply velocity with remainder tracking (>>6 = divide by 64)
        int16_t bvx_applied = (bvx_req + sbullets[i].vx_rem) >> SBULLET_SPEED_SHIFT;
        int16_t bvy_applied = (bvy_req + sbullets[i].vy_rem) >> SBULLET_SPEED_SHIFT;
        
        sbullets[i].vx_rem = bvx_req + sbullets[i].vx_rem - (bvx_applied << SBULLET_SPEED_SHIFT);
        sbullets[i].vy_rem = bvy_req + sbullets[i].vy_rem - (bvy_applied << SBULLET_SPEED_SHIFT);
        
        // Move bullet
        sbullets[i].x += bvx_applied;
        sbullets[i].y += bvy_applied;
        
        // Check if bullet is still on screen
        if (sbullets[i].x >= 0 && sbullets[i].x < SCREEN_WIDTH &&
            sbullets[i].y >= 0 && sbullets[i].y < SCREEN_HEIGHT) {
            // Update sprite position
            unsigned ptr = SBULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, sbullets[i].x);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, sbullets[i].y);
        } else {
            // Off screen - deactivate
            sbullets[i].status = -1;
            unsigned ptr = SBULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
        }
    }
}
