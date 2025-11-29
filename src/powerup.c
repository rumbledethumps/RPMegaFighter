#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "powerup.h"
#include "player.h"

powerup_t powerup = { .active = false, .timer = 0 };

void render_powerup(void)
{

    if (powerup.active == false) {
        return;
    }
    xram0_struct_set(POWERUP_CONFIG, vga_mode4_sprite_t, x_pos_px, powerup.x);
    xram0_struct_set(POWERUP_CONFIG, vga_mode4_sprite_t, y_pos_px, powerup.y);

    return;
}

void update_powerup(void)
{
    if (powerup.active == false) {
        return;
    }

    // Update power-up position
    powerup.y += powerup.vy;
    powerup.x -= scroll_dx;
    powerup.y -= scroll_dy;

    // Check for collision with player (simple bounding box)
    if ((powerup.x < player_x + 16) && (powerup.x + 8 > player_x) &&
        (powerup.y < player_y + 16) && (powerup.y + 8 > player_y)) {
        // Player collected power-up
        powerup.active = false;
        // Move power-up sprite offscreen
        xram0_struct_set(POWERUP_CONFIG, vga_mode4_sprite_t, x_pos_px, -100);
        xram0_struct_set(POWERUP_CONFIG, vga_mode4_sprite_t, y_pos_px, -100); 
        {
            // Grant power-up effect to player (e.g., extra life, weapon upgrade)
            // This is a placeholder; actual implementation depends on game design
        }
        return;
    }

    // Decrease timer
    powerup.timer--;
    if (powerup.timer <= 0) {
        powerup.active = false;
        // Move power-up sprite offscreen
        xram0_struct_set(POWERUP_CONFIG, vga_mode4_sprite_t, x_pos_px, -100);
        xram0_struct_set(POWERUP_CONFIG, vga_mode4_sprite_t, y_pos_px, -100);
    }
}
