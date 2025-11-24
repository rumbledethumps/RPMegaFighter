#include "title_screen.h"
#include "screen.h"
#include "sbullets.h"
#include "music.h"
#include <rp6502.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

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

#define GP_CONNECTED 0x80
#define GP_CONNECTED 0x80
#define KEYBOARD_INPUT 0xEC20
#define GAMEPAD_INPUT 0xEC50
#define KEYBOARD_BYTES 32
#define GAMEPAD_COUNT 4
#define GP_BTN_START 0x08  // Start button in BTN1
#define GP_BTN_A 0x01      // A button in BTN0
#define GP_BTN_B 0x02      // B button in BTN0

extern uint8_t keystates[KEYBOARD_BYTES];
#define key(code) (keystates[code >> 3] & (1 << (code & 7)))

#define KEY_ESC 0x29
#define KEY_ENTER 0x28  // ENTER key to start game

extern gamepad_t gamepad[GAMEPAD_COUNT];

void show_title_screen(void)
{
    const uint8_t red_color = 0x03;      // Pure red
    const uint8_t blue_color = 0x1F;     // Blue
    const uint16_t center_x = 110;       // X position for centered text
    
    // Clear any remaining bullets from previous game
    init_sbullets();
    
    // Start title music
    start_title_music();
    
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
    draw_text(center_x - 30, 130, "PUSH A+B TO EXIT", blue_color);
    
    printf("Title screen displayed. Press START to begin...\n");
    
    // Title screen loop - wait for START button
    bool start_button_was_pressed = false;  // Track button state for edge detection
    while (true) {
        // Wait for vertical sync
        if (RIA.vsync == vsync_last)
            continue;
        vsync_last = RIA.vsync;
        
        // Update music
        update_music();
        
        // Read input
        RIA.addr0 = KEYBOARD_INPUT;
        RIA.step0 = 1;
        for (uint8_t i = 0; i < KEYBOARD_BYTES; i++) {
            keystates[i] = RIA.rw0;
        }
        
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
        
        // Check for keyboard ENTER or gamepad START button to start game
        bool start_pressed = false;
        
        // Check keyboard ENTER
        if (key(KEY_ENTER)) {
            start_pressed = true;
        }
        
        // Check gamepad START button (BTN1 bit 0x08)
        if (gamepad[0].dpad & GP_CONNECTED) {
            if (gamepad[0].btn1 & GP_BTN_START) {
                start_pressed = true;
            }
        }
        
        // Handle start with edge detection
        if (start_pressed) {
            if (!start_button_was_pressed) {
                // This is a new press (edge detection)
                start_button_was_pressed = true;
                // Stop music
                stop_music();
                // Clear entire screen before exiting
                RIA.addr0 = 0;
                RIA.step0 = 1;
                for (unsigned i = vlen; i--;) {
                    RIA.rw0 = 0;
                }
                printf("START/ENTER pressed - beginning game!\n");
                
                // Wait for button/key to be released before exiting
                while (true) {
                    if (RIA.vsync == vsync_last)
                        continue;
                    vsync_last = RIA.vsync;
                    
                    // Read keyboard
                    RIA.addr0 = KEYBOARD_INPUT;
                    RIA.step0 = 1;
                    for (uint8_t i = 0; i < KEYBOARD_BYTES; i++) {
                        keystates[i] = RIA.rw0;
                    }
                    
                    // Read gamepad
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
                    
                    // Exit loop when both ENTER and START are released
                    if (!key(KEY_ENTER) && !(gamepad[0].btn1 & GP_BTN_START)) {
                        break;
                    }
                }
                
                return;  // Exit title screen
            }
        } else {
            // Button/key is not pressed
            start_button_was_pressed = false;
        }
        
        // Check for A+B buttons pressed together to exit
        if ((gamepad[0].btn0 & GP_BTN_A) && (gamepad[0].btn0 & GP_BTN_B)) {
            printf("A+B pressed - exiting...\n");
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
