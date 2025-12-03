#include "explosions.h"
#include "constants.h" // EXPLOSION_DATA
#include "player.h" // scroll_x, scroll_y, random
#include "random.h"
#include <rp6502.h>
#include <stdlib.h>

explosion_t explosions[MAX_EXPLOSIONS];
extern unsigned EXPLOSION_CONFIG;

// ---------------------------------------------------------
// INIT
// ---------------------------------------------------------
void init_explosions(void) {
    size_t size = sizeof(vga_mode4_sprite_t);
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        explosions[i].active = false;
        
        unsigned ptr = EXPLOSION_CONFIG + (i * size);
        xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
    }
}

// ---------------------------------------------------------
// SPAWN
// ---------------------------------------------------------
void start_explosion(int16_t x, int16_t y) {
    int particles_spawned = 0;
    size_t size = sizeof(vga_mode4_sprite_t);
    
    // Try to spawn 4 particles for a nice cluster
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].active) {
            explosions[i].active = true;
            
            // Random scatter (-4 to +4 pixels)
            explosions[i].x = x + (int16_t)random(0, 8) - 4;
            explosions[i].y = y + (int16_t)random(0, 8) - 4;
            
            // Random Velocity (Explode outward)
            explosions[i].vx = (rand16() & 1) ? random(10, 40) : -random(10, 40);
            explosions[i].vy = (rand16() & 1) ? random(10, 40) : -random(10, 40);
            
            // Start at frame 2 (skip the "ship" frames 0/1)
            explosions[i].frame = 2; 
            explosions[i].timer = 0;

            // --- CONFIG (Standard Sprite) ---
            unsigned ptr = EXPLOSION_CONFIG + (i * size);
            
            // Calculate offset: 4x4 sprite = 16 pixels * 2 bytes = 32 bytes per frame
            uint16_t offset = 2 * 32; 

            xram0_struct_set(ptr, vga_mode4_sprite_t, xram_sprite_ptr,(uint16_t)(EXPLOSION_DATA + offset));
            xram0_struct_set(ptr, vga_mode4_sprite_t, log_size, 2); // 4x4
            xram0_struct_set(ptr, vga_mode4_sprite_t, has_opacity_metadata, false);
            
            // Note: Position is set in update loop, or can set here initially
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, explosions[i].x);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, explosions[i].y);

            particles_spawned++;
            if (particles_spawned >= 4) break; 
        }
    }
}

// ---------------------------------------------------------
// UPDATE
// ---------------------------------------------------------
void update_explosions(void) {
    size_t size = sizeof(vga_mode4_sprite_t);

    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].active) continue;

        // Move (Simple integer math for particles)
        // Divide by 10 to slow down the subpixel velocity
        explosions[i].x += (explosions[i].vx / 10); 
        explosions[i].y += (explosions[i].vy / 10);

        // Animation Timer
        explosions[i].timer++;
        if (explosions[i].timer > 4) { // Change frame every 4 ticks
            explosions[i].timer = 0;
            explosions[i].frame++;
            
            // Asset has 8 frames total (0-7). We use 2-7.
            if (explosions[i].frame >= 8) {
                // Done
                explosions[i].active = false;
                unsigned ptr = EXPLOSION_CONFIG + (i * size);
                xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
                continue;
            }
            
            // Update Pointer
            unsigned ptr = EXPLOSION_CONFIG + (i * size);
            uint16_t offset = explosions[i].frame * 32; 
            xram0_struct_set(ptr, vga_mode4_sprite_t, xram_sprite_ptr, (uint16_t)(EXPLOSION_DATA + offset));
        }

        // Render
        explosions[i].x -= scroll_dx;
        explosions[i].y -= scroll_dy;
        
        unsigned ptr = EXPLOSION_CONFIG + (i * size);
        xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, explosions[i].x);
        xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, explosions[i].y);
    }
}
