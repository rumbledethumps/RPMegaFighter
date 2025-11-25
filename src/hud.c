#include "hud.h"
#include "constants.h"
#include "screen.h"
#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>

// External dependencies from main game
extern int16_t player_score;
extern int16_t enemy_score;
extern int16_t game_score;
extern int16_t game_level;

// Graphics functions from main file
extern void draw_text(int16_t x, int16_t y, const char* text, uint8_t color);
extern void clear_rect(int16_t x, int16_t y, int16_t width, int16_t height);

// Forward declare internal helper function
static inline void set(int16_t x, int16_t y, uint8_t color);

/**
 * Set a pixel at (x, y) to the given color
 */
static inline void set(int16_t x, int16_t y, uint8_t color)
{
    if (x >= 0 && x < 320 && y >= 0 && y < 180) {
        RIA.addr0 = x + (SCREEN_WIDTH * y);
        RIA.step0 = 1;
        RIA.rw0 = color;
    }
}

/**
 * Draw a horizontal bar (health/score meter) - fills left to right
 */
static void draw_bar(int16_t x, int16_t y, int16_t width, int16_t value, int16_t max_value, uint8_t fill_color, uint8_t bg_color)
{
    int16_t filled_width = (value * width) / max_value;
    if (filled_width > width) filled_width = width;
    
    // Draw filled portion
    for (int16_t i = 0; i < filled_width; i++) {
        for (int16_t j = 0; j < 5; j++) {
            set(x + i, y + j, fill_color);
        }
    }
    
    // Draw empty portion
    for (int16_t i = filled_width; i < width; i++) {
        for (int16_t j = 0; j < 5; j++) {
            set(x + i, y + j, bg_color);
        }
    }
}

/**
 * Draw a horizontal bar (health/score meter) - fills right to left
 */
static void draw_bar_rtl(int16_t x, int16_t y, int16_t width, int16_t value, int16_t max_value, uint8_t fill_color, uint8_t bg_color)
{
    int16_t filled_width = (value * width) / max_value;
    if (filled_width > width) filled_width = width;
    
    // Draw empty portion (on the left)
    for (int16_t i = 0; i < width - filled_width; i++) {
        for (int16_t j = 0; j < 5; j++) {
            set(x + i, y + j, bg_color);
        }
    }
    
    // Draw filled portion (on the right)
    for (int16_t i = width - filled_width; i < width; i++) {
        for (int16_t j = 0; j < 5; j++) {
            set(x + i, y + j, fill_color);
        }
    }
}

/**
 * Draw the HUD (score, health, etc.)
 */
void draw_hud(void)
{
    static int16_t prev_player_score = -1;
    static int16_t prev_enemy_score = -1;
    static int16_t prev_game_score = -1;
    static int16_t prev_game_level = -1;
    
    const uint8_t hud_y = 2;
    const uint8_t text_color = 0xFF;
    const uint8_t player_bar_color = 0x1F;  // Blue
    const uint8_t enemy_bar_color = 0x03;   // Pure red (RGB: R=3 bits, G=3 bits, B=2 bits)
    const uint8_t bar_bg_color = 0x08;      // Dark gray
    
    // Check if anything changed
    if (prev_player_score == player_score && 
        prev_enemy_score == enemy_score && 
        prev_game_score == game_score &&
        prev_game_level == game_level) {
        return;  // No changes, skip update
    }
    
    // Update previous values
    prev_player_score = player_score;
    prev_enemy_score = enemy_score;
    prev_game_score = game_score;
    prev_game_level = game_level;
    
    // Format with longer bars for better spacing
    
    // Draw static text (only on first draw)
    static bool first_draw = true;
    if (first_draw) {
        draw_text(5, hud_y, "YOU", text_color);
        draw_text(290, hud_y, "THEM", text_color);
        first_draw = false;
    }
    
    // Clear and draw player score (3 digits)
    clear_rect(30, hud_y, 12, 5);
    char score_buf[4];
    score_buf[0] = '0' + (player_score / 100) % 10;
    score_buf[1] = '0' + (player_score / 10) % 10;
    score_buf[2] = '0' + player_score % 10;
    score_buf[3] = '\0';
    draw_text(30, hud_y, score_buf, text_color);
    
    // Draw BAR1 (player progress bar) - longer bar (60 pixels)
    draw_bar(55, hud_y, 80, player_score, SCORE_TO_WIN, player_bar_color, bar_bg_color);
    
    // Clear and draw game score (5 digits)
    clear_rect(145, hud_y, 20, 5);
    char game_score_buf[6];
    game_score_buf[0] = '0' + (game_score / 10000) % 10;
    game_score_buf[1] = '0' + (game_score / 1000) % 10;
    game_score_buf[2] = '0' + (game_score / 100) % 10;
    game_score_buf[3] = '0' + (game_score / 10) % 10;
    game_score_buf[4] = '0' + game_score % 10;
    game_score_buf[5] = '\0';
    draw_text(145, hud_y, game_score_buf, text_color);
    
    // Draw BAR2 (enemy progress bar) - fills right to left
    draw_bar_rtl(175, hud_y, 80, enemy_score, SCORE_TO_WIN, enemy_bar_color, bar_bg_color);
    
    // Clear and draw enemy score (3 digits)
    clear_rect(270, hud_y, 12, 5);
    score_buf[0] = '0' + (enemy_score / 100) % 10;
    score_buf[1] = '0' + (enemy_score / 10) % 10;
    score_buf[2] = '0' + enemy_score % 10;
    draw_text(270, hud_y, score_buf, text_color);
    
    // Draw level indicator at bottom of screen
    const uint16_t level_y = 170;
    
    // Clear level area before drawing (wider to cover both text and number)
    clear_rect(10, level_y, 50, 5);
    
    draw_text(10, level_y, "LEVEL", text_color);
    char level_buf[3];
    level_buf[0] = '0' + (game_level / 10) % 10;
    level_buf[1] = '0' + game_level % 10;
    level_buf[2] = '\0';
    draw_text(50, level_y, level_buf, text_color);
}
