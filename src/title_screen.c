#include "title_screen.h"
#include <rp6502.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

// Define SWIDTH before including graphics.h
#define SWIDTH 320

#include "graphics.h"
#include "usb_hid_keys.h"

// External references
extern void draw_text(uint16_t x, uint16_t y, const char *str, uint8_t colour);
extern void clear_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
extern void draw_high_scores(void);
extern const uint16_t vlen;

// Gamepad structure and constants
typedef struct {
    uint8_t dpad;
    uint8_t sticks;
    uint8_t btn0;
    uint8_t btn1;
    int8_t lx;         // Signed for analog sticks
    int8_t ly;
    int8_t rx;
    int8_t ry;
    uint8_t l2;
    uint8_t r2;
} gamepad_t;

#define GAMEPAD_COUNT 4
#define GP_CONNECTED 0x80
#define KEYBOARD_INPUT 0xEC20
#define GAMEPAD_INPUT 0xEC50
#define KEYBOARD_BYTES 32

extern gamepad_t gamepad[GAMEPAD_COUNT];
extern uint8_t keystates[KEYBOARD_BYTES];

// Key macro
#define key(k) (keystates[(k)>>3] & (1 << ((k) & 7)))

void show_title_screen(void)
{
    const uint8_t red_color = 0x03;      // Pure red
    const uint8_t blue_color = 0x1F;     // Blue
    const uint16_t center_x = 110;       // X position for centered text
    
    // Clear screen
    RIA.addr0 = 0;
    RIA.step0 = 1;
    for (unsigned i = vlen; i--;) {
        RIA.rw0 = 0;
    }
    
    // Draw title text
    draw_text(center_x, 40, "MEGA", red_color);
    draw_text(center_x, 55, "SUPER", red_color);
    draw_text(center_x, 70, "FIGHTER", red_color);
    draw_text(center_x, 85, "CHALLENGE", blue_color);
    
    // Draw high scores on right side
    draw_high_scores();
    
    uint8_t vsync_last = RIA.vsync;
    uint16_t flash_counter = 0;
    bool press_start_visible = true;
    const uint16_t flash_interval = 30;  // 0.5 seconds at 60 Hz
    uint8_t current_color = red_color;
    
    // Draw initial "PRESS START" text
    draw_text(center_x - 20, 110, "PRESS START", red_color);
    
    // Draw exit instruction
    draw_text(center_x - 30, 130, "PUSH A+C TO EXIT", blue_color);
    
    printf("Title screen displayed. Press START to begin...\n");
    
    // Title screen loop - wait for START button
    bool start_button_was_pressed = false;  // Track button state for edge detection
    while (true) {
        // Wait for vertical sync
        if (RIA.vsync == vsync_last)
            continue;
        vsync_last = RIA.vsync;
        
        // Read input
        RIA.addr0 = KEYBOARD_INPUT;
        RIA.step0 = 2;
        keystates[0] = RIA.rw0;
        RIA.step0 = 1;
        keystates[2] = RIA.rw0;
        
        RIA.addr0 = GAMEPAD_INPUT;
        RIA.step0 = 1;
        for (uint8_t i = 0; i < GAMEPAD_COUNT; i++) {
            gamepad[i].dpad = RIA.rw0;
            gamepad[i].sticks = RIA.rw0;
            gamepad[i].btn0 = RIA.rw0;
            gamepad[i].btn1 = RIA.rw0;
            gamepad[i].lx = RIA.rw0;
            gamepad[i].ly = RIA.rw0;
            gamepad[i].rx = RIA.rw0;
            gamepad[i].ry = RIA.rw0;
            gamepad[i].l2 = RIA.rw0;
            gamepad[i].r2 = RIA.rw0;
        }
        
        // Check for START button (BTN1 bit 0x02)
        if (gamepad[0].dpad & GP_CONNECTED) {
            if (gamepad[0].btn1 & 0x02) {
                // Button is currently pressed
                if (!start_button_was_pressed) {
                    // This is a new press (edge detection)
                    start_button_was_pressed = true;
                    // Clear entire screen before exiting
                    RIA.addr0 = 0;
                    RIA.step0 = 1;
                    for (unsigned i = vlen; i--;) {
                        RIA.rw0 = 0;
                    }
                    printf("START pressed - beginning game!\n");
                    
                    // Wait for START button to be released before exiting
                    while (true) {
                        if (RIA.vsync == vsync_last)
                            continue;
                        vsync_last = RIA.vsync;
                        
                        RIA.addr0 = GAMEPAD_INPUT;
                        RIA.step0 = 1;
                        for (uint8_t i = 0; i < GAMEPAD_COUNT; i++) {
                            gamepad[i].dpad = RIA.rw0;
                            gamepad[i].sticks = RIA.rw0;
                            gamepad[i].btn0 = RIA.rw0;
                            gamepad[i].btn1 = RIA.rw0;
                            gamepad[i].lx = RIA.rw0;
                            gamepad[i].ly = RIA.rw0;
                            gamepad[i].rx = RIA.rw0;
                            gamepad[i].ry = RIA.rw0;
                            gamepad[i].l2 = RIA.rw0;
                            gamepad[i].r2 = RIA.rw0;
                        }
                        
                        // Exit loop when START button is released
                        if (!(gamepad[0].btn1 & 0x02)) {
                            break;
                        }
                    }
                    
                    return;  // Exit title screen
                }
            } else {
                // Button is not pressed
                start_button_was_pressed = false;
            }
        }
        
        // Check for A+C buttons pressed together to exit (0x04 + 0x20 = 0x24)
        if ((gamepad[0].btn0 & 0x04) && (gamepad[0].btn0 & 0x20)) {
            printf("A+C pressed - exiting...\n");
            exit(0);
        }
        
        // Check for ESC to exit game
        if (key(KEY_ESC)) {
            printf("ESC pressed - exiting...\n");
            exit(0);
        }
        
        // Flash "PRESS START" every 5 seconds
        flash_counter++;
        if (flash_counter >= flash_interval) {
            flash_counter = 0;
            press_start_visible = !press_start_visible;
            
            if (press_start_visible) {
                // Alternate color between red and blue
                current_color = (current_color == red_color) ? blue_color : red_color;
                draw_text(center_x - 20, 110, "PRESS START", current_color);
            } else {
                // Clear the text
                clear_rect(center_x - 20, 110, 120, 5);
            }
        }
    }
}
