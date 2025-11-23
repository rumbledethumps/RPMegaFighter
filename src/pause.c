#include "pause.h"
#include <rp6502.h>
#include <stdio.h>

// Graphics constants
#define SWIDTH 320

#include "graphics.h"

// External references
extern void draw_text(uint16_t x, uint16_t y, const char *str, uint8_t colour);
extern void clear_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

// Gamepad structure and constants
typedef struct {
    uint8_t dpad;
    uint8_t sticks;
    uint8_t btn0;
    uint8_t btn1;
    uint8_t lx;
    uint8_t ly;
    uint8_t rx;
    uint8_t ry;
    uint8_t l2;
    uint8_t r2;
} gamepad_t;

#define GP_CONNECTED 0x80

extern gamepad_t gamepad[4];

// Pause state
static bool game_paused = false;
static bool start_button_pressed = false;  // For edge detection

void display_pause_message(bool show_paused)
{
    const uint8_t pause_color = 0xFF;
    const uint16_t center_x = 120;
    const uint16_t center_y = 85;
    
    if (show_paused) {
        // Draw "PAUSED" using simple block letters
        // P
        for (uint16_t x = center_x; x < center_x + 3; x++) {
            for (uint16_t y = center_y; y < center_y + 12; y++) {
                set(x, y, pause_color);
            }
        }
        for (uint16_t x = center_x; x < center_x + 8; x++) {
            set(x, center_y, pause_color);
            set(x, center_y + 6, pause_color);
        }
        for (uint16_t y = center_y; y < center_y + 7; y++) {
            set(center_x + 8, y, pause_color);
        }
        
        // A
        for (uint16_t y = center_y + 3; y < center_y + 12; y++) {
            set(center_x + 12, y, pause_color);
            set(center_x + 20, y, pause_color);
        }
        for (uint16_t x = center_x + 12; x < center_x + 21; x++) {
            set(x, center_y + 3, pause_color);
            set(x, center_y + 7, pause_color);
        }
        
        // U
        for (uint16_t y = center_y; y < center_y + 12; y++) {
            set(center_x + 24, y, pause_color);
            set(center_x + 32, y, pause_color);
        }
        for (uint16_t x = center_x + 24; x < center_x + 33; x++) {
            set(x, center_y + 11, pause_color);
        }
        
        // S
        for (uint16_t x = center_x + 36; x < center_x + 44; x++) {
            set(x, center_y, pause_color);
            set(x, center_y + 6, pause_color);
            set(x, center_y + 11, pause_color);
        }
        for (uint16_t y = center_y; y < center_y + 7; y++) {
            set(center_x + 36, y, pause_color);
        }
        for (uint16_t y = center_y + 6; y < center_y + 12; y++) {
            set(center_x + 44, y, pause_color);
        }
        
        // E
        for (uint16_t y = center_y; y < center_y + 12; y++) {
            set(center_x + 48, y, pause_color);
        }
        for (uint16_t x = center_x + 48; x < center_x + 56; x++) {
            set(x, center_y, pause_color);
            set(x, center_y + 6, pause_color);
            set(x, center_y + 11, pause_color);
        }
        
        // D
        for (uint16_t y = center_y; y < center_y + 12; y++) {
            set(center_x + 60, y, pause_color);
        }
        for (uint16_t x = center_x + 60; x < center_x + 67; x++) {
            set(x, center_y, pause_color);
            set(x, center_y + 11, pause_color);
        }
        for (uint16_t y = center_y + 1; y < center_y + 11; y++) {
            set(center_x + 67, y, pause_color);
        }
        
        // Draw "PUSH A+C TO EXIT" below PAUSED
        draw_text(center_x - 10, center_y + 20, "PUSH A+C TO EXIT", pause_color);
    } else {
        // Clear PAUSED text by drawing black
        for (uint16_t x = center_x; x < center_x + 68; x++) {
            for (uint16_t y = center_y; y < center_y + 12; y++) {
                set(x, y, 0x00);
            }
        }
        
        // Clear "PUSH A+C TO EXIT" text
        clear_rect(center_x - 10, center_y + 20, 90, 5);
    }
}

void handle_pause_input(void)
{
    // Handle pause button (Start) - BTN1 bit 0x02
    if (gamepad[0].dpad & GP_CONNECTED) {
        if (gamepad[0].btn1 & 0x02) {  // START button = 0x02 in BTN1
            if (!start_button_pressed) {
                game_paused = !game_paused;
                display_pause_message(game_paused);
                start_button_pressed = true;
                printf("\nGame %s\n", game_paused ? "PAUSED" : "RESUMED");
            }
        } else {
            start_button_pressed = false;
        }
    }
}

bool is_game_paused(void)
{
    return game_paused;
}

void set_game_paused(bool paused)
{
    game_paused = paused;
}

void reset_pause_state(void)
{
    game_paused = false;
    start_button_pressed = false;
}

bool check_pause_exit(void)
{
    // Check for A+C buttons pressed together to exit (0x04 + 0x20 = 0x24)
    if ((gamepad[0].btn0 & 0x04) && (gamepad[0].btn0 & 0x20)) {
        return true;
    }
    return false;
}
