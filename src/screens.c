#include "screens.h"
#include <rp6502.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "usb_hid_keys.h"

// External references
extern void draw_text(int16_t x, int16_t y, const char* text, uint8_t color);
extern void clear_rect(int16_t x, int16_t y, int16_t width, int16_t height);
extern void move_fighters_offscreen(void);
extern void move_ebullets_offscreen(void);
extern void reset_player_position(void);
extern int8_t check_high_score(int16_t score);
extern void get_player_initials(char* initials);
extern void insert_high_score(int8_t position, const char* initials, int16_t score);
extern void save_high_scores(void);
extern void start_end_music(void);
extern void update_music(void);
extern void stop_music(void);

extern int16_t game_level;
extern int16_t game_score;
extern const uint16_t vlen;

// Bullet structure
typedef struct {
    int16_t x, y;
    int16_t vx, vy;
    int16_t status;
} Bullet;

extern Bullet bullets[];

// Gamepad structure
typedef struct {
    uint8_t dpad;
    uint8_t sticks;
    uint8_t btn0;
    uint8_t btn1;
    int8_t lx;
    int8_t ly;
    int8_t rx;
    int8_t ry;
    uint8_t l2;
    uint8_t r2;
} gamepad_t;

#define MAX_BULLETS 8
#define GAMEPAD_COUNT 4
#define KEYBOARD_INPUT 0xEC20
#define GAMEPAD_INPUT 0xEC50
#define KEYBOARD_BYTES 32

extern gamepad_t gamepad[GAMEPAD_COUNT];
extern uint8_t keystates[KEYBOARD_BYTES];

// Key macro
#define key(code) (keystates[code >> 3] & (1 << (code & 7)))

// Sprite configuration addresses
extern unsigned BULLET_CONFIG;

/**
 * Display level up message and wait for START button
 */
void show_level_up(void)
{
    const uint8_t blue_color = 0x1F;
    const uint8_t white_color = 0xFF;
    const uint16_t center_x = 120;
    const uint16_t center_y = 80;
    
    // Button definitions
    #define GP_BTN_START 0x08  // START button in BTN1
    #define KEY_ENTER 0x28     // ENTER key
    
    // Draw "LEVEL UP" message
    draw_text(center_x, center_y, "LEVEL UP", blue_color);
    draw_text(center_x - 45, center_y + 15, "PRESS START TO CONTINUE", white_color);
    
    printf("\n*** LEVEL UP! Now on level %d ***\n", game_level);
    
    uint8_t vsync_last = RIA.vsync;
    bool start_pressed = false;
    
    // Wait for START button to be released first
    while (true) {
        if (RIA.vsync == vsync_last)
            continue;
        vsync_last = RIA.vsync;
        
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
        
        // Check if START or ENTER is released
        if (!(gamepad[0].btn1 & GP_BTN_START) && !key(KEY_ENTER)) {
            break;
        }
    }
    
    // Now wait for START or ENTER to be pressed
    while (true) {
        if (RIA.vsync == vsync_last)
            continue;
        vsync_last = RIA.vsync;
        
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
        
        // Check for START button or ENTER key
        bool start_now = (gamepad[0].btn1 & GP_BTN_START) || key(KEY_ENTER);
        
        if (start_now && !start_pressed) {
            break;  // START pressed, continue to next level
        }
        start_pressed = start_now;
    }
    
    // Wait for START button to be released before exiting
    while (true) {
        if (RIA.vsync == vsync_last)
            continue;
        vsync_last = RIA.vsync;
        
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
        
        // Check if START and ENTER are both released
        if (!(gamepad[0].btn1 & GP_BTN_START) && !key(KEY_ENTER)) {
            break;  // Button released, safe to exit
        }
    }
    
    // Clear the message
    clear_rect(center_x - 45, center_y, 150, 25);
}

/**
 * Display game over screen and wait for fire button
 */
void show_game_over(void)
{
    const uint8_t red_color = 0x03;
    const uint16_t center_x = 100;
    
    // Start end music
    start_end_music();
    
    // Move all active fighters offscreen
    move_fighters_offscreen();
    
    // Move all bullets offscreen
    for (uint8_t i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].status >= 0) {
            unsigned ptr = BULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            bullets[i].status = -1;
        }
    }
    
    // Move all enemy bullets offscreen
    move_ebullets_offscreen();
    
    // Reset player position to center
    reset_player_position();
    
    // Check if player got a high score
    int8_t high_score_pos = check_high_score(game_score);
    if (high_score_pos >= 0) {
        // Get player initials
        char initials[4];
        get_player_initials(initials);
        
        // Insert into high score table
        insert_high_score(high_score_pos, initials, game_score);
        
        // Save to file
        save_high_scores();
    }
    
    // Draw "GAME OVER" message
    draw_text(center_x, 70, "GAME OVER", red_color);
    draw_text(center_x - 30, 90, "HIT FIRE TO CONTINUE", red_color);
    
    printf("\n*** GAME OVER ***\n");
    printf("Final Level: %d\n", game_level);
    printf("Final Score: %d\n", game_score);
    
    uint8_t vsync_last = RIA.vsync;
    bool fire_button_released = false;
    
    // Wait for fire button
    while (true) {
        if (RIA.vsync == vsync_last)
            continue;
        vsync_last = RIA.vsync;
        
        // Update music
        update_music();
        
        // Read gamepad input
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
        
        // Read keyboard
        RIA.addr0 = KEYBOARD_INPUT;
        RIA.step0 = 1;
        for (uint8_t i = 0; i < KEYBOARD_BYTES; i++) {
            keystates[i] = RIA.rw0;
        }
        
        // Check if fire button is released first
        bool fire_pressed = key(KEY_SPACE) ||
                           (gamepad[0].btn0 & 0x04) ||  // A
                           (gamepad[0].btn0 & 0x02) ||  // B
                           (gamepad[0].btn0 & 0x20);    // C
        
        if (!fire_pressed) {
            fire_button_released = true;
        } else if (fire_button_released) {
            // Fire button pressed after being released
            printf("Fire button pressed - continuing...\n");
            
            // Stop end music before returning to title screen
            stop_music();
            
            // Clear screen before returning to title screen
            RIA.addr0 = 0;
            RIA.step0 = 1;
            for (unsigned i = vlen; i--;) {
                RIA.rw0 = 0;
            }
            
            return;
        }
        
        // Check for ESC to exit
        if (key(KEY_ESC)) {
            printf("ESC pressed - exiting...\n");
            exit(0);
        }
    }
}
