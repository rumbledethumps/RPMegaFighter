#include "constants.h"
#include "pause.h"
#include "screen.h"
#include <rp6502.h>
#include <stdio.h>
#include "input.h"

#include "graphics.h"

// External references
extern void draw_text(uint16_t x, uint16_t y, const char *str, uint8_t colour);
extern void clear_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

// Gamepad structure and constants
// typedef struct {
//     uint8_t dpad;
//     uint8_t sticks;
//     uint8_t btn0;
//     uint8_t btn1;
//     int8_t lx;         // Signed for analog sticks
//     int8_t ly;
//     int8_t rx;
//     int8_t ry;
//     uint8_t l2;
//     uint8_t r2;
// } gamepad_t;

extern gamepad_t gamepad[4];

// Keyboard support
extern uint8_t keystates[KEYBOARD_BYTES];
#define key(code) (keystates[code >> 3] & (1 << (code & 7)))

// Pause state
static bool game_paused = false;
static bool start_button_pressed = false;  // For edge detection

void display_pause_message(bool show_paused)
{
    const uint8_t pause_color = 0xFF;
    const uint8_t exit_color = 0x03;  // Red for exit message
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
            set(center_x + 40 + 8, y, pause_color);
        }
        
        // Add exit instruction below PAUSED
        extern void draw_text(uint16_t x, uint16_t y, const char *str, uint8_t colour);
        draw_text(center_x + 10, center_y + 20, "ESC TO EXIT GAME", exit_color);
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
        
        // Add exit instruction below PAUSED
        // extern void draw_text(uint16_t x, uint16_t y, const char *str, uint8_t colour);
        // draw_text(center_x - 30, center_y + 20, "A+Y TO EXIT", exit_color);
    } else {
        // Clear the entire pause area
        extern void clear_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
        clear_rect(center_x - 5, center_y - 5, 80, 30);
    }
}

void handle_pause_input(void)
{
    bool pause_button_pressed = false;
    
    // // Check keyboard ENTER key
    // if (key(KEY_ENTER)) {
    //     pause_button_pressed = true;
    // }
    
    // Check gamepad START button (BTN1 bit 0x08)
    if (gamepad[0].dpad & GP_CONNECTED) {
        if (is_action_pressed(0, ACTION_PAUSE)) {
            pause_button_pressed = true;
        }
    }
    
    // Handle pause toggle with edge detection
    if (pause_button_pressed) {
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
    // Check for A+Y buttons pressed together to exit
    // if ((gamepad[0].btn0 & GP_BTN_A) && (gamepad[0].btn0 & GP_BTN_Y)) {
    //     return true;
    // }
    return false;
}
