#include "fighters.h"
#include "constants.h"
#include "screen.h"
#include "random.h"
#include "sound.h"
#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "powerup.h"

// ============================================================================
// CONSTANTS
// ============================================================================


// ============================================================================
// TYPES
// ============================================================================

typedef struct {
    int16_t x, y;
    int16_t status;
    int16_t vx_rem, vy_rem;
} Bullet;

typedef struct {
    int16_t x, y;
    int16_t vx, vy;
    int16_t vx_i, vy_i;
    int16_t vx_rem, vy_rem;
    int16_t status;
    int16_t dx, dy;
    int16_t frame;
    int16_t lx1, ly1;
    int16_t lx2, ly2;
    int16_t anim_timer;
    bool is_exploding;
} Fighter;

// ============================================================================
// EXTERNAL DEPENDENCIES
// ============================================================================

// Game state from main
extern int16_t player_x, player_y;
extern int16_t player_vx_applied, player_vy_applied;
extern int16_t scroll_dx, scroll_dy;
extern int16_t enemy_score;
extern int16_t game_level;
extern uint16_t game_frame;

// Sprite configuration addresses
extern unsigned FIGHTER_CONFIG;
extern unsigned EBULLET_CONFIG;

// Lookup tables from definitions.h
extern const int16_t sin_fix[25];
extern const int16_t cos_fix[25];

// Sound system (types defined in sound.h)
extern void play_sound(uint8_t type, uint16_t frequency, uint8_t waveform, 
                       uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release);

// ============================================================================
// MODULE STATE
// ============================================================================

static Bullet ebullets[MAX_EBULLETS];
static uint16_t ebullet_cooldown = 0;
static uint16_t max_ebullet_cooldown = INITIAL_EBULLET_COOLDOWN;
static uint8_t current_ebullet_index = 0;

static Fighter fighters[MAX_FIGHTERS];
int16_t active_fighter_count = 0;  // Non-static, may be used externally

// Fighter speed parameters (increase with level)
static int16_t fighter_speed_min = INITIAL_FIGHTER_SPEED_MIN;
static int16_t fighter_speed_max = INITIAL_FIGHTER_SPEED_MAX;

// ============================================================================
// FUNCTIONS
// ============================================================================

#define FIGHTER_BYTES_PER_FRAME 32  // 4x4 pixels * 2 bytes per pixel

void set_fighter_frame(uint8_t fighter_idx, uint8_t frame_idx) {
    if (fighter_idx >= MAX_FIGHTERS) return; // Safety check

    // 1. Calculate address of the specific fighter's configuration struct
    unsigned sprite_config_ptr = FIGHTER_CONFIG + (fighter_idx * sizeof(vga_mode4_sprite_t));

    // 2. Calculate address of the specific frame image data
    //    Address = Start of Sheet + (Frame Number * Bytes per Frame)
    unsigned image_data_ptr = EXPLOSION_DATA + (frame_idx * FIGHTER_BYTES_PER_FRAME);

    // 3. Update the pointer in XRAM
    //    We only change where this specific sprite looks for pixels
    xram0_struct_set(sprite_config_ptr, vga_mode4_sprite_t, xram_sprite_ptr, image_data_ptr);
}


void init_fighters(void)
{
    for (uint8_t i = 0; i < MAX_FIGHTERS; i++) {
        fighters[i].vx_i = random(fighter_speed_min, fighter_speed_max);
        fighters[i].vy_i = random(fighter_speed_min, fighter_speed_max);
        fighters[i].vx = 0;
        fighters[i].vy = 0;
        fighters[i].status = 1;
        fighters[i].is_exploding = false; // Not exploding at start
        fighters[i].anim_timer = 0; // Initialize animation timer
        set_fighter_frame(i, 0); // Points back to the first image in the sheet (Normal ship)
        
        // fighters[i].x = random(SCREEN_WIDTH_D2, SCREEN_WIDTH) + SCREEN_WIDTH + 144;
        // fighters[i].y = random(SCREEN_HEIGHT_D2, SCREEN_HEIGHT) + SCREEN_HEIGHT + 104;
        
        // if (random(0, 1)) {
        //     fighters[i].x = -fighters[i].x;
        // }
        // if (random(0, 1)) {
        //     fighters[i].y = -fighters[i].y;
        // }

        uint8_t edge = random(0, 4);  // 0=right, 1=left, 2=top, 3=bottom
                
        if (edge == 0) {
            // Spawn on right edge
            fighters[i].x = SCREEN_WIDTH + random(70, 150);
            fighters[i].y = random(20, SCREEN_HEIGHT - 20);
        } else if (edge == 1) {
            // Spawn on left edge
            fighters[i].x = -random(70, 150);
            fighters[i].y = random(20, SCREEN_HEIGHT - 20);
        } else if (edge == 2) {
            // Spawn on top edge
            fighters[i].x = random(20, SCREEN_WIDTH - 20);
            fighters[i].y = SCREEN_HEIGHT + random(70, 150);
        } else {
            // Spawn on bottom edge
            fighters[i].x = random(20, SCREEN_WIDTH - 20);
            fighters[i].y = -random(70, 150);
        }
        
        fighters[i].vx_rem = 0;
        fighters[i].vy_rem = 0;
        fighters[i].dx = 0;
        fighters[i].dy = 0;
        fighters[i].frame = random(0, 1);
    }
    active_fighter_count = MAX_FIGHTERS;
    
    // Initialize ebullets
    for (uint8_t i = 0; i < MAX_EBULLETS; i++) {
        ebullets[i].status = -1;
        ebullets[i].x = 0;
        ebullets[i].y = 0;
        ebullets[i].vx_rem = 0;
        ebullets[i].vy_rem = 0;
    }
}

void update_fighters(void)
{
    int16_t fvx_applied, fvy_applied;
    
    int16_t player_world_x = player_x;
    int16_t player_world_y = player_y;

    // for (uint8_t i = 0; i < 1; i++) {
    //     printf("Fighter %d position 1: x=%d, y=%d\n", i, fighters[i].x, fighters[i].y);
    //     printf(fighters[i].status ? "active\n" : "inactive\n");
    // }

    for (uint8_t i = 0; i < MAX_FIGHTERS; i++) {

        if (fighters[i].is_exploding) {
            fighters[i].anim_timer++;
    
            // Slow down animation (e.g., change frame every 4 ticks)
            uint8_t current_frame = fighters[i].anim_timer / 4;
            
            if (current_frame < 8) {
                set_fighter_frame(i, current_frame);
            } else {
                // Animation done, kill fighter or respawn
                fighters[i].is_exploding = false;
            }

            if (current_frame == 8 && !powerup.active) {
                int16_t drop_chance = random(0, 100);
                if (drop_chance < POWERUP_DROP_CHANCE_PERCENT) {
                    powerup.active = true;
                    powerup.timer = POWERUP_DURATION_FRAMES;
                    powerup.x = fighters[i].x;
                    powerup.y = fighters[i].y;
                }
            }

        }

        if (fighters[i].status <= 0) {
            fighters[i].status--;
            if (fighters[i].status <= -FIGHTER_SPAWN_RATE) {
                fighters[i].vx_i = random(fighter_speed_min, fighter_speed_max);
                fighters[i].vy_i = random(fighter_speed_min, fighter_speed_max);
                
                uint8_t edge = random(0, 4);  // 0=right, 1=left, 2=top, 3=bottom
                
                if (edge == 0) {
                    // Spawn on right edge
                    fighters[i].x = SCREEN_WIDTH + random(20, 100);
                    fighters[i].y = random(20, SCREEN_HEIGHT - 20);
                } else if (edge == 1) {
                    // Spawn on left edge
                    fighters[i].x = -random(20, 100);
                    fighters[i].y = random(20, SCREEN_HEIGHT - 20);
                } else if (edge == 2) {
                    // Spawn on top edge
                    fighters[i].x = random(20, SCREEN_WIDTH - 20);
                    fighters[i].y = SCREEN_HEIGHT + random(20, 100);
                } else {
                    // Spawn on bottom edge
                    fighters[i].x = random(20, SCREEN_WIDTH - 20);
                    fighters[i].y = -random(20, 100);
                }
                
                fighters[i].status = 1;
                fighters[i].is_exploding = false; // Reset exploding state
                fighters[i].anim_timer = 0; // Initialize animation timer
                set_fighter_frame(i, 0); // Points back to the first image in the sheet (Normal ship)
                active_fighter_count++;
            }
            if (fighters[i].is_exploding) {
                fighters[i].x -= scroll_dx;
                fighters[i].y -= scroll_dy;
            } 
            continue;
        }

        fighters[i].x -= scroll_dx;
        fighters[i].y -= scroll_dy;

        if (fighters[i].x + 4 > player_x && fighters[i].x < player_x + 8 &&
            fighters[i].y + 4 > player_y && fighters[i].y < player_y + 8) {
            fighters[i].status = 0;
            active_fighter_count--;
            enemy_score += 2;
            fighters[i].is_exploding = true; // Start explosion sequence
            continue;
        }
        
        if (game_frame == 0) {
            int16_t fdx = player_world_x - fighters[i].x;
            int16_t fdy = player_world_y - fighters[i].y;
            
            if (fdx > 0) {
                fighters[i].vx = fighters[i].vx_i;
            } else if (fdx < 0) {
                fighters[i].vx = -fighters[i].vx_i;
            } else {
                fighters[i].vx = 0;
            }
            
            if (fdy > 0) {
                fighters[i].vy = fighters[i].vy_i;
            } else if (fdy < 0) {
                fighters[i].vy = -fighters[i].vy_i;
            } else {
                fighters[i].vy = 0;
            }
        }
        
        fvx_applied = (fighters[i].vx + fighters[i].vx_rem) >> 8;
        fvy_applied = (fighters[i].vy + fighters[i].vy_rem) >> 8;
        
        fighters[i].vx_rem = fighters[i].vx + fighters[i].vx_rem - (fvx_applied << 8);
        fighters[i].vy_rem = fighters[i].vy + fighters[i].vy_rem - (fvy_applied << 8);
        
        fighters[i].dx = fvx_applied;
        fighters[i].dy = fvy_applied;
        
        fighters[i].x += fvx_applied;
        fighters[i].y += fvy_applied;

        if (fighters[i].x > STARFIELD_X) {
            fighters[i].x -= WORLD_X;
        } else if (fighters[i].x < -STARFIELD_X) {
            fighters[i].x += WORLD_X;
        }

        if (fighters[i].y > STARFIELD_Y) {
            fighters[i].y -= WORLD_Y;
        } else if (fighters[i].y < -STARFIELD_Y) {
            fighters[i].y += WORLD_Y;
        }

    }

    // for (uint8_t i = 0; i < 1; i++) {
    //     printf("Fighter %d position 2: x=%d, y=%d\n", i, fighters[i].x, fighters[i].y);
    //     printf(fighters[i].status ? "active\n" : "inactive\n");
    // }
}

void fire_ebullet(void)
{
    if (ebullet_cooldown > 0) { //Global Fighter fire cooldown
        return;
    }
    
    // ebullet_cooldown = max_ebullet_cooldown;
    ebullet_cooldown = NEBULLET_TIMER_MAX;
    
    if (ebullets[current_ebullet_index].status < 0) {
        for (uint8_t i = 0; i < MAX_FIGHTERS; i++) {
            if (fighters[i].status == 1) {  // A single ship is ready to fire

                if (fighters[i].x > 0 && fighters[i].x < SCREEN_WIDTH - 4 &&
                    fighters[i].y > 0 && fighters[i].y < SCREEN_HEIGHT - 4) {

                    int16_t fdx = player_x - fighters[i].x;
                    int16_t fdy = -(player_y - fighters[i].y);
                    int16_t distance = abs(fdx) + abs(fdy);
                    
                    if (distance > 0) {
                        int16_t tti_frames = distance / 4;
                        if (tti_frames == 0) tti_frames = 1;
                        
                        int16_t pre_player_x = player_x + 4 + (player_vx_applied * tti_frames);
                        int16_t pre_player_y = player_y + 4 + (player_vy_applied * tti_frames);

                        fdx = pre_player_x - fighters[i].x;
                        fdy = -pre_player_y + fighters[i].y;
                        
                        int16_t best_index = 0;
                        int32_t max_dot = -8388608;
                        
                        for (uint8_t j = 0; j < SHIP_ROTATION_STEPS; j++) {
                            int32_t current_dot = (int32_t)fdx * cos_fix[j] + (int32_t)fdy * sin_fix[j];
                            if (current_dot > max_dot) {
                                max_dot = current_dot;
                                best_index = j;
                            }
                        }
                        
                        ebullets[current_ebullet_index].status = best_index;
                        ebullets[current_ebullet_index].x = fighters[i].x;
                        ebullets[current_ebullet_index].y = fighters[i].y;
                        ebullets[current_ebullet_index].vx_rem = 0;
                        ebullets[current_ebullet_index].vy_rem = 0;
                        
                        unsigned bullet_ptr = EBULLET_CONFIG + current_ebullet_index * sizeof(vga_mode4_sprite_t);
                        xram0_struct_set(bullet_ptr, vga_mode4_sprite_t, x_pos_px, fighters[i].x);
                        xram0_struct_set(bullet_ptr, vga_mode4_sprite_t, y_pos_px, fighters[i].y);

                        play_sound(SFX_TYPE_ENEMY_FIRE, 440, PSG_WAVE_TRIANGLE, 0, 4, 3, 3);
                        
                        fighters[i].status = 2;
                        
                        current_ebullet_index++;
                        if (current_ebullet_index >= MAX_EBULLETS) {
                            current_ebullet_index = 0;
                        }
                        
                        break;
                    }
                }
            } else if (fighters[i].status > 1) {  // Cooling down after firing
                fighters[i].status++;
                if (fighters[i].status > max_ebullet_cooldown) {
                    fighters[i].status = 1;
                }
            }
        }
    }
}

void update_ebullets(void)
{
    // Decrement cooldown
    if (ebullet_cooldown > 0) {
        ebullet_cooldown--;
    }
    
    // Adjust for scrolling
    for (uint8_t i = 0; i < MAX_EBULLETS; i++) {
        if (ebullets[i].status >= 0) {
            ebullets[i].x -= scroll_dx;
            ebullets[i].y -= scroll_dy;
        }
    }
    
    for (uint8_t i = 0; i < MAX_EBULLETS; i++) {
        unsigned ptr = EBULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
        
        if (ebullets[i].status < 0) {
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            continue;
        }
        
        if (player_x < ebullets[i].x + 2 &&
            player_x + 8 > ebullets[i].x &&
            player_y < ebullets[i].y + 2 &&
            player_y + 8 > ebullets[i].y) {
            
            ebullets[i].status = -1;
            enemy_score++;
            
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            
            continue;
        }
        
        int16_t bvx = cos_fix[ebullets[i].status];
        int16_t bvy = -sin_fix[ebullets[i].status];
        
        int16_t bvx_applied = (bvx + ebullets[i].vx_rem) >> 6;
        int16_t bvy_applied = (bvy + ebullets[i].vy_rem) >> 6;
        
        ebullets[i].vx_rem = bvx + ebullets[i].vx_rem - (bvx_applied << 6);
        ebullets[i].vy_rem = bvy + ebullets[i].vy_rem - (bvy_applied << 6);
        
        ebullets[i].x += bvx_applied;
        ebullets[i].y += bvy_applied;
        
        if (ebullets[i].x > -10 && ebullets[i].x < SCREEN_WIDTH + 10 &&
            ebullets[i].y > -10 && ebullets[i].y < SCREEN_HEIGHT + 10) {
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, ebullets[i].x);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, ebullets[i].y);
        } else {
            ebullets[i].status = -1;
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
        }
    }
}

void render_fighters(void)
{
    for (uint8_t i = 0; i < MAX_FIGHTERS; i++) {
        if (fighters[i].status > 0 || fighters[i].is_exploding) {
            
            unsigned ptr = FIGHTER_CONFIG + i * sizeof(vga_mode4_sprite_t);

            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, fighters[i].x);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, fighters[i].y);
        } else {
            unsigned ptr = FIGHTER_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
        }
    }
}

void move_fighters_offscreen(void)
{
    for (uint8_t i = 0; i < MAX_FIGHTERS; i++) {
        if (fighters[i].status > 0) {
            unsigned ptr = FIGHTER_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            fighters[i].status = 0;
        }
    }
}

void move_ebullets_offscreen(void)
{
    for (uint8_t i = 0; i < MAX_EBULLETS; i++) {
        if (ebullets[i].status >= 0) {
            unsigned ptr = EBULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            ebullets[i].status = -1;
        }
    }
}

bool check_bullet_fighter_collision(int16_t bullet_x, int16_t bullet_y, 
                                     int16_t* player_score_out, int16_t* game_score_out)
{
    for (uint8_t f = 0; f < MAX_FIGHTERS; f++) {
        if (fighters[f].status > 0) {
            int16_t fighter_screen_x = fighters[f].x - scroll_dx;
            int16_t fighter_screen_y = fighters[f].y - scroll_dy;
            
            if (bullet_x >= fighter_screen_x - 2 && bullet_x < fighter_screen_x + 6 &&
                bullet_y >= fighter_screen_y - 2 && bullet_y < fighter_screen_y + 6) {
                
                fighters[f].status = 0;
                fighters[f].is_exploding = true; // Start explosion sequenc
                active_fighter_count--;
                
                // Award points based on current level
                *player_score_out += 1;
                *game_score_out += game_level;
                
                return true;
            }
        }
    }
    return false;
}

void decrement_ebullet_cooldown(void)
{
    if (ebullet_cooldown > 0) {
        ebullet_cooldown--;
    }
}

void increase_fighter_difficulty(void)
{
    max_ebullet_cooldown -= EBULLET_COOLDOWN_DECREASE;
    if (max_ebullet_cooldown < MIN_EBULLET_COOLDOWN) {
        max_ebullet_cooldown = MIN_EBULLET_COOLDOWN;
    }
    
    // Increase fighter speed range
    fighter_speed_max += FIGHTER_SPEED_INCREASE;
    if (fighter_speed_max > MAX_FIGHTER_SPEED) {
        fighter_speed_max = MAX_FIGHTER_SPEED;
    }
}

void reset_fighter_difficulty(void)
{
    max_ebullet_cooldown = INITIAL_EBULLET_COOLDOWN;
    fighter_speed_min = INITIAL_FIGHTER_SPEED_MIN;
    fighter_speed_max = INITIAL_FIGHTER_SPEED_MAX;
}
