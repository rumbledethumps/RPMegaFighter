#include "fighters.h"
#include "random.h"
#include "sound.h"
#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       180
#define SCREEN_WIDTH_D2     160
#define SCREEN_HEIGHT_D2    90

#define MAX_FIGHTERS        30
#define FIGHTER_SPAWN_RATE  128
#define EFIRE_COOLDOWN_TIMER 16

#define MAX_EBULLETS        16
#define INITIAL_EBULLET_COOLDOWN 10
#define MIN_EBULLET_COOLDOWN     1
#define EBULLET_COOLDOWN_DECREASE 2

#define SHIP_ROTATION_STEPS 24

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
} Fighter;

// ============================================================================
// EXTERNAL DEPENDENCIES
// ============================================================================

// Game state from main
extern int16_t player_x, player_y;
extern int16_t player_vx_applied, player_vy_applied;
extern int16_t world_offset_x, world_offset_y;
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
extern const uint8_t ri_max;

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

// ============================================================================
// FUNCTIONS
// ============================================================================

void init_fighters(void)
{
    for (uint8_t i = 0; i < MAX_FIGHTERS; i++) {
        fighters[i].vx_i = random(16, 256);
        fighters[i].vy_i = random(16, 256);
        fighters[i].vx = 0;
        fighters[i].vy = 0;
        fighters[i].status = 1;
        
        fighters[i].x = random(SCREEN_WIDTH_D2, SCREEN_WIDTH) + SCREEN_WIDTH + 144;
        fighters[i].y = random(SCREEN_HEIGHT_D2, SCREEN_HEIGHT) + SCREEN_HEIGHT + 104;
        
        if (random(0, 1)) {
            fighters[i].x = -fighters[i].x;
        }
        if (random(0, 1)) {
            fighters[i].y = -fighters[i].y;
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
    
    int16_t player_world_x = player_x + world_offset_x;
    int16_t player_world_y = player_y + world_offset_y;
    
    for (uint8_t i = 0; i < MAX_FIGHTERS; i++) {
        if (fighters[i].status <= 0) {
            fighters[i].status--;
            if (fighters[i].status <= -FIGHTER_SPAWN_RATE) {
                fighters[i].vx_i = random(16, 256);
                fighters[i].vy_i = random(16, 256);
                
                uint8_t edge = random(0, 3);
                
                if (edge == 0) {
                    fighters[i].x = world_offset_x + SCREEN_WIDTH + random(20, 100);
                    fighters[i].y = world_offset_y + random(20, SCREEN_HEIGHT - 20);
                } else if (edge == 1) {
                    fighters[i].x = world_offset_x - random(20, 100);
                    fighters[i].y = world_offset_y + random(20, SCREEN_HEIGHT - 20);
                } else if (edge == 2) {
                    fighters[i].x = world_offset_x + random(20, SCREEN_WIDTH - 20);
                    fighters[i].y = world_offset_y + SCREEN_HEIGHT + random(20, 100);
                } else {
                    fighters[i].x = world_offset_x + random(20, SCREEN_WIDTH - 20);
                    fighters[i].y = world_offset_y - random(20, 100);
                }
                
                fighters[i].status = 1;
                active_fighter_count++;
            }
            continue;
        }
        
        int16_t fighter_screen_x = fighters[i].x - world_offset_x;
        int16_t fighter_screen_y = fighters[i].y - world_offset_y;
        
        if (fighter_screen_x + 4 > player_x && fighter_screen_x < player_x + 8 &&
            fighter_screen_y + 4 > player_y && fighter_screen_y < player_y + 8) {
            fighters[i].status = 0;
            active_fighter_count--;
            enemy_score += 2;
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
        
        int16_t fighter_offset_x = fighters[i].x - world_offset_x;
        int16_t fighter_offset_y = fighters[i].y - world_offset_y;
        
        if (fighter_offset_x > SCREEN_WIDTH + 200) {
            fighters[i].x = world_offset_x - 150;
        } else if (fighter_offset_x < -200) {
            fighters[i].x = world_offset_x + SCREEN_WIDTH + 150;
        }
        
        if (fighter_offset_y > SCREEN_HEIGHT + 200) {
            fighters[i].y = world_offset_y - 150;
        } else if (fighter_offset_y < -200) {
            fighters[i].y = world_offset_y + SCREEN_HEIGHT + 150;
        }
    }
}

void fire_ebullet(void)
{
    if (ebullet_cooldown > 0) {
        return;
    }
    
    ebullet_cooldown = max_ebullet_cooldown;
    
    int16_t player_world_x = player_x + world_offset_x;
    int16_t player_world_y = player_y + world_offset_y;
    
    if (ebullets[current_ebullet_index].status < 0) {
        for (uint8_t i = 0; i < MAX_FIGHTERS; i++) {
            if (fighters[i].status == 1) {
                int16_t screen_x = fighters[i].x - world_offset_x;
                int16_t screen_y = fighters[i].y - world_offset_y;
                
                if (screen_x > 0 && screen_x < SCREEN_WIDTH - 4 &&
                    screen_y > 0 && screen_y < SCREEN_HEIGHT - 4) {
                    
                    int16_t fdx = player_world_x - fighters[i].x;
                    int16_t fdy = -(player_world_y - fighters[i].y);
                    int16_t distance = abs(fdx) + abs(fdy);
                    
                    if (distance > 0) {
                        int16_t tti_frames = distance / 4;
                        if (tti_frames == 0) tti_frames = 1;
                        
                        int16_t pre_player_x = player_world_x + 4 + (player_vx_applied * tti_frames);
                        int16_t pre_player_y = player_world_y + 4 + (player_vy_applied * tti_frames);
                        
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
                        ebullets[current_ebullet_index].x = screen_x;
                        ebullets[current_ebullet_index].y = screen_y;
                        ebullets[current_ebullet_index].vx_rem = 0;
                        ebullets[current_ebullet_index].vy_rem = 0;
                        
                        unsigned bullet_ptr = EBULLET_CONFIG + current_ebullet_index * sizeof(vga_mode4_sprite_t);
                        xram0_struct_set(bullet_ptr, vga_mode4_sprite_t, x_pos_px, screen_x);
                        xram0_struct_set(bullet_ptr, vga_mode4_sprite_t, y_pos_px, screen_y);
                        
                        play_sound(SFX_TYPE_ENEMY_FIRE, 440, PSG_WAVE_TRIANGLE, 0, 4, 3, 2);
                        
                        fighters[i].status = 2;
                        
                        current_ebullet_index++;
                        if (current_ebullet_index >= MAX_EBULLETS) {
                            current_ebullet_index = 0;
                        }
                        
                        break;
                    }
                }
            } else if (fighters[i].status > 1) {
                fighters[i].status++;
                if (fighters[i].status > EFIRE_COOLDOWN_TIMER) {
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
        if (fighters[i].status > 0) {
            int16_t screen_x = fighters[i].x - world_offset_x;
            int16_t screen_y = fighters[i].y - world_offset_y;
            
            unsigned ptr = FIGHTER_CONFIG + i * sizeof(vga_mode4_sprite_t);
            
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, screen_x);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, screen_y);
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
            int16_t fighter_screen_x = fighters[f].x - world_offset_x;
            int16_t fighter_screen_y = fighters[f].y - world_offset_y;
            
            if (bullet_x >= fighter_screen_x - 2 && bullet_x < fighter_screen_x + 6 &&
                bullet_y >= fighter_screen_y - 2 && bullet_y < fighter_screen_y + 6) {
                
                fighters[f].status = 0;
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
}
