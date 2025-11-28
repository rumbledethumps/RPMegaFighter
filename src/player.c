#include "player.h"
#include "constants.h"
#include "screen.h"
#include "bullets.h"
#include "sbullets.h"
#include "sound.h"
#include "input.h"
#include "usb_hid_keys.h"
#include "random.h"
#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// TYPES
// ============================================================================

// Bullet type defined in bullets.h

// ============================================================================
// EXTERNAL DEPENDENCIES
// ============================================================================

extern gamepad_t gamepad[GAMEPAD_COUNT];

// Lookup tables from definitions.h
extern const int16_t sin_fix[25];
extern const int16_t cos_fix[25];
extern const int16_t t2_fix4[25];

// Sprite configuration addresses
extern unsigned SPACECRAFT_CONFIG;
extern unsigned BULLET_CONFIG;

// Bullet array from main
extern Bullet bullets[MAX_BULLETS];
extern uint8_t current_bullet_index;

// World scrolling state (modified by player movement)
extern int16_t scroll_dx, scroll_dy;
extern int16_t world_offset_x, world_offset_y;

// Sound system (types defined in sound.h)
extern void play_sound(uint8_t type, uint16_t frequency, uint8_t waveform, 
                       uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release);

// ============================================================================
// MODULE STATE
// ============================================================================

// Player position (exported for other modules)
int16_t player_x = SCREEN_WIDTH_D2;
int16_t player_y = SCREEN_HEIGHT_D2;
int16_t player_vx_applied = 0;
int16_t player_vy_applied = 0;

// Player internal state
static int16_t player_vx = 0, player_vy = 0;
static int16_t player_x_rem = 0, player_y_rem = 0;
static int16_t player_rotation = 0;
static int16_t player_rotation_frame = 0;
static int16_t player_thrust_x = 0;
static int16_t player_thrust_y = 0;
static int16_t player_thrust_delay = 0;
static int16_t player_thrust_count = 0;

// Demo-mode state (controls AI rotation holds/direction)
static int8_t demo_rotate_dir = 0; // -1 = left, 0 = none, 1 = right
static uint16_t demo_rotate_hold = 0; // frames remaining to hold current rotation

// Demo-mode thrust state
static bool demo_thrusting = false;
static uint16_t demo_thrust_hold = 0;

// Bullet cooldown
static uint16_t bullet_cooldown = 0;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Get velocity components from rotation angle
 * Rotation 0 = pointing up (negative Y), increases clockwise
 */
static inline void get_velocity_from_rotation(uint8_t rotation, int16_t* vx_out, int16_t* vy_out)
{
    rotation = rotation % SHIP_ROTATION_STEPS;
    *vx_out = -sin_fix[rotation];
    *vy_out = -cos_fix[rotation];
}

// Helper: compute minimal signed rotation difference in range (-steps/2, steps/2]
static inline int rotation_diff(int cur, int tgt)
{
    int diff = tgt - cur;
    int half = SHIP_ROTATION_STEPS / 2;
    while (diff > half) diff -= SHIP_ROTATION_STEPS;
    while (diff <= -half) diff += SHIP_ROTATION_STEPS;
    return diff;
}

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

void init_player(void)
{
    player_x = SCREEN_WIDTH_D2;
    player_y = SCREEN_HEIGHT_D2;
    player_vx = 0;
    player_vy = 0;
    player_vx_applied = 0;
    player_vy_applied = 0;
    player_x_rem = 0;
    player_y_rem = 0;
    player_rotation = 0;
    player_rotation_frame = 0;
    player_thrust_x = 0;
    player_thrust_y = 0;
    player_thrust_delay = 0;
    player_thrust_count = 0;
    bullet_cooldown = 0;
}

void reset_player_position(void)
{
    player_x = SCREEN_WIDTH_D2;
    player_y = SCREEN_HEIGHT_D2;
    player_vx = 0;
    player_vy = 0;
    player_thrust_x = 0;
    player_thrust_y = 0;
    
    // Update sprite position
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, x_pos_px, player_x);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, y_pos_px, player_y);
}

void update_player(bool demomode)
{
    (void)demomode; 
    bool rotate_left = false;
    bool rotate_right = false;
    bool thrust = false;

    // Handle player rotation
    player_rotation_frame++;
    if (player_rotation_frame >= SHIP_ROT_SPEED) {
        player_rotation_frame = 0;
        
        if (demomode) {
            // Demo-mode AI: pick a direction and hold it for a few frames.
            if (demo_rotate_hold == 0) {
                // Decide rotation with bias toward steering to screen center.
                // Compute vector from player to screen center.
                int16_t cx = SCREEN_WIDTH_D2;
                int16_t cy = SCREEN_HEIGHT_D2;
                int32_t dx = (int32_t)cx - (int32_t)player_x;
                int32_t dy = (int32_t)cy - (int32_t)player_y;

                // Find best rotation index pointing toward center by maximizing dot(thrust_vector, center_vector)
                int best_rot = 0;
                int32_t best_dot = INT32_MIN;
                for (int r = 0; r < SHIP_ROTATION_STEPS; r++) {
                    int32_t tvx = - (int32_t)sin_fix[r];
                    int32_t tvy = - (int32_t)cos_fix[r];
                    int32_t dot = tvx * dx + tvy * dy;
                    if (dot > best_dot) {
                        best_dot = dot;
                        best_rot = r;
                    }
                }

                int diff = rotation_diff(player_rotation, best_rot);

                // If error is large, force a steer toward the center to correct quickly.
                int absdiff = diff < 0 ? -diff : diff;
                if (absdiff > 2) {
                    demo_rotate_dir = (diff < 0) ? -1 : 1;
                    // Shorter hold to allow finer corrections
                    demo_rotate_hold = random(6, 18);
                } else {
                    // Otherwise bias strongly toward steering to center, but allow
                    // occasional pauses to reduce mechanical motion.
                    // Lower the no-rotation chance (more likely to steer).
                    uint16_t rprob = random(0, 99);
                    if (rprob < 30) {
                        demo_rotate_dir = 0;
                    } else {
                        // Very high chance to steer toward center when choosing to rotate
                        uint16_t r2 = random(0, 99);
                        if (r2 < 95) {
                            if (diff < 0) demo_rotate_dir = -1; else if (diff > 0) demo_rotate_dir = 1; else demo_rotate_dir = 0;
                        } else {
                            // fallback small chance to pick a random direction for variation
                            demo_rotate_dir = (random(0, 1) == 0) ? -1 : 1;
                        }
                    }

                    // Hold between 6 and 40 frames
                    demo_rotate_hold = random(6, 40);
                }
            } else {
                demo_rotate_hold--;
            }

            rotate_left = (demo_rotate_dir == -1);
            rotate_right = (demo_rotate_dir == 1);
        } else {
            rotate_left = key(KEY_LEFT) || 
                          (gamepad[0].sticks & GP_LSTICK_LEFT) ||
                          (gamepad[0].dpad & GP_DPAD_LEFT);
            rotate_right = key(KEY_RIGHT) || 
                           (gamepad[0].sticks & GP_LSTICK_RIGHT) ||
                           (gamepad[0].dpad & GP_DPAD_RIGHT);
        }
        
        if (rotate_left) {
            player_rotation++;
            if (player_rotation >= SHIP_ROTATION_STEPS) {
                player_rotation = 0;
            }
        }
        if (rotate_right) {
            player_rotation--;
            if (player_rotation < 0) {
                player_rotation = SHIP_ROTATION_STEPS - 1;
            }
        }
    }
    
    // Handle thrust/acceleration
    if (demomode) {
        // Demo-mode AI for thrust: bias toward thrusting, but throttle
        // when the ship is facing away from the screen center.
        if (demo_thrust_hold == 0) {
            // Compute vector to screen center
            int16_t cx = SCREEN_WIDTH_D2;
            int16_t cy = SCREEN_HEIGHT_D2;
            int32_t dx = (int32_t)cx - (int32_t)player_x;
            int32_t dy = (int32_t)cy - (int32_t)player_y;

            // Current facing thrust vector
            int32_t tvx = - (int32_t)sin_fix[player_rotation];
            int32_t tvy = - (int32_t)cos_fix[player_rotation];

            // Dot product: positive means facing toward center
            int64_t dot = (int64_t)tvx * dx + (int64_t)tvy * dy;

            // Base thrust probability
            uint16_t base_prob = 80; // percent

            // If facing away from center, reduce probability and shorten holds
            if (dot <= 0) {
                base_prob = 25; // much less likely to thrust when facing away
            }

            uint16_t r = random(0, 99);
            demo_thrusting = (r < base_prob);

            // Hold length biased longer when thrusting toward center
            if (demo_thrusting) {
                demo_thrust_hold = random(20, 80);
            } else {
                demo_thrust_hold = random(6, 30);
            }
        } else {
            demo_thrust_hold--;
        }
        thrust = demo_thrusting;
    } else {
        thrust = key(KEY_UP) || 
                 (gamepad[0].sticks & GP_LSTICK_UP) ||
                 (gamepad[0].dpad & GP_DPAD_UP);
    }
    // bool reverse_thrust = key(KEY_DOWN) || (gamepad[0].sticks & GP_LSTICK_DOWN) || (gamepad[0].dpad & GP_DPAD_DOWN);  // Disabled for game balance
    
    if (thrust) {
        int16_t thrust_vx, thrust_vy;
        get_velocity_from_rotation(player_rotation, &thrust_vx, &thrust_vy);
        
        player_vx = thrust_vx;
        player_vy = thrust_vy;
        
        player_thrust_delay = 0;
        
        int16_t new_thrust_x = player_thrust_x + (thrust_vx >> 4);
        int16_t new_thrust_y = player_thrust_y + (thrust_vy >> 4);
        
        if (new_thrust_x > -1024 && new_thrust_x < 1024) {
            player_thrust_x = new_thrust_x;
        }
        if (new_thrust_y > -1024 && new_thrust_y < 1024) {
            player_thrust_y = new_thrust_y;
        }
    } else {
        player_vx = 0;
        player_vy = 0;
    }
    
    // Apply momentum and friction
    int16_t total_vx = player_vx + player_thrust_x;
    int16_t total_vy = player_vy + player_thrust_y;
    
    player_vx_applied = (total_vx + player_x_rem) >> 9;
    player_vy_applied = (total_vy + player_y_rem) >> 9;
    
    player_x_rem = total_vx + player_x_rem - (player_vx_applied << 9);
    player_y_rem = total_vy + player_y_rem - (player_vy_applied << 9);
    
    // Apply friction when not thrusting
    if (!thrust) {
        player_thrust_count++;
        if (player_thrust_count > 50 && player_thrust_delay < 8) {
            player_thrust_delay++;
            player_thrust_count = 0;
            
            if (player_vx == 0) {
                player_thrust_x >>= 1;
            }
            if (player_vy == 0) {
                player_thrust_y >>= 1;
            }
        }
        
        if (player_thrust_delay >= 8) {
            player_thrust_x = 0;
            player_thrust_y = 0;
        }
    }
    
    // Update player position
    int16_t new_x = player_x + player_vx_applied;
    int16_t new_y = player_y + player_vy_applied;
    
    // Handle screen boundaries (scrolling behavior)
    if (new_x > BOUNDARY_X && new_x < (SCREEN_WIDTH - BOUNDARY_X)) {
        player_x = new_x;
        scroll_dx = 0;
    } else {
        scroll_dx = new_x - player_x;
        world_offset_x += scroll_dx;
    }
    
    if (new_y > BOUNDARY_Y && new_y < (SCREEN_HEIGHT - BOUNDARY_Y)) {
        player_y = new_y;
        scroll_dy = 0;
    } else {
        scroll_dy = new_y - player_y;
        world_offset_y += scroll_dy;
    }
}

void update_player_sprite(void)
{
    // Update sprite position
    RIA.step0 = sizeof(vga_mode4_asprite_t);
    RIA.step1 = sizeof(vga_mode4_asprite_t);
    RIA.addr0 = SPACECRAFT_CONFIG + 12;
    RIA.addr1 = SPACECRAFT_CONFIG + 13;
    
    RIA.rw0 = player_x & 0xFF;
    RIA.rw1 = (player_x >> 8) & 0xFF;
    
    RIA.addr0 = SPACECRAFT_CONFIG + 14;
    RIA.addr1 = SPACECRAFT_CONFIG + 15;
    RIA.rw0 = player_y & 0xFF;
    RIA.rw1 = (player_y >> 8) & 0xFF;
    
    // Update rotation transform matrix
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[0],  cos_fix[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[1], -sin_fix[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[2],  t2_fix4[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[3],  sin_fix[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[4],  cos_fix[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[5],  t2_fix4[SHIP_ROTATION_MAX - player_rotation + 1]);
}

void fire_bullet(void)
{
    if (bullet_cooldown > 0) {
        return;
    }
    
    if (bullets[current_bullet_index].status < 0) {
        bullets[current_bullet_index].status = player_rotation;
        bullets[current_bullet_index].x = player_x + 4;
        bullets[current_bullet_index].y = player_y + 4;
        bullets[current_bullet_index].vx_rem = 0;
        bullets[current_bullet_index].vy_rem = 0;
        
        unsigned ptr = BULLET_CONFIG + current_bullet_index * sizeof(vga_mode4_sprite_t);
        xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, bullets[current_bullet_index].x);
        xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, bullets[current_bullet_index].y);
        
        play_sound(SFX_TYPE_PLAYER_FIRE, 110, PSG_WAVE_SQUARE, 0, 3, 4, 0);
        
        current_bullet_index++;
        if (current_bullet_index >= MAX_BULLETS) {
            current_bullet_index = 0;
        }
        
        bullet_cooldown = BULLET_COOLDOWN;
    }
}

void decrement_bullet_cooldown(void)
{
    if (bullet_cooldown > 0) {
        bullet_cooldown--;
    }
}

int16_t get_player_rotation(void)
{
    return player_rotation;
}
