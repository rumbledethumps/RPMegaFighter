#include "hud.h"
#include "constants.h"
#include "screen.h"
#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>

// Access to text RAM pointer defined in main module
extern unsigned text_message_addr;

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

// Old pixel-based bar drawing functions removed — HUD now uses text-plane block bars.

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
    // static bool first_draw = true;
    // if (first_draw) {
    //     draw_text(5, hud_y, "YOU", text_color);
    //     draw_text(290, hud_y, "THEM", text_color);
    //     first_draw = false;
    // }
    
    // Update player score (3 digits) directly in text RAM
    char score_buf[4];
    score_buf[0] = '0' + (player_score / 100) % 10;
    score_buf[1] = '0' + (player_score / 10) % 10;
    score_buf[2] = '0' + player_score % 10;
    score_buf[3] = '\0';

    // Message layout matches main text: left_pad places player score at start
    const int MESSAGE_LENGTH_LOCAL = 36;
    const int seq_len = 3 + 1 + 8 + 1 + 5 + 1 + 8 + 1 + 3; // 31
    const int left_pad = (MESSAGE_LENGTH_LOCAL - seq_len) / 2; // 2
    const int player_index = left_pad; // player score begins here

    // Each character in text RAM is 3 bytes (char, palette/attr, extra)
    unsigned addr = text_message_addr + player_index * 3;
    RIA.addr0 = addr;
    RIA.step0 = 1;

    for (uint8_t k = 0; k < 3; ++k) {
        RIA.rw0 = score_buf[k];
        RIA.rw0 = 0xE0; // normal text attribute
        RIA.rw0 = 0x00; // extra/unused
    }
    
    // Draw BAR1 as text-plane blocks (8 chars). Grey when 0, fill with color to 100.
    const int block_chars = 8;
    const int block1_start = player_index + 3 + 1; // after player(3) + space
    int filled1 = (player_score * block_chars) / SCORE_TO_WIN;
    if (filled1 < 0) filled1 = 0;
    if (filled1 > block_chars) filled1 = block_chars;

    unsigned b1_addr = text_message_addr + block1_start * 3;
    RIA.addr0 = b1_addr;
    RIA.step0 = 1;
    for (int i = 0; i < block_chars; ++i) {
        RIA.rw0 = 0xDB; // block glyph
        if (i < filled1) RIA.rw0 = BLOCK1_ATTR; else RIA.rw0 = BLOCK_EMPTY_ATTR;
        RIA.rw0 = 0x10;
    }
    
    // Update game score (5 digits) directly in text RAM
    char game_score_buf[6];
    game_score_buf[0] = '0' + (game_score / 10000) % 10;
    game_score_buf[1] = '0' + (game_score / 1000) % 10;
    game_score_buf[2] = '0' + (game_score / 100) % 10;
    game_score_buf[3] = '0' + (game_score / 10) % 10;
    game_score_buf[4] = '0' + game_score % 10;
    game_score_buf[5] = '\0';

    const int game_index = player_index + 3 + 1 + 8 + 1; // left_pad + 13

    unsigned game_addr = text_message_addr + game_index * 3;
    RIA.addr0 = game_addr;
    RIA.step0 = 1;
    for (uint8_t k = 0; k < 5; ++k) {
        RIA.rw0 = game_score_buf[k];
        RIA.rw0 = 0xE0;
        RIA.rw0 = 0x00;
    }
    
    // Draw BAR2 as text-plane blocks (8 chars), filled right-to-left.
    const int block2_start = player_index + 3 + 1 + 8 + 1 + 5 + 1; // after player, space, block1, space, game, space
    int filled2 = (enemy_score * block_chars) / SCORE_TO_WIN;
    if (filled2 < 0) filled2 = 0;
    if (filled2 > block_chars) filled2 = block_chars;

    unsigned b2_addr = text_message_addr + block2_start * 3;
    RIA.addr0 = b2_addr;
    RIA.step0 = 1;
    for (int i = 0; i < block_chars; ++i) {
        RIA.rw0 = 0xDB; // block glyph
        // fill from right: positions >= (block_chars - filled2) are filled
        if (i >= (block_chars - filled2)) RIA.rw0 = BLOCK2_ATTR; else RIA.rw0 = BLOCK_EMPTY_ATTR;
        RIA.rw0 = 0x10;
    }
    
    // Update enemy score (3 digits) directly in text RAM
    score_buf[0] = '0' + (enemy_score / 100) % 10;
    score_buf[1] = '0' + (enemy_score / 10) % 10;
    score_buf[2] = '0' + enemy_score % 10;

    const int enemy_index = game_index + 5 + 1 + 8 + 1; // game_index + 15 -> left_pad + 28? (results in 30)
    unsigned enemy_addr = text_message_addr + enemy_index * 3;
    RIA.addr0 = enemy_addr;
    RIA.step0 = 1;
    for (uint8_t k = 0; k < 3; ++k) {
        RIA.rw0 = score_buf[k];
        RIA.rw0 = 0xE0;
        RIA.rw0 = 0x00;
    }
    
    // // Draw level indicator at bottom of screen
    // const uint16_t level_y = 170;
    
    // // Clear level area before drawing (wider to cover both text and number)
    // clear_rect(10, level_y, 50, 5);
    
    // draw_text(10, level_y, "LEVEL", text_color);
    // char level_buf[3];
    // level_buf[0] = '0' + (game_level / 10) % 10;
    // level_buf[1] = '0' + game_level % 10;
    // level_buf[2] = '\0';
    // draw_text(50, level_y, level_buf, text_color);
}
