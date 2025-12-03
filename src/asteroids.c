#include "asteroids.h"
#include "constants.h"      // Needs ASTEROID_M_DATA, game_frame
#include "player.h"         // Needs scroll_dx, scroll_dy
#include "random.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <rp6502.h>
#include <stdlib.h>

// Rotation Tables (Reuse from player.c)
extern const int16_t sin_fix[];
extern const int16_t cos_fix[];

const int16_t t2_fix32[] = {
       0, 1152, 2560, 4064, 5536, 6944, 8128, 9056, 9632, 9856, 
    9632, 9056, 8128, 6944, 5536, 4064, 2560, 1152,    0, -928, 
   -1504, -1728, -1504, -928,    0
};

#define MAX_ROTATION 24

// Globals
asteroid_t ast_l[MAX_AST_L];
asteroid_t ast_m[MAX_AST_M];
asteroid_t ast_s[MAX_AST_S];

// Config Addresses (From rpmegafighter.c)
extern unsigned ASTEROID_L_CONFIG;
extern unsigned ASTEROID_M_CONFIG;
extern unsigned ASTEROID_S_CONFIG;

extern int16_t scroll_dx, scroll_dy;

// Asteroid World Boundaries
#define AWORLD_PAD 25  // Extra padding beyond screen edges
#define AWORLD_X1 -AWORLD_PAD  // World boundaries
#define AWORLD_X2 (SCREEN_WIDTH + AWORLD_PAD)  // World boundaries
#define AWORLD_Y1 -AWORLD_PAD  // World boundaries
#define AWORLD_Y2 (SCREEN_HEIGHT + AWORLD_PAD)  // World boundaries
#define AWORLD_X (AWORLD_X2 - AWORLD_X1)  // Total world width
#define AWORLD_Y (AWORLD_Y2 - AWORLD_Y1)  // Total world height

// ---------------------------------------------------------
// INITIALIZATION
// ---------------------------------------------------------
void init_asteroids(void) {
    // 1. Reset Large (Affine)
    size_t size_l = sizeof(vga_mode4_asprite_t);
    for (int i=0; i<MAX_AST_L; i++) {
        ast_l[i].active = false;
        unsigned ptr = ASTEROID_L_CONFIG + (i * size_l);
        xram0_struct_set(ptr, vga_mode4_asprite_t, y_pos_px, -100); // Hide
    }
    
    // 2. Reset Medium (Standard)
    size_t size_std = sizeof(vga_mode4_sprite_t);
    for (int i=0; i<MAX_AST_M; i++) {
        ast_m[i].active = false;
        unsigned ptr = ASTEROID_M_CONFIG + (i * size_std);
        xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
    }
    
    // 3. Reset Small (Standard)
    for (int i=0; i<MAX_AST_S; i++) {
        ast_s[i].active = false;
        unsigned ptr = ASTEROID_S_CONFIG + (i * size_std);
        xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
    }
}

// ---------------------------------------------------------
// SPAWNING
// ---------------------------------------------------------
// Internal helper to setup a specific asteroid
static void activate_asteroid(asteroid_t *a, AsteroidType type) {
    a->active = true;
    a->type = type;
    a->rx = 0; 
    a->ry = 0;
    a->anim_frame = random(0, MAX_ROTATION); // Random start angle

    // Spawn at Random World Edge (-512 to +512)
    // 50% chance X-Edge, 50% chance Y-Edge
    if (rand16() & 1) {
        a->x = (rand16() & 1) ? AWORLD_X1 : AWORLD_X2;
        a->y = (int16_t)random(0, AWORLD_Y) + AWORLD_Y2;
    } else {
        a->x = (int16_t)random(0, AWORLD_X) + AWORLD_X2;
        a->y = (rand16() & 1) ? AWORLD_Y2 : AWORLD_Y2;
    }

    // Velocity (Slower for Large, Faster for Small)
    int speed_base = (type == AST_LARGE) ? 64 : ((type == AST_MEDIUM) ? 128 : 256);
    a->vx = (rand16() & 1) ? speed_base : -speed_base;
    a->vy = (rand16() & 1) ? speed_base : -speed_base;

    // Health
    if (type == AST_LARGE) a->health = 20;
    else if (type == AST_MEDIUM) a->health = 10;
    else a->health = 2;

    // --- INITIAL GPU CONFIG ---
    // We set static properties here so we don't have to set them every frame
    if (type == AST_LARGE) {
        // Find index in pool to get address
        int idx = a - ast_l; // Pointer math
        unsigned ptr = ASTEROID_L_CONFIG + (idx * sizeof(vga_mode4_asprite_t));
        
        xram0_struct_set(ptr, vga_mode4_asprite_t, xram_sprite_ptr, ASTEROID_L_DATA);
        xram0_struct_set(ptr, vga_mode4_asprite_t, log_size, 5); // 32x32
        xram0_struct_set(ptr, vga_mode4_asprite_t, has_opacity_metadata, false);
    } 
    // (Med/Small config handled in update or similar block)
}

void spawn_asteroid_wave(int level) {
    // Only spawn Large for now
    // 2% chance per frame to try spawning
    if (rand16() % 100 < 2) {
        for (int i = 0; i < MAX_AST_L; i++) {
            if (!ast_l[i].active) {
                activate_asteroid(&ast_l[i], AST_LARGE);
                printf("Spawned Large Asteroid %d at %d, %d\n", i, ast_l[i].x, ast_l[i].y);
                break; // Only spawn one per frame
            }
        }
    }
}

// ---------------------------------------------------------
// UPDATE & RENDER
// ---------------------------------------------------------
static void update_single(asteroid_t *a, int index, unsigned base_cfg, int size_bytes) {
    // 1. Movement (Fixed Point)
    a->rx += a->vx; if (a->rx >= 256) { a->x++; a->rx -= 256; } else if (a->rx <= -256) { a->x--; a->rx += 256; }
    a->ry += a->vy; if (a->ry >= 256) { a->y++; a->ry -= 256; } else if (a->ry <= -256) { a->y--; a->ry += 256; }

    // 2. World Wrap (AWORLD_X1 to AWORLD_X2) & (AWORLD_Y1 to AWORLD_Y2)
    if (a->x < AWORLD_X1) a->x += AWORLD_X; else if (a->x > AWORLD_X2) a->x -= AWORLD_X;
    if (a->y < AWORLD_Y1) a->y += AWORLD_Y; else if (a->y > AWORLD_Y2) a->y -= AWORLD_Y;


    a->x -= scroll_dx;
    a->y -= scroll_dy;

    // 3. Render
    int sx = a->x;
    int sy = a->y;
    unsigned ptr = base_cfg + (index * size_bytes);

    if (a->type == AST_LARGE) {
        // --- LARGE (Affine Plane 1) ---
        // Rotate every 8th frame
        // printf("Game Frame: %d\n", game_frame);
        if (game_frame % 8 == 0) {
            // Alternate direction based on index (i)
            if (index & 1) {
                a->anim_frame++; // Spin Clockwise
                if (a->anim_frame >= MAX_ROTATION) a->anim_frame = 0;
            } else {
                a->anim_frame--; // Spin Counter-Clockwise
                if (a->anim_frame >= 250) a->anim_frame = MAX_ROTATION - 1; // Handle wrap
            }
        }
        int r = a->anim_frame; 

        // Update Matrix (Rotation)
        xram0_struct_set(ptr, vga_mode4_asprite_t, transform[0],  cos_fix[r]); // SX
        xram0_struct_set(ptr, vga_mode4_asprite_t, transform[1], -sin_fix[r]); // SHY
        xram0_struct_set(ptr, vga_mode4_asprite_t, transform[3],  sin_fix[r]); // SHX
        xram0_struct_set(ptr, vga_mode4_asprite_t, transform[4],  cos_fix[r]); // SY

        int16_t tx = t2_fix32[r]; 
        
        // TY uses the inverse angle (24 - r)
        // Check bounds just in case r > 24
        int y_idx = (MAX_ROTATION - r);
        if (y_idx < 0) y_idx += MAX_ROTATION; // Safety wrap
        int16_t ty = t2_fix32[y_idx];

        xram0_struct_set(ptr, vga_mode4_asprite_t, transform[2], tx); // TX
        xram0_struct_set(ptr, vga_mode4_asprite_t, transform[5], ty); // TY

        xram0_struct_set(ptr, vga_mode4_asprite_t, x_pos_px, sx);
        xram0_struct_set(ptr, vga_mode4_asprite_t, y_pos_px, sy);
    } 
    else {
        // --- MED/SMALL (Standard Plane 2) ---
        // Just position (no rotation logic yet)
        xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, sx);
        xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, sy);
        
        // Ensure data ptr is set (simple safeguard)
        uint16_t data = (a->type == AST_MEDIUM) ? ASTEROID_M_DATA : ASTEROID_S_DATA;
        uint8_t lsize = (a->type == AST_MEDIUM) ? 4 : 3;
        
        xram0_struct_set(ptr, vga_mode4_sprite_t, xram_sprite_ptr, data);
        xram0_struct_set(ptr, vga_mode4_sprite_t, log_size, lsize);
        xram0_struct_set(ptr, vga_mode4_sprite_t, has_opacity_metadata, false);
    }
}

void update_asteroids(void) {
    // Loop through pools
    for(int i=0; i<MAX_AST_L; i++) {
        if (ast_l[i].active) update_single(&ast_l[i], i, ASTEROID_L_CONFIG, sizeof(vga_mode4_asprite_t));
    }
    for(int i=0; i<MAX_AST_M; i++) {
        if (ast_m[i].active) update_single(&ast_m[i], i, ASTEROID_M_CONFIG, sizeof(vga_mode4_sprite_t));
    }
    for(int i=0; i<MAX_AST_S; i++) {
        if (ast_s[i].active) update_single(&ast_s[i], i, ASTEROID_S_CONFIG, sizeof(vga_mode4_sprite_t));
    }
}

// ---------------------------------------------------------
// SPLITTING LOGIC
// ---------------------------------------------------------

// Helper to spawn a child asteroid at a specific spot with specific velocity
static void spawn_child(AsteroidType type, int16_t x, int16_t y, int16_t vx, int16_t vy) {
    asteroid_t *pool;
    int max_count;
    
    // Select Pool
    if (type == AST_MEDIUM) { pool = ast_m; max_count = MAX_AST_M; }
    else { pool = ast_s; max_count = MAX_AST_S; }

    // Find free slot
    for (int i = 0; i < max_count; i++) {
        if (!pool[i].active) {
            pool[i].active = true;
            pool[i].type = type;
            pool[i].x = x;
            pool[i].y = y;
            pool[i].rx = 0; 
            pool[i].ry = 0;
            pool[i].vx = vx;
            pool[i].vy = vy;
            pool[i].anim_frame = 0;
            
            // Set Health
            pool[i].health = (type == AST_MEDIUM) ? 6 : 1;
            
            // Set Config Immediately (So it doesn't wait for next update frame)
            unsigned ptr;
            if (type == AST_MEDIUM) {
                ptr = ASTEROID_M_CONFIG + (i * sizeof(vga_mode4_sprite_t));
                xram0_struct_set(ptr, vga_mode4_sprite_t, xram_sprite_ptr, ASTEROID_M_DATA);
                xram0_struct_set(ptr, vga_mode4_sprite_t, log_size, 4); // 16x16
            } else {
                ptr = ASTEROID_S_CONFIG + (i * sizeof(vga_mode4_sprite_t));
                xram0_struct_set(ptr, vga_mode4_sprite_t, xram_sprite_ptr, ASTEROID_S_DATA);
                xram0_struct_set(ptr, vga_mode4_sprite_t, log_size, 3); // 8x8
            }
            xram0_struct_set(ptr, vga_mode4_sprite_t, has_opacity_metadata, false);

            printf("Spawning Child Type %d at %d,%d (Slot %d)\n", type, x, y, i);
            
            return; // Spawned successfully
        }
    }
}

// ---------------------------------------------------------
// COLLISION LOGIC
// ---------------------------------------------------------

bool check_asteroid_hit(int16_t bx, int16_t by) {
    // 1. Check LARGE Asteroids (Radius ~14px)
    for (int i = 0; i < MAX_AST_L; i++) {
        if (!ast_l[i].active) continue;
        
        // Simple Box Check (Faster than distance calc)
        if (abs(ast_l[i].x - bx) < 14 && abs(ast_l[i].y - by) < 14) {
            ast_l[i].health--;
            
            if (ast_l[i].health <= 0) {
                // DESTROY LARGE -> Spawn 2 Mediums
                ast_l[i].active = false;
                // start_explosion(ast_l[i].x, ast_l[i].y);
                player_score += 5;

                // Split velocities (diverge from parent)
                // Parent velocity +/- 30 subpixels
                spawn_child(AST_MEDIUM, ast_l[i].x, ast_l[i].y, ast_l[i].vx + 128, ast_l[i].vy - 128);
                spawn_child(AST_MEDIUM, ast_l[i].x, ast_l[i].y, ast_l[i].vx - 128, ast_l[i].vy + 128);
                
                // Hide sprite immediately
                unsigned ptr = ASTEROID_L_CONFIG + (i * sizeof(vga_mode4_asprite_t));
                xram0_struct_set(ptr, vga_mode4_asprite_t, y_pos_px, -100);
            }
            return true; // Bullet hit something
        }
    }

    // 2. Check MEDIUM Asteroids (Radius ~7px)
    for (int i = 0; i < MAX_AST_M; i++) {
        if (!ast_m[i].active) continue;
        
        if (abs(ast_m[i].x - bx) < 8 && abs(ast_m[i].y - by) < 8) {
            ast_m[i].health--;
            
            if (ast_m[i].health <= 0) {
                // DESTROY MEDIUM -> Spawn 2 Smalls
                ast_m[i].active = false;
                // start_explosion(ast_m[i].x, ast_m[i].y);
                player_score += 2;

                // Make small ones fast! (+/- 60 subpixels)
                spawn_child(AST_SMALL, ast_m[i].x, ast_m[i].y, ast_m[i].vx + 128, ast_m[i].vy + 128);
                spawn_child(AST_SMALL, ast_m[i].x, ast_m[i].y, ast_m[i].vx - 128, ast_m[i].vy - 128);

                unsigned ptr = ASTEROID_M_CONFIG + (i * sizeof(vga_mode4_sprite_t));
                xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            }
            return true;
        }
    }

    // 3. Check SMALL Asteroids (Radius ~4px)
    for (int i = 0; i < MAX_AST_S; i++) {
        if (!ast_s[i].active) continue;
        
        if (abs(ast_s[i].x - bx) < 5 && abs(ast_s[i].y - by) < 5) {
            ast_s[i].health--; // Usually 1 hit kill
            
            if (ast_s[i].health <= 0) {
                // DESTROY SMALL -> Dust
                ast_s[i].active = false;
                // start_explosion(ast_s[i].x, ast_s[i].y);
                player_score += 1;

                unsigned ptr = ASTEROID_S_CONFIG + (i * sizeof(vga_mode4_sprite_t));
                xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            }
            return true;
        }
    }

    return false;
}