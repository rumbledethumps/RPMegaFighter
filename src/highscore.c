/*
 * highscore.c - High score system implementation for RPMegaFighter
 */

#include "highscore.h"
#include "input.h"
#include "usb_hid_keys.h"
#include <stdio.h>
#include <string.h>
#include <rp6502.h>

// Forward declarations for graphics functions
extern void draw_text(int16_t x, int16_t y, const char* text, uint8_t color);
extern void clear_rect(int16_t x, int16_t y, int16_t width, int16_t height);

// High score data
static HighScore high_scores[MAX_HIGH_SCORES];

// External references from main game
extern gamepad_t gamepad[GAMEPAD_COUNT];

/**
 * Initialize high scores with default values
 */
void init_high_scores(void)
{
    for (uint8_t i = 0; i < MAX_HIGH_SCORES; i++) {
        high_scores[i].name[0] = 'A';
        high_scores[i].name[1] = 'A';
        high_scores[i].name[2] = 'A';
        high_scores[i].name[3] = '\0';
        high_scores[i].score = (MAX_HIGH_SCORES - i) * 10;  // 100, 90, 80, ...
    }
}

/**
 * Load high scores from file
 * Returns true if loaded successfully, false if file doesn't exist
 */
bool load_high_scores(void)
{
    FILE* fp = fopen(HIGH_SCORE_FILE, "rb");
    if (!fp) {
        printf("High score file not found, initializing defaults\n");
        init_high_scores();
        return false;
    }
    
    size_t read = fread(high_scores, sizeof(HighScore), MAX_HIGH_SCORES, fp);
    fclose(fp);
    
    if (read != MAX_HIGH_SCORES) {
        printf("Error reading high scores, initializing defaults\n");
        init_high_scores();
        return false;
    }
    
    printf("Loaded %d high scores from file\n", MAX_HIGH_SCORES);
    return true;
}

/**
 * Save high scores to file
 */
void save_high_scores(void)
{
    FILE* fp = fopen(HIGH_SCORE_FILE, "wb");
    if (!fp) {
        printf("Error: Could not open high score file for writing\n");
        return;
    }
    
    fwrite(high_scores, sizeof(HighScore), MAX_HIGH_SCORES, fp);
    fclose(fp);
    printf("High scores saved\n");
}

/**
 * Check if score qualifies for high score list
 * Returns the position (0-9) if it qualifies, -1 otherwise
 */
int8_t check_high_score(int16_t score)
{
    for (uint8_t i = 0; i < MAX_HIGH_SCORES; i++) {
        if (score > high_scores[i].score) {
            return i;
        }
    }
    return -1;
}

/**
 * Insert a new high score at the given position
 * Shifts lower scores down
 */
void insert_high_score(int8_t position, const char* name, int16_t score)
{
    if (position < 0 || position >= MAX_HIGH_SCORES) return;
    
    // Shift scores down
    for (int8_t i = MAX_HIGH_SCORES - 1; i > position; i--) {
        high_scores[i] = high_scores[i - 1];
    }
    
    // Insert new score
    strncpy(high_scores[position].name, name, HIGH_SCORE_NAME_LEN);
    high_scores[position].name[HIGH_SCORE_NAME_LEN] = '\0';
    high_scores[position].score = score;
}

/**
 * Display high scores on screen
 */
void draw_high_scores(void)
{
    const uint8_t yellow_color = 0xE3;  // Yellow
    const uint8_t white_color = 0xFF;
    const uint16_t start_x = 210;
    const uint16_t start_y = 40;
    
    // Draw title
    draw_text(start_x, start_y, "HIGH SCORES", yellow_color);
    
    // Draw each score
    for (uint8_t i = 0; i < MAX_HIGH_SCORES; i++) {
        uint16_t y = start_y + 15 + (i * 8);
        
        // Draw rank number
        char rank[3];
        if (i == 9) {
            rank[0] = '1';
            rank[1] = '0';
            rank[2] = '\0';
        } else {
            rank[0] = '1' + i;
            rank[1] = '\0';
        }
        draw_text(start_x, y, rank, white_color);
        
        // Draw name
        draw_text(start_x + 10, y, high_scores[i].name, white_color);
        
        // Draw score (5 digits)
        char score_buf[6];
        score_buf[0] = '0' + (high_scores[i].score / 10000) % 10;
        score_buf[1] = '0' + (high_scores[i].score / 1000) % 10;
        score_buf[2] = '0' + (high_scores[i].score / 100) % 10;
        score_buf[3] = '0' + (high_scores[i].score / 10) % 10;
        score_buf[4] = '0' + high_scores[i].score % 10;
        score_buf[5] = '\0';
        draw_text(start_x + 30, y, score_buf, white_color);
    }
}

/**
 * Get player initials for high score entry
 */
void get_player_initials(char* name)
{
    const uint8_t yellow_color = 0xE3;
    const uint8_t white_color = 0xFF;
    const uint16_t center_x = 100;
    const uint16_t center_y = 100;
    
    name[0] = 'A';
    name[1] = 'A';
    name[2] = 'A';
    name[3] = '\0';
    
    uint8_t current_char = 0;  // Which character we're editing (0-2)
    uint8_t vsync_last = RIA.vsync;
    bool up_pressed = false;
    bool down_pressed = false;
    bool fire_pressed = false;
    uint8_t blink_counter = 0;  // Counter for blinking (0.1s = 6 frames at 60 FPS)
    bool blink_state = false;   // Current blink state
    
    draw_text(center_x - 20, center_y - 15, "NEW HIGH SCORE!", yellow_color);
    draw_text(center_x - 20, center_y, "ENTER INITIALS:", yellow_color);
    
    printf("\nNEW HIGH SCORE! Enter your initials\n");
    
    // Wait for all buttons to be released before starting
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
        
        // Check if all fire buttons are released
        bool any_button_pressed = key(KEY_SPACE) || 
                                  (gamepad[0].btn0 & 0x04) ||  // A
                                  (gamepad[0].btn0 & 0x02) ||  // B
                                  (gamepad[0].btn0 & 0x20);    // C
        
        if (!any_button_pressed) {
            break;  // All buttons released, can start entering initials
        }
    }
    
    while (current_char < 3) {
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
        
        // Update blink counter (0.1s = 6 frames at 60 FPS)
        blink_counter++;
        if (blink_counter >= 6) {
            blink_counter = 0;
            blink_state = !blink_state;
        }
        
        // Clear the name area before redrawing to avoid character overlap
        clear_rect(center_x + 10, center_y + 15, 16, 10);
        
        // Display current name with current character blinking
        for (uint8_t i = 0; i < 3; i++) {
            char letter[2] = {name[i], '\0'};
            uint8_t color;
            
            if (i == current_char) {
                // Current character blinks between yellow and white
                color = blink_state ? white_color : yellow_color;
            } else {
                // Other characters stay yellow
                color = yellow_color;
            }
            
            draw_text(center_x + 10 + (i * 4), center_y + 15, letter, color);
        }
        
        // Highlight current character with underscore
        char underscore[2] = "_";
        draw_text(center_x + 10 + (current_char * 4), center_y + 20, underscore, yellow_color);
        
        // Handle up/down to change letter (support both DPAD and stick inputs)
        bool up_now = key(KEY_UP) || 
                      (gamepad[0].dpad & GP_DPAD_UP) || 
                      (gamepad[0].sticks & GP_LSTICK_UP);
        bool down_now = key(KEY_DOWN) || 
                        (gamepad[0].dpad & GP_DPAD_DOWN) || 
                        (gamepad[0].sticks & GP_LSTICK_DOWN);
        
        if (up_now && !up_pressed) {
            name[current_char]++;
            if (name[current_char] > 'Z') name[current_char] = 'A';
        }
        up_pressed = up_now;
        
        if (down_now && !down_pressed) {
            name[current_char]--;
            if (name[current_char] < 'A') name[current_char] = 'Z';
        }
        down_pressed = down_now;
        
        // Handle fire to move to next character
        bool fire_now = key(KEY_SPACE) || 
                       (gamepad[0].btn0 & 0x04) ||  // A
                       (gamepad[0].btn0 & 0x02);    // B
        
        if (fire_now && !fire_pressed) {
            // Clear underscore
            clear_rect(center_x + 10 + (current_char * 4), center_y + 20, 4, 5);
            current_char++;
        }
        fire_pressed = fire_now;
    }
    
    printf("Initials entered: %s\n", name);
    
    // Clear the entry screen
    clear_rect(center_x - 20, center_y - 15, 130, 40);
}
