/*
 * highscore.c - High score system implementation for RPMegaFighter
 */

#include "highscore.h"
#include "constants.h"
#include "input.h"
#include "usb_hid_keys.h"
#include "music.h"
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
    const uint8_t color_cycle[] = {0xE3, 0x1F, 0xFF, 0xF8, 0x3F, 0x07, 0xC7}; // yellow, blue, white, red, green, cyan, magenta
    const uint8_t color_cycle_len = sizeof(color_cycle) / sizeof(color_cycle[0]);
    const uint16_t start_x = 210;
    const uint16_t start_y = 40;

    // Animate color based on vsync/frame (arcade effect)
    uint8_t frame = RIA.vsync;

    // Draw title with animated color
    uint8_t title_color = color_cycle[(frame / 8) % color_cycle_len];
    draw_text(start_x, start_y, "HIGH SCORES", title_color);

    // Draw each score with animated color cycling
    for (uint8_t i = 0; i < MAX_HIGH_SCORES; i++) {
        uint16_t y = start_y + 15 + (i * 8);
        uint8_t row_color = color_cycle[(frame / 8 + i) % color_cycle_len];

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
        draw_text(start_x, y, rank, row_color);

        // Draw name
        draw_text(start_x + 10, y, high_scores[i].name, row_color);

        // Draw score (5 digits)
        char score_buf[6];
        score_buf[0] = '0' + (high_scores[i].score / 10000) % 10;
        score_buf[1] = '0' + (high_scores[i].score / 1000) % 10;
        score_buf[2] = '0' + (high_scores[i].score / 100) % 10;
        score_buf[3] = '0' + (high_scores[i].score / 10) % 10;
        score_buf[4] = '0' + high_scores[i].score % 10;
        score_buf[5] = '\0';
        draw_text(start_x + 30, y, score_buf, row_color);
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
    
    // Default Name
    name[0] = 'A';
    name[1] = 'A';
    name[2] = 'A';
    name[3] = '\0';
    
    uint8_t current_char = 0;   // Editing index 0-2
    uint8_t vsync_last = RIA.vsync;
    
    // Input Edge Detection States
    bool up_was_pressed = false;
    bool down_was_pressed = false;
    bool fire_was_pressed = false;
    
    // Visual States
    uint8_t blink_counter = 0;
    bool blink_state = false;
    
    // Draw UI
    draw_text(center_x - 20, center_y - 15, "NEW HIGH SCORE!", yellow_color);
    draw_text(center_x - 20, center_y, "ENTER INITIALS:", yellow_color);
    
    printf("\nNEW HIGH SCORE! Enter your initials\n");
    
    // ---------------------------------------------------------
    // PHASE 1: Wait for Release (Debounce)
    // Wait until FIRE is NOT pressed to prevent accidental input
    // ---------------------------------------------------------
    while (true) {
        if (RIA.vsync == vsync_last)
            continue;
        vsync_last = RIA.vsync;
        
        handle_input();
        update_music();
        
        // Check if FIRE is released
        if (!is_action_pressed(0, ACTION_FIRE)) {
            break;
        }
    }
    
    // ---------------------------------------------------------
    // PHASE 2: Entry Loop
    // ---------------------------------------------------------
    while (current_char < 3) {
        if (RIA.vsync == vsync_last)
            continue;
        vsync_last = RIA.vsync;
        
        handle_input();
        update_music();
        
        // --- VISUALS ---
        
        // Update blink counter (0.1s = 6 frames at 60 FPS)
        blink_counter++;
        if (blink_counter >= 6) {
            blink_counter = 0;
            blink_state = !blink_state;
        }
        
        // Clear the name area to prevent artifacts
        clear_rect(center_x + 10, center_y + 15, 32, 12);
        
        // Draw the 3 characters
        for (uint8_t i = 0; i < 3; i++) {
            char letter[2] = {name[i], '\0'};
            uint8_t color = yellow_color;
            
            // Only the active character blinks
            if (i == current_char && blink_state) {
                color = white_color;
            }
            
            draw_text(center_x + 10 + (i * 8), center_y + 15, letter, color);
        }
        
        // Draw Underscore under current char
        char underscore[2] = "_";
        draw_text(center_x + 10 + (current_char * 8), center_y + 20, underscore, yellow_color);
        
        // --- INPUT HANDLING ---
        
        // 1. UP Input (Thrust Action)
        bool up_now = is_action_pressed(0, ACTION_THRUST);
        if (up_now && !up_was_pressed) {
            name[current_char]++;
            if (name[current_char] > 'Z') name[current_char] = 'A';
        }
        up_was_pressed = up_now;
        
        // 2. DOWN Input (Reverse Thrust Action)
        bool down_now = is_action_pressed(0, ACTION_REVERSE_THRUST);
        if (down_now && !down_was_pressed) {
            name[current_char]--;
            if (name[current_char] < 'A') name[current_char] = 'Z';
        }
        down_was_pressed = down_now;
        
        // 3. CONFIRM Input (Fire Action)
        bool fire_now = is_action_pressed(0, ACTION_FIRE);
        if (fire_now && !fire_was_pressed) {
            // Advance to next character
            current_char++;
        }
        fire_was_pressed = fire_now;
    }
    
    printf("Initials entered: %s\n", name);
    
    // Clear the entry screen area before returning
    clear_rect(center_x - 20, center_y - 15, 130, 40);
}
